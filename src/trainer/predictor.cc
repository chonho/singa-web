#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

#include <thread>
#include <vector>
#include <map>
#include <chrono>
#include <glog/logging.h>
#include "utils/tinydir.h"
#include <unistd.h>
#include "utils/cluster.h"
#include "utils/common.h"
#include "proto/common.pb.h"
#include "trainer/trainer.h"
#include "trainer/predictor.h"
#include "mshadow/tensor.h"

namespace singa {
using std::vector;
using std::map;
using std::queue;
using namespace std::chrono;
using std::make_shared;

/***********************Predictor****************************/
Predictor::~Predictor() {
  // free Params (i.e., slices) in server shard
  for (auto entry : server_shard_)
    for (auto param : entry.second->shares)
      delete param;
  delete router_;
}

const vector<int> Predictor::SliceParams(const vector<Param*>& params) {
  // for load-balance among servers in a group and among server groups
  int nserver_grps = Cluster::Get()->nserver_groups();
  int nservers_per_grp = Cluster::Get()->nservers_per_group();
  int lcm = LeastCommonMultiple(nserver_grps, nservers_per_grp);

  // collect sizes of unique Params
  std::vector<int> paramsize;
  for (auto param : params)
    if (param->id() == param->owner())
      paramsize.push_back(param->size());
  // slice into lcm pieces to achieve good load-balance for both intra-group
  // partition (among servers in a group) and inter-group partition (each group
  // is assgined a sub-set of slices)
  auto param_slice = Slice(lcm, paramsize);
  // construct map from Param ID to its slices <slice id, len>
  std::unordered_map<int, vector<std::pair<int, int>>> paramid2slices;
  vector<int> slices;
  auto it = param_slice.begin();
  int slice_id = 0;
  for (auto param : params) {
    if (param->id() == param->owner()) {
      for (int len : *it) {
        slices.push_back(len);
        paramid2slices[param->id()].push_back(std::make_pair(slice_id++, len));
      }
      it++;
    }
  }
  // add slice info for every Param
  for (auto param : params)
    for (auto entry : paramid2slices[param->owner()]) {
      param->AddSlice(entry.first, entry.second);
      LOG(INFO) << "param id " << param->id() << " owner=" << param->owner()
        << ": " << entry.first << ", " << entry.second;
    }
  return slices;
}

void Predictor::SetupClassifier(
    const JobProto& job_conf,
    const vector<Classifier*>& classifiers) {

  const auto& net_conf = job_conf.neuralnet();
  std::unordered_map<int, shared_ptr<NeuralNet>> grp_net;

  for (auto classifier : classifiers) {
    auto net = NeuralNet::Create(net_conf, kTrain, 1); // 1: num of partitions
    int grp_id = classifier->grp_id();
    shared_ptr<NeuralNet> test_net = nullptr, valid_net = nullptr;

    grp_net[grp_id] = net;

    classifier->Setup(job_conf, grp_net[grp_id], valid_net, test_net);

    LOG(ERROR) << "Classifier " << classifier->id() << " is ready";
  }
}

void Predictor::CreateClassifiers(int numClassifiers, const JobProto& job, vector<Classifier*>& classifiers) {
  int temp = 0;
  for (int cid = 0; cid < numClassifiers; cid++) {
      auto *classifier = Classifier::Create(job);
      classifier->Init(temp, temp, cid);
      classifiers.push_back(classifier);
  }
}

void Predictor::Resume(JobProto* jobConf) {
  tinydir_dir dir;
  string folder = Cluster::Get()->checkpoint_folder();
  tinydir_open(&dir, folder.c_str());
  int latest_step = 0;
  // there would be multi checkpoint files (from diff workers) for one step
  vector<string> ck_files;
  // iterate all files to get the files for the last checkpoint
  while (dir.has_next) {
    tinydir_file file;
    tinydir_readfile(&dir, &file);
    tinydir_next(&dir);
    char* ch = strstr(file.name, "step");
    if (ch == nullptr) {
      if (file.name[0] != '.')
        LOG(INFO) << "Irregular file in checkpoint folder: " << file.name;
      continue;
    }

    LOG(INFO) << "Add checkpoint file for resume: " << ch;
    int step = atoi(ch+4);
    if (step == latest_step) {
      ck_files.push_back(file.name);
    } else if(step > latest_step) {
      latest_step = step;
      ck_files.clear();
      ck_files.push_back(string(file.name));
    }
  }

  if (latest_step > 0) {
    jobConf->set_step(latest_step);
    if (!jobConf->has_reset_param_version())
      jobConf->set_reset_param_version(false);
    jobConf->clear_checkpoint_path();
    for (auto ck_file : ck_files)
      jobConf->add_checkpoint_path(folder + "/" + ck_file);
  }
  tinydir_close(&dir);
}

void Predictor::Start(bool resume, const SingaProto& singaConf, JobProto* job, vector<Classifier*>* classifiers, int numClassifiers) {
  // register job to zookeeper at the beginning
  auto cluster = Cluster::Setup(job->id(), singaConf, job->cluster());
  if (resume)
    Resume(job);

  router_ = new Router();
  router_->Bind(kInprocRouterEndpoint);
  const string hostip = cluster->hostip();
  int port = router_->Bind("tcp://" + hostip + ":*");
  // register endpoint to zookeeper
  cluster->Register(getpid(), hostip + ":" + std::to_string(port));

  CreateClassifiers(numClassifiers, *job, *classifiers);

  SetupClassifier(*job, *classifiers);

//#ifdef USE_MPI
//  for (int i = 0; i < nthreads; i++)
//    MPIQueues.push_back(make_shared<SafeQueue>());
//#endif
}

inline int bandwidth(int bytes, system_clock::time_point start) {
  auto now=system_clock::now();
  auto duration=duration_cast<std::chrono::milliseconds> (now - start);
  return static_cast<int>(bytes*1000.f/duration.count());
}

void Predictor::Run(
    const vector<Worker*>& workers,
    const vector<Server*>& servers) {
  int nworkers = workers.size(), nservers = servers.size();
  auto cluster = Cluster::Get();
  procs_id_ = cluster->procs_id();
  LOG(INFO) << "Stub in process " << procs_id_ << " starts";

  // for sync among server groups
  auto start = std::chrono::system_clock::now();
  float trans_size = 0.f;  // total size of msg transferred since start time
  int sync_server_id = 0;
  int max_bandwidth = cluster->bandwidth();
  int nserver_grps = cluster->nserver_groups();

  map<int, Dealer*> inter_dealers;  // for sending msg to other procs

  std::queue<Msg*> msg_queue;
  Poller poll(router_);
  bool stop=false;
  while (!stop || !msg_queue.empty()) {
    if (msg_queue.empty()) {
      // if the poll time is large, then the poller may not expire
      // if it is small, then many reminder messages will be sent which may
      // slow done the process of other request. TODO tune it.
      auto *sock = poll.Wait(cluster->poll_time());
      if (poll.Terminated()) {
        LOG(ERROR) << "Connection broken!";
        exit(0);
      } else if (sock == nullptr) {
        if (nserver_grps > 1 && bandwidth(trans_size, start) < max_bandwidth) {
          Msg* msg = GenSyncReminderMsg(sync_server_id, servers);
          router_->Send(&msg) ;
          sync_server_id = (sync_server_id + 1) % nservers;
        }
        continue;
      }
      Msg* msg = router_->Receive();
      msg_queue.push(msg);
    }
    Msg* msg = msg_queue.front();
    msg_queue.pop();
    int type = msg->type(), dst = msg->dst(), flag = AddrType(dst);
    if (flag == kStub && (AddrProc(dst) == procs_id_ || AddrGrp(dst) == -1)) {
      if (type == kConnect) {
        DeleteMsg(&msg);
      } else if (type == kMetric) {
        DisplayMetric(&msg);
      } else if (type == kStop) {
        int src_flag = AddrType(msg->src());
        if (src_flag == kServer) nservers--;
        else if (src_flag == kWorkerParam) nworkers--;
        DeleteMsg(&msg);
        if (nworkers == 0 && nservers == 0) break;
      } else if (nserver_grps > 0) {
        HandleLocalMsg(&msg_queue, &msg);
      } else {
        DeleteMsg(&msg);
      }
    } else {
      int dst_procs = AddrProc(dst);
      if (flag != kStub)
        dst_procs = cluster->ProcsIDOf(AddrGrp(dst), AddrID(dst), flag);
      if (dst_procs != procs_id_) {
        if (bandwidth(trans_size, start) <= cluster->bandwidth()) {
          start = std::chrono::system_clock::now();
          trans_size = 0;
        }
        trans_size += msg->size();

        if (inter_dealers.find(dst_procs) == inter_dealers.end())
          inter_dealers[dst_procs] = CreateInterProcsDealer(dst_procs);
        inter_dealers[dst_procs]->Send(&msg);
      } else {
        if (type == kSyncRequest)
          msg->AddFormatFrame("i", max_bandwidth - bandwidth(trans_size, start));
        router_->Send(&msg);
      }
    }
  }
  LOG(ERROR) << "Stub in process " << procs_id_ << " stops";
  for (auto& entry : inter_dealers)
    delete entry.second;
}

Msg* Predictor::GenSyncReminderMsg(int server, const vector<Server*>& servers ) {
  Msg* msg = new Msg();
  msg->set_src(Addr(-1,-1, kStub));
  msg->set_dst(Addr(servers[server]->grp_id(), servers[server]->id(), kServer));
  msg->set_type(kSyncReminder);
  return msg;
}

void Predictor::DisplayMetric(Msg** msg) {
  Msg* msgg = *msg;
  // only display metrics from the first group
  if (AddrGrp(msgg->src()) == 0) {
    int step = msgg->trgt_version();
    char prefix[128];
    msgg->ParseFormatFrame("s", prefix);
    CHECK(msgg->NextFrame());
    const string perf(static_cast<char*>(msgg->FrameData()), msgg->FrameSize());
    Metric cur(perf);
    LOG(ERROR) << prefix << " step-" << step <<", " << cur.ToLogString();
  }
  DeleteMsg(msg);
}

Dealer* Predictor::CreateInterProcsDealer(int dst_procs) {
  // forward to other procs
  auto cluster = Cluster::Get();
  auto dealer = new Dealer();
  while(cluster->endpoint(dst_procs)=="") {
    //kCollectSleepTime));
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    LOG(ERROR)<<"waiting for procs "<< dst_procs<<" to register";
  }
  dealer->Connect("tcp://"+cluster->endpoint(dst_procs));
  return dealer;
}

void Predictor::HandleLocalMsg(queue<Msg*>* msg_queue, Msg** msg) {
  Msg* msgg = *msg;
  int paramid = ParamID(msgg->trgt_val());
  int type = msgg->type();
  int grp;
  ParamEntry *entry = nullptr;
  switch (type) {  // TODO process other requests, e.g. RESTful
    case kUpdate:
      grp = AddrGrp(msgg->src());
      entry = worker_shard_.at(Hash(grp, paramid));
      for(auto update_msg : HandleUpdate(entry, msg))
        msg_queue->push(update_msg);
      break;
    case kRUpdate:
      grp = AddrGrp(msgg->dst());
      entry = worker_shard_.at(Hash(grp, paramid));
      HandleUpdateResponse(entry, msg);
      break;
    case kGet:
      grp = AddrGrp(msgg->src());
      entry = worker_shard_.at(Hash(grp, paramid));
      for(auto get_msg : HandleGet(entry, msg))
        msg_queue->push(get_msg);
      break;
    case kRGet:
      grp = AddrGrp(msgg->dst());
      entry = worker_shard_.at(Hash(grp, paramid));
      HandleGetResponse(entry, msg);
      break;
    case kPut:
      grp = AddrGrp(msgg->src());
      entry = worker_shard_.at(Hash(grp, paramid));
      for(auto put_msg : HandlePut(entry, msg))
        msg_queue->push(put_msg);
      break;
    default:
      LOG(ERROR)<<"Unknow message type:"<<type;
      break;
  }
}

void Predictor::GenMsgs(int type, int version, ParamEntry* entry,
    Msg* msg, vector<Msg*> *ret) {
  int src_grp = AddrGrp(msg->src());
  int dst_grp = src_grp / Cluster::Get()->nworker_groups_per_server_group();
  auto param=entry->shares.at(0);
  for (int idx = 0 ; idx < param->num_slices(); idx++) {
    int slice_id =param->slice_start() + idx;
    int server = slice2server_[slice_id];
    int procs = Cluster::Get()->ProcsIDOf(dst_grp, server, kServer);
    Msg* new_msg = nullptr;
    if (type == kPut) {
      CHECK_GT(entry->num_total, 0);
      new_msg = param->GenPutMsg(procs != procs_id_, idx);
      new_msg->AddFormatFrame("i", entry->num_total);
    } else if (type == kGet) {
      new_msg = param->GenGetMsg(procs != procs_id_, idx);
    } else if (type == kUpdate) {
      new_msg = param->GenUpdateMsg(procs != procs_id_, idx);
      new_msg->AddFormatFrame("i", entry->num_local);
    } else {
      LOG(FATAL) << "Wrong type";
    }
    new_msg->set_trgt(ParamTrgt(param->owner(), slice_id), version);
    new_msg->set_src(Addr(src_grp, procs_id_, kStub));
    new_msg->set_dst(Addr(dst_grp, server, kServer));
    ret->push_back(new_msg);
  }
}

const vector<Msg*> Predictor::HandleGet(ParamEntry* entry, Msg** msg) {
  vector<Msg*> ret;
  int version = (*msg)->trgt_version();
  if (version > entry->next_version) {
    entry->next_version = version;
    GenMsgs(kGet, version, entry, *msg, &ret);
  }
  DeleteMsg(msg);
  return ret;
}

const vector<Msg*> Predictor::HandleUpdate(ParamEntry *entry, Msg** msg) {
  vector<Msg*> ret;
  entry->num_update++;
  if (entry->num_update >= entry->num_local) {
    // average local gradient
    if (entry->num_local > 1) {
      auto it = entry->shares.begin();
      auto shape=mshadow::Shape1((*it)->size());
      mshadow::Tensor<mshadow::cpu,1> sum((*it)->mutable_cpu_grad(), shape);
      for (++it; it != entry->shares.end(); it++) {
        mshadow::Tensor<mshadow::cpu,1> grad((*it)->mutable_cpu_grad(), shape);
        sum += grad;
      }
      sum /= entry->num_total;
    }
    int step = (*msg)->trgt_version();
    GenMsgs(kUpdate, step, entry, *msg, &ret);
    entry->num_update = 0;
  }
  DeleteMsg(msg);
  return ret;
}

const vector<Msg*> Predictor::HandlePut(ParamEntry* entry, Msg** msg) {
  vector<Msg*> ret;
  int version = (*msg)->trgt_version();
  GenMsgs(kPut, version, entry, *msg, &ret);
  DeleteMsg(msg);
  return ret;
}

void Predictor::HandleGetResponse(ParamEntry* entry, Msg** msg) {
  int version = (*msg)->trgt_version();
  int sliceid = SliceID((*msg)->trgt_val());
  auto param = entry->shares.at(0);
  if (param->ParseGetResponseMsg(*msg, sliceid-param->slice_start()))
    param->set_version(version);
  DeleteMsg(msg);
}

void Predictor::HandleUpdateResponse(ParamEntry* entry, Msg** msg) {
  int version = (*msg)->trgt_version();
  int sliceid = SliceID((*msg)->trgt_val());
  auto param = entry->shares.at(0);
  if (param->ParseUpdateResponseMsg(*msg, sliceid-param->slice_start()))
    param->set_version(version);
  DeleteMsg(msg);
}
} /* singa */
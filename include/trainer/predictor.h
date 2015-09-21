#ifndef INCLUDE_TRAINER_PREDICTOR_H_
#define INCLUDE_TRAINER_PREDICTOR_H_
#include <unordered_map>
#include <queue>
#include "proto/job.pb.h"
#include "proto/singa.pb.h"
#include "utils/param.h"
#include "utils/singleton.h"
#include "utils/factory.h"
#include "neuralnet/neuralnet.h"
#include "trainer/classifier.h"
#include "trainer/server.h"
#include "communication/socket.h"

namespace singa {
/**
 * Every running process has a training object which launches one or more
 * worker (and server) threads.
 *
 * The main thread runs a loop to forward messages between workers and servers.
 */

class Predictor {
 public:
  ~Predictor();

  /**
   * Entrance function which construct the workers and servers, and luanch
   * one thread per worker/server.
   *
   * @param resume if true resume the training from the latest checkpoint files
   * @param singaConf global singa configuration including zookeeper and
   * @param jobConf job configuration, including cluster and model configuration
   */
  void Start(bool resume, const SingaProto& singaConf, JobProto* jobConf, vector<Classifier*>* classifiers, int numClassifiers);
  //CLEEvoid Start(bool resume, const SingaProto& singaConf, JobProto* jobConf);

 protected:
  /**
   * Setting the checkpoint field of model configuration to resume training.
   *
   * The checkpoint folder will be searched to get the files for the latest
   * checkpoint, which will be added into the checkpoint field. The workers
   * would then load the values of params from the checkpoint files.
   *
   * @param jobConf job configuration
   */
  void Resume(JobProto* jobConf);

  const vector<int> SliceParams(const vector<Param*>& params); 

  /**
   * Create workers instances.
   * @param nthread total num of threads in current procs which is used to
   * assign each thread a local thread ID. The number of workers is extracted
   * from Cluster
   * @param jobConf
   * @return worker instances
   */
  void CreateClassifiers(int numClassifiers, const JobProto& jobConf, vector<Classifier*>& classifiers);


  void SetupClassifier(
    const JobProto& jobConf,
    const vector<Classifier*>& classifiers);


  void Run(const vector<Worker*>& workers, const vector<Server*>& servers);
  /**
   * Generate msg to trigger synchronization with other server groups.
   *
   * @param server the local server index whom the message is sent to
   * @param servers all local servers
   * @return sync msg
   */
  Msg* GenSyncReminderMsg(int server, const vector<Server*>& servers);
  /**
   * Display metrics to log (standard output)
   */
  void DisplayMetric(Msg** msg);
  /**
   * Create a socket to send msg to the specified process
   * @param dst_procs the dst process (logical) ID
   * @return the newly created socket
   */
  Dealer* CreateInterProcsDealer(int dst_procs);
  /**
   * Handle messages to local servers and local stub
   */
  void HandleLocalMsg(std::queue<Msg*>* msg_queue, Msg** msg);

	/**
	 * Generate a request message to Get the parameter object.
	 */
	const vector<Msg*> HandleGet(ParamEntry* entry, Msg** msg);
	void HandleGetResponse(ParamEntry* entry, Msg** msg);

	/**
	 * Generate a request message to Update the parameter object.
	 */
	const vector<Msg*> HandleUpdate(ParamEntry* entry, Msg** msg);

  void HandleUpdateResponse(ParamEntry* entry, Msg** msg);

  /**
	 * Generate a request message to Put the parameter object.
	 */
	const vector<Msg*> HandlePut(ParamEntry* entry, Msg** msg);

  /**
   * Called by HandlePut, HandleUpdate and HandleGet functions
   * @param type message type
   * @param version param version
   * @param entry
   * @param msg
   * @param ret generated messages
   */
  void GenMsgs(int type, int version, ParamEntry* entry,
    Msg* msg, vector<Msg*> *ret);
  /**
   * Get a hash id for a Param object from a group.
   *
   * Simple multiple group_id with a large prime number 997 (assuming there are
   * no more than 997 worker groups) and plus owner param id.
   */
  inline int Hash(int grp_id, int param_id) {
    return grp_id * 997 + param_id;
  }


 protected:
  int procs_id_;
  Router *router_;
  std::unordered_map<int, ParamEntry*> worker_shard_;
  //!< map from slice ID to slice, used by servers and deleted in the destructor
  std::unordered_map<int, ParamEntry*> server_shard_;
  //!< map from slice to the server that updates it
  vector<int> slice2server_;
};
} /* singa */
#endif // INCLUDE_TRAINER_PREDICTOR_H_

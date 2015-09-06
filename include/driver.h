#ifndef SINGA_DRIVER_H_
#define SINGA_DRIVER_H_
#include "singa.h"

namespace singa {

class Driver {
 public:
  /**
   * Init SINGA, including init glog, parse job id and job conf from cmd line,
   * and register built-in layer, worker, updater, param subclasses.
   *
   * May be used for MPI init if it is used for message passing.
   */
  void Init(int argc, char** argv);
  /**
   * Register a Layer subclass.
   *
   * @param type layer type ID. If called to register built-in subclasses,
   * it is from LayerType; if called to register user-defined
   * subclass, it is a string;
   * @return 0 if success; otherwise -1.
   */
  template<typename Subclass, typename Type>
  int RegisterLayer(const Type& type);
  /**
   * Register an Updater subclass.
   *
   * @param type ID of the subclass. If called to register built-in subclasses,
   * it is from UpdaterType; if called to register user-defined
   * subclass, it is a string;
   * @return 0 if success; otherwise -1.
   */
  template<typename Subclass, typename Type>
  int RegisterUpdater(const Type& type);
  /**
   * Register a learning rate generator subclasses.
   *
   * @param type ID of the subclass. If called to register built-in subclasses,
   * it is from ChangeMethod; if called to register user-defined
   * subclass, it is a string;
   * @return 0 if success; otherwise -1.
   */
  template<typename Subclass, typename Type>
  int RegisterLRGenerator(const Type& type);
  /**
   * Register a Worker subclass.
   *
   * @param type ID of the subclass. If called to register built-in subclasses,
   * it is from TrainOneBatchAlg; if called to register user-defined
   * subclass, it is a string;
   * @return 0 if success; otherwise -1.
   */
  template<typename Subclass, typename Type>
  int RegisterWorker(const Type& type);
  
  // CLEE
  template<typename Subclass, typename Type>
  int RegisterClassifier(const Type& type);
  /**
   * Register a Param subclass.
   * @param type ID of the subclass. If called to register built-in subclasses,
   * it is from ParamType; if called to register user-defined
   * subclass, it is a string;
   *
   * @return 0 if success; otherwise -1.
   */
  template<typename Subclass, typename Type>
  int RegisterParam(const Type& type);
  /**
   * Register ParamGenerator subclasses for initalizing Param objects.
   *
   * @param type ID of the subclass. If called to register built-in subclasses,
   * it is from InitMethod; if called to register user-defined
   * subclass, it is a string;
   * @return 0 if success; otherwise -1.
   */
  template<typename Subclass, typename Type>
  int RegisterParamGenerator(const Type& type);

  /**
   * Submit the job configuration for starting the job.
   * @param resume resume from last checkpoint if true.
   * @param job job configuration
   */
  void Submit(bool resume, const JobProto& job);
  /**
   * @return job ID which is generated by zookeeper and passed in by the
   * launching script.
   */
  inline int job_id() const { return job_id_; }
  /**
   * @return job conf path which is passed by users at the command line. It
   * should at least contains the cluster configuration.
   */
  inline JobProto job_conf() const { return job_conf_; }

 private:
  int job_id_;
  int mode_; // CLEE
  int num_classifiers_; // CLEE
  JobProto job_conf_;
  SingaProto singa_conf_;
};

template<typename Subclass, typename Type>
int Driver::RegisterLayer(const Type& type) {
  auto factory = Singleton<Factory<singa::Layer>>::Instance();
  factory->Register(type, CreateInstance(Subclass, Layer));
  return 1;
}

template<typename Subclass, typename Type>
int Driver::RegisterParam(const Type& type) {
  auto factory = Singleton<Factory<singa::Param>>::Instance();
  factory->Register(type, CreateInstance(Subclass, Param));
  return 1;
}
template<typename Subclass, typename Type>
int Driver::RegisterParamGenerator(const Type& type) {
  auto factory = Singleton<Factory<singa::ParamGenerator>>::Instance();
  factory->Register(type, CreateInstance(Subclass, ParamGenerator));
  return 1;
}
template<typename Subclass, typename Type>
int Driver::RegisterUpdater(const Type& type) {
  auto factory = Singleton<Factory<singa::Updater>>::Instance();
  factory->Register(type, CreateInstance(Subclass, Updater));
  return 1;
}
template<typename Subclass, typename Type>
int Driver::RegisterLRGenerator(const Type& type) {
  auto factory = Singleton<Factory<singa::LRGenerator>>::Instance();
  factory->Register(type, CreateInstance(Subclass, LRGenerator));
  return 1;
}
template<typename Subclass, typename Type>
int Driver::RegisterWorker(const Type& type) {
  auto factory = Singleton<Factory<singa::Worker>>::Instance();
  factory->Register(type, CreateInstance(Subclass, Worker));
  return 1;
}
//CLEE
template<typename Subclass, typename Type>
int Driver::RegisterClassifier(const Type& type) {
  auto factory = Singleton<Factory<singa::Classifier>>::Instance();
  factory->Register(type, CreateInstance(Subclass, Classifier));
  return 1;
}


}  // namespace singa
#endif  // SINGA_DRIVER_H_



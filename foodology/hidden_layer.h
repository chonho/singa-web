#include "singa.h"
#include "neuralnet/layer.h"
#include "myproto.pb.h"

namespace singa {

class InputLayer: public DataLayer{
 public:
  using Layer::ComputeFeature;
  using Layer::setTestImage;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;

  //CLEE
  void setTestImage(string testImgName) override
       { testImgName_ = testImgName; }

 private:
  string testImgName_;
  DataShard* shard_;
};

class OutputLayer: public LossLayer {
  /*
   * connected from the label layer and the last fc layer
   */
 public:
  using Layer::ComputeFeature;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override {}
  /**
   * softmax is not recommendeded for partition because it requires the whole
   * src layer for normalization.
   */
  int partition_dim() const override {
    CHECK_LE(layer_proto_.partition_dim(), 1);
    return layer_proto_.partition_dim();
  }
  ConnectionType src_neuron_connection(int k) const override {
    // CHECK_LT(k, srclayers_.size());
    return kOneToAll;
  }

  void getOutputMessage(string& out) { out = outputMessage_; }

 private:
  int batchsize_;
  int dim_;
  float scale_;
  int topk_;
  string outputFilePath_;
  string outputMessage_;
};

}  // namespace singa


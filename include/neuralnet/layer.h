#ifndef SINGA_NEURALNET_LAYER_H_
#define SINGA_NEURALNET_LAYER_H_

#include <lmdb.h>

#include <vector>
#include <string>
#include <map>
#include <functional>
#include <utility>
#include <memory>
#include <chrono>
#include <random>

#include "proto/job.pb.h"
#include "utils/data_shard.h"
#include "neuralnet/base_layer.h"

/**
 * \file this file includes the declarations neuron layer classes that conduct
 * the transformation of features.
 */
namespace singa {

/**
 * Convolution layer.
 */
class ConvolutionLayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;
  const vector<Param*> GetParams() const override {
    vector<Param*> params{weight_, bias_};
    return params;
  }
  ConnectionType src_neuron_connection(int k) const  override {
    // CHECK_LT(k, srclayers_.size());
    return kOneToAll;
  }
  ~ConvolutionLayer();

 protected:
  int kernel_, pad_,  stride_;
  int batchsize_,  channels_, height_, width_;
  int col_height_, col_width_, conv_height_, conv_width_, num_filters_;
  Param* weight_, *bias_;
  Blob<float> col_data_, col_grad_;
};

class DropoutLayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;

 protected:
  // drop probability
  float pdrop_;
  /* record which neuron is dropped, required for back propagating gradients,
   * if mask[i]=0, then the i-th neuron is dropped.
   */
  Blob<float> mask_;
};

/**
 * RBM visible layer
 */
class RBMVisLayer: public RBMLayer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  ~RBMVisLayer();
  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;
  Blob<float>* Sample(int flat) override;

 private:
  RBMLayer* hid_layer_;
  Layer* input_layer_;
};
/**
 * RBM hidden layer
 */
class RBMHidLayer: public RBMLayer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  ~RBMHidLayer();
  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;
  Blob<float>* Sample(int flat) override;
 private:
  // whether use gaussian sampling
  bool gaussian_;
  RBMLayer *vis_layer_;
};
/**
  * fully connected layer
  */
class InnerProductLayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;

  ConnectionType src_neuron_connection(int k) const override {
    // CHECK_LT(k, srclayers_.size());
    return kOneToAll;
  }
  const vector<Param*> GetParams() const override {
    vector<Param*> params{weight_, bias_};
    return params;
  }
  ~InnerProductLayer();

 private:
  //! dimension of the hidden layer
  int hdim_;
  //! dimension of the visible layer
  int vdim_;
  int batchsize_;
  bool transpose_;
  Param* weight_, *bias_;
};

class LabelLayer: public ParserLayer {
 public:
  using ParserLayer::ParseRecords;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ParseRecords(int flag, const vector<Record>& records,
      Blob<float>* blob) override;
};

class LRNLayer: public Layer {
/**
 * Local Response Normalization edge
 * b_i=a_i/x_i^beta
 * x_i=knorm+alpha*\sum_{j=max(0,i-n/2}^{min(N,i+n/2}(a_j)^2
 * n is size of local response area.
 * a_i, the activation (after ReLU) of a neuron convolved with the i-th kernel.
 * b_i, the neuron after normalization, N is the total num of kernels
 */
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;

 protected:
  //! shape of the bottom layer feature
  int batchsize_, channels_, height_, width_;
  //! size local response (neighbor) area
  int lsize_;
  //! hyper-parameter
  float alpha_, beta_, knorm_;
  Blob<float> norm_;
};

class MnistLayer: public ParserLayer {
 public:
  using ParserLayer::ParseRecords;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ParseRecords(int flag, const vector<Record>& records,
      Blob<float>* blob) override;
  ConnectionType dst_layer_connection() const override {
    return kOneToMany;
  }
 protected:
  // height and width of the image after deformation
  // kernel size for elastic distortion
  // n^2 images are processed as a batch for elastic distortion
  // conv height and conv width
  // gauss kernel values, displacements, column image and tmp buffer
  // float* gauss_, *displacementx_, *displacementy_, *colimg_, *tmpimg_;
  float  gamma_, beta_, sigma_, kernel_, alpha_, norm_a_, norm_b_;
  int resize_, elastic_freq_;
};

class PoolingLayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;

 protected:
  int kernel_, pad_, stride_;
  int batchsize_, channels_, height_, width_, pooled_height_, pooled_width_;
  PoolingProto_PoolMethod pool_;
};

class ReLULayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions = 1) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;
};

class EuclideanLossLayer: public LossLayer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;


  int partition_dim() const override {
    CHECK_LE(layer_proto_.partition_dim(), 1);
    return layer_proto_.partition_dim();
  }
  ConnectionType src_neuron_connection(int k) const override {
    // CHECK_LT(k, srclayers_.size());
    return kOneToAll;
  }

 private:
  int batchsize_;
  int dim_;
};

class SoftmaxLossLayer: public LossLayer {
  /*
   * connected from the label layer and the last fc layer
   */
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;

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

 private:
  int batchsize_;
  int dim_;
  float scale_;
  int topk_;
};

class RGBImageLayer: public ParserLayer {
 public:
  using ParserLayer::ParseRecords;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ParseRecords(int flag, const vector<Record>& records,
      Blob<float>* blob) override;

 private:
  float scale_;
  int cropsize_;
  bool mirror_;
  Blob<float> mean_;
};

class ShardDataLayer: public DataLayer{
 public:
  using Layer::ComputeFeature;

  ~ShardDataLayer();
  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;

 private:
  DataShard* shard_;
};

/**
 * This layer apply Sigmoid function to neuron activations.
 * f(x)=1/(1+exp(-x))
 * f'(x)=f(x)*(1-f(x))
 */
class SigmoidLayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;
};

/**
 * This layer apply Tan function to neuron activations.
 * f(x)=A tanh(Bx)
 * f'(x)=B/A (A*A-f(x)*f(x))
 */
class TanhLayer: public Layer {
 public:
  using Layer::ComputeFeature;
  using Layer::ComputeGradient;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;
  void ComputeGradient(int flag) override;

 private:
  float outer_scale_, inner_scale_;
};


class InputLayer: public DataLayer{
 public:
  using Layer::ComputeFeature;
  using Layer::setTestImage;

  void Setup(const LayerProto& proto, int npartitions) override;
  void ComputeFeature(int flag, Metric *perf) override;

  //CLEE
  void setTestImage(string testImgName) override 
	{ testImgPath_ = inputDirPath_ + testImgName; }

 private:
  string inputDirPath_;
  string testImgPath_;
  DataShard* shard_;
};

/**
 * This is an output Layer to write result to disk
 */
 
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

  string getOutputMessage() { return outputMessage_; }

 private:
  int batchsize_;
  int dim_;
  float scale_;
  int topk_;
  string outputFilePath_;
  string outputMessage_;
};


}  // namespace singa

#endif  // SINGA_NEURALNET_LAYER_H_

#include <math.h>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include "hidden_layer.h"
#include "picojson.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <curl/curl.h>

#include "mshadow/tensor.h"
#include "mshadow/cxxnet_op.h"

using namespace mshadow;
using mshadow::cpu;
using mshadow::Shape1;
using mshadow::Shape2;

using std::map;
using namespace picojson;

namespace singa {

void InputLayer::Setup(const LayerProto& proto, int npartitions) {
  Layer::Setup(proto, npartitions);
  SingleLabelImageRecord *slir = sample_.mutable_image();
  slir->add_shape(3);
  slir->add_shape(32);
  slir->add_shape(32);
  // inputDirPath_ = proto.GetExtension(input_conf).path();
  batchsize_ = 1;

  records_.resize(1);
}


struct memoryStruct {
  char *memory;
  size_t size;
};

static void* CURL_realloc(void *ptr, size_t size) {
  /* There might be a realloc() out there that doesn't like reallocing
     NULL pointers, so we take care of it here */
  if(ptr)
    return realloc(ptr, size);
  else
    return malloc(size);
}

size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data) {
  size_t realsize = size * nmemb;
  struct memoryStruct *mem = (struct memoryStruct *)data;

  mem->memory = (char *)
		CURL_realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }
  return realsize;
}

const int kImageSize = 32;
const int kImageNBytes = 3*kImageSize*kImageSize;

void InputLayer::ComputeFeature(int flag, Metric* perf){

  // LOG(ERROR) << "InputLayer ComputeFeature";

  string hostname = "http://localhost:8889/" + testImgName_;

  CURL *curl;       // CURL objects
  CURLcode res;
  cv::Mat imgSrc; 	// image object
  memoryStruct buffer;  // memory buffer

  curl = curl_easy_init(); // init CURL library object/structure

  if(curl) {
    // set up the write to memory buffer
    buffer.memory = NULL;
    buffer.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, hostname.c_str());
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); // tell us what is happening

    // specify where to write the image (to a dynamic memory buffer)
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA, (void *) &buffer);

    res = curl_easy_perform(curl);  // get image from the URL

    // TODO check CV_8SC3 or CV_9UC3
    imgSrc = cv::imdecode(cv::Mat(1, buffer.size, CV_8SC3, buffer.memory), CV_LOAD_IMAGE_COLOR);

    curl_easy_cleanup(curl);
    //free(buffer.memory);
  }

  cv::Mat imgDst(kImageSize, kImageSize, imgDst.type());
  cv::resize(imgSrc, imgDst, imgDst.size(), cv::INTER_CUBIC);

  int idx=0;
  char imgDst_buffer[kImageNBytes];
  for (int y=0; y<imgDst.rows; y++) {
    for (int x=0; x<imgDst.cols; x++) {
      cv::Vec3b p = imgDst.at<cv::Vec3b>(x,y);  // Vec3b: G B R
      imgDst_buffer[idx] = p[2];
      imgDst_buffer[32*32+idx] = p[0];
      imgDst_buffer[2*32*32+idx] = p[1];
      idx++;
    }
  }
   
  int label = 0;

  singa::Record& record = records_.at(0);
  singa::SingleLabelImageRecord* image = record.mutable_image();;
  image->add_shape(3);
  image->add_shape(kImageSize);
  image->add_shape(kImageSize);

  image->set_label(label);
  image->set_pixel(imgDst_buffer, kImageNBytes);

}

void OutputLayer::Setup(const LayerProto& proto, int npartitions) {
  //LOG(ERROR) << "output layer setup";

  LossLayer::Setup(proto, npartitions);
  CHECK_EQ(srclayers_.size(),1);
  data_.Reshape(srclayers_[0]->data(this).shape());
  batchsize_=data_.shape()[0];
  dim_=data_.count()/batchsize_;
  topk_=proto.GetExtension(output_conf).topk();
  scale_=proto.GetExtension(output_conf).scale();

  outputFilePath_ = proto.GetExtension(output_conf).path();
  outputMessage_ = "";

}
void OutputLayer::ComputeFeature(int flag, Metric* perf) {

  //LOG(ERROR) << "output layer compute";
  outputMessage_ = "";

  Shape<2> s=Shape2(batchsize_, dim_);
  Tensor<cpu, 2> prob(data_.mutable_cpu_data(), s);
  Tensor<cpu, 2> src(srclayers_[0]->mutable_data(this)->mutable_cpu_data(), s);
  Softmax(prob, src);

  const float* probptr=prob.dptr;
  std::ofstream write(outputFilePath_);

  array adata; // for data value in json format

  for(int n=0;n<batchsize_;n++){
    vector<std::pair<float, int> > probvec;
    for (int j = 0; j < dim_; ++j) {
      probvec.push_back(std::make_pair(probptr[j], j));
    }
    std::partial_sort(
        probvec.begin(), probvec.begin() + topk_,
        probvec.end(), std::greater<std::pair<float, int> >());
    char str_buffer[24] = "-----\n";
    object o_data;

    for (auto it = probvec.begin(); it < probvec.begin() + topk_; it++) {
      float prob = it->first;
      int label = it->second;
      //char str_buffer[24];
      snprintf(str_buffer, 24, "prob %d:%.4f\n", label, prob);

      
      value lval(std::to_string(label));
      value pval((double)prob);
      object odata;
      odata.insert(map<string,value>::value_type("label", lval));
      odata.insert(map<string,value>::value_type("prob", pval));
      value oval(odata);
      adata.push_back(oval);

      // write to file
      write << string(str_buffer);
      //LOG(ERROR) << str_buffer;
    }
    write << string(str_buffer);
    //LOG(ERROR) << str_buffer;

    probptr+=dim_;
  }

  write.close();
  CHECK_EQ(probptr, prob.dptr+prob.shape.Size());

  object obj;
  value aval(adata);
  outputMessage_ = aval.serialize().c_str();

}

}  // namespace singa



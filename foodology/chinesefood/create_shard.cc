//
// This code creates training and test DataShard for CFood dataset.
// It is adapted from the convert_cFood_data from Caffe
//
// Usage:
//    create_shard.bin input_folder output_folder
//
// The CFood dataset could be downloaded at
//    http://www.cs.toronto.edu/~kriz/cFood.html
//

#include <fstream>
#include <string>

#include <glog/logging.h>
#include <cstdint>
#include <iostream>

#include "singa.h"

using std::string;

using singa::DataShard;
using singa::WriteProtoToBinaryFile;

const int kCFoodSize = 32;
const int kCFoodImageNBytes = 3072;
const int kCFoodBatchSize = 800;
const int kCFoodTrainBatches = 6;

void read_image(std::ifstream* file, int* label, char* buffer) {
  char label_char[2];

  file->read(label_char, 2);
  unsigned int a=(unsigned char)label_char[0],b=(unsigned char)label_char[1];
  *label = a*256+b;
//  std::cout<<a<<" "<<b<<" "<<*label<<"\t";
  file->read(buffer, kCFoodImageNBytes);
  return;
}

void create_shard(const string& input_folder, const string& output_folder) {
  int label;
  // Data buffer
  char str_buffer[kCFoodImageNBytes];
  singa::Record record;
  singa::SingleLabelImageRecord* image=record.mutable_image();;
  image->add_shape(3);
  image->add_shape(kCFoodSize);
  image->add_shape(kCFoodSize);

  singa::SingleLabelImageRecord mean;
  mean.CopyFrom(*image);
  for(int i=0;i<kCFoodImageNBytes;i++)
    mean.add_data(0.);

  DataShard train_shard(output_folder+"/cfood_train_shard",DataShard::kCreate);
  LOG(INFO) << "Writing Training data";
  int count=0;
  for (int fileid = 0; fileid < kCFoodTrainBatches; ++fileid) {
    // Open files
    LOG(INFO) << "Training Batch " << fileid + 1;
    snprintf(str_buffer, 128, "/data_batch_%d.bin", fileid+1);
    std::cout<< "training batch: "<< input_folder+str_buffer<<"\n";
    std::ifstream data_file((input_folder + str_buffer).c_str(),
        std::ios::in | std::ios::binary);
    CHECK(data_file) << "Unable to open train file #" << fileid+1;
    LOG(INFO) << "Writing Testing data";
    for (int itemid = 0; itemid < kCFoodBatchSize; ++itemid) {
      read_image(&data_file, &label, str_buffer);
      image->set_label(label);
      image->set_pixel(str_buffer, kCFoodImageNBytes);
      int length = snprintf(str_buffer, kCFoodImageNBytes, "%05d",
          fileid * kCFoodBatchSize + itemid);
      CHECK(train_shard.Insert(string(str_buffer, length), record));

      const string& pixels=image->pixel();
      for(int i=0;i<kCFoodImageNBytes;i++)
        mean.set_data(i, mean.data(i)+static_cast<uint8_t>(pixels[i]));
      count+=1;
    }
  }
  train_shard.Flush();
  for(int i=0;i<kCFoodImageNBytes;i++)
    mean.set_data(i, mean.data(i)/count);
  WriteProtoToBinaryFile(mean, (output_folder+"/image_mean.bin").c_str());

  LOG(INFO) << "Writing Testing data";
  DataShard test_shard(output_folder+"/cfood_test_shard",DataShard::kCreate);
  // Open files
  std::ifstream test_data_file((input_folder + "/test_batch.bin").c_str(),
      std::ios::in | std::ios::binary);
  CHECK(test_data_file) << "Unable to open test file.";
  for (int itemid = 0; itemid < kCFoodBatchSize; ++itemid) {
    read_image(&test_data_file, &label, str_buffer);
    image->set_label(label);
    image->set_pixel(str_buffer, kCFoodImageNBytes);
    int length = snprintf(str_buffer, kCFoodImageNBytes, "%05d", itemid);
    CHECK(test_shard.Insert(string(str_buffer, length), record));
  }
  test_shard.Flush();
}

int main(int argc, char** argv) {
  if (argc != 3) {
  std::cout<<"Create train and test DataShard for Cifar dataset.\n"
           <<"Usage:\n"
           <<"    create_shard.bin input_folder output_folder\n"
           <<"Where the input folder should contain the binary batch files.\n";
  } else {
    google::InitGoogleLogging(argv[0]);
    create_shard(string(argv[1]), string(argv[2]));
  }
  return 0;
}

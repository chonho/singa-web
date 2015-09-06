libs :=singa glog protobuf

.PHONY: all download create

create:
	$(CXX) create_shard.cc -std=c++11 -lsinga -lprotobuf -lglog -lzookeeper_mt \
		-I../../include -L../../.libs/ -Wl,-unresolved-symbols=ignore-in-shared-libs \
		-Wl,-rpath=../../.libs/  -o create_shard.bin
	mkdir cfood_train_shard
	mkdir cfood_test_shard
	./create_shard.bin images_bin/ .




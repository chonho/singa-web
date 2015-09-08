#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

#include <trainer/classifier.h>

namespace singa {

class HttpRequestHandler {

public:
 
  void Setup() {
	    for(int i=0; i<(signed int)classifiers_.size(); i++)
		is_available_.push_back(true);
	}
  
  int Start();

  int getIndex() {
	    for(int i=0; i<(signed int)classifiers_.size(); i++) {
		if (is_available_[i] == true) return i;
		else return -1;
	    }
  	}

  vector<Classifier*>* ptr_classifiers() { return &classifiers_; }

private:

  static int send_page (struct MHD_Connection *connection, const char *page);

  static int iterate_post (void *coninfo_cls, 
              enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size);
  
  static int answer_to_connection (void *cls,
                          struct MHD_Connection *connection,
                          const char *url,
                          const char *method,
                          const char *version,
                          const char *upload_data,
                          size_t *upload_data_size, void **con_cls);
 
//  static Classifier* classifiers(int idx) { return classifiers_[idx]; }

  vector<Classifier*> classifiers_;

  vector<bool> is_available_;

};

} // singa

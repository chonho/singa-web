#include <httprequesthandler.h>
#include <signal.h>

namespace singa {

#define PORT		8080

#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512

#define GET 0
#define POST 1

#define KEY_IMAGE "image"
#define KEY_TESTID "testid"

struct connection_info_struct
{
  int connectiontype;
  char *answerstring;
  char *imagepath;
  int testid;
  struct MHD_PostProcessor *postprocessor;
};

const char *askpage = "<html><body>\
                       What's image path?<br>\
                       <form action=\"/namepost\" method=\"post\">\
                       <input name=\"name\" type=\"text\"\
                       <input type=\"submit\" value=\" Send \"></form>\
                       </body></html>";

const char *page_post =
  "<html><body><h1>Image Path: %s received!</center></h1></body></html>";

const char *errorpage =
  "<html><body>This doesn't seem to be right.</body></html>";

static int HttpRequestHandler::send_page (struct MHD_Connection *connection, const char *page)
{
  int ret;
  struct MHD_Response *response;

  response =
    MHD_create_response_from_buffer (strlen (page), (void *) page,
				     MHD_RESPMEM_PERSISTENT);
  if (!response)
    return MHD_NO;

  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  return ret;
}

static int HttpRequestHandler::iterate_post (void *coninfo_cls,
	      enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size)
{
  struct connection_info_struct *con_info = (connection_info_struct*) coninfo_cls;

  if (0 == strcmp (key, KEY_IMAGE)) // retrieve value for "image"
  {
      if ((size > 0) && (size <= MAXNAMESIZE))
      {
	  char *value;
          value = (char*) malloc(MAXANSWERSIZE);
          if (!value) return MHD_NO;
          snprintf (value, MAXANSWERSIZE, "%s", data);
	  con_info->imagepath = value;

          char *answerstring;
          answerstring = (char*) malloc(MAXANSWERSIZE);
          if (!answerstring) return MHD_NO;
          snprintf (answerstring, MAXANSWERSIZE, page_post, data);
          con_info->answerstring = answerstring;
      }
      else
        con_info->answerstring = NULL;

      return MHD_YES;
  }


  if (0 == strcmp (key, KEY_TESTID)) {
     
     con_info->testid = atoi(data); 
     return MHD_YES;

  }

  return MHD_YES;
}

static int HttpRequestHandler::answer_to_connection (void *cls,
                          struct MHD_Connection *connection,
                          const char *url,
                          const char *method,
                          const char *version,
                          const char *upload_data,
                          size_t *upload_data_size,
			  void **con_cls)
{
   const char *page_get  = "<html><body>Hello, browser! Works! GET</body></html>";

   if (NULL == *con_cls)
   {
      struct connection_info_struct *con_info;


      con_info = malloc (sizeof (struct connection_info_struct));
      if (NULL == con_info)
        return MHD_NO;
      con_info->answerstring = NULL;

      if (0 == strcmp (method, "POST")) 
      {
          con_info->postprocessor =
            MHD_create_post_processor (connection, POSTBUFFERSIZE,
                                       iterate_post, (void *) con_info);
          
	  if (NULL == con_info->postprocessor)
            {
              free (con_info);
              return MHD_NO;
            }

          con_info->connectiontype = POST;
      }
      else
        con_info->connectiontype = GET;

      *con_cls = (void *) con_info;

      return MHD_YES;
    }

    //printf("%s, %s, %s, %s\n", url, method, version, upload_data);

    if (0 == strcmp (method, "GET")) {

	return send_page( connection, page_get );
    }

    string output="";

    if (0 == strcmp (method, "POST")) {
	  
        struct connection_info_struct *con_info = (connection_info_struct*) *con_cls;

        if (*upload_data_size != 0)
        {
          MHD_post_process (con_info->postprocessor,
                            upload_data,
                            *upload_data_size);

    	  HttpRequestHandler* handler = (HttpRequestHandler*) cls;
          int idx = 0;
	  while((idx = handler->getIndex())==-1);
	  //printf("---available classifier[%d]\n",idx);
	  Classifier* classifier = handler->classifiers_[idx];
	  //classifier->setTestImage( con_info->imagepath, con_info->testid );
	  classifier->Load();
          handler->is_available_[idx] = false;
	  classifier->Run( &output );
          handler->is_available_[idx] = true;

          *upload_data_size = 0;
	
          char *temp;
          temp = (char*) malloc(MAXANSWERSIZE);
          snprintf (temp, MAXANSWERSIZE, "%s", output.c_str());
          con_info->answerstring = temp;

          return MHD_YES;
        }
        else if (NULL != con_info->answerstring)
        {
          return send_page (connection, con_info->answerstring);
        }
    }

    return send_page (connection, errorpage);
}

static void HttpRequestHandler::request_completed (void *cls,
			    struct MHD_Connection *connection,
			    void **con_cls,
			    enum MHD_RequestTerminationCode toe)
{
  struct connection_info_struct *con_info = *con_cls;

  if (NULL == con_info) return;
  if (con_info->connectiontype == POST)
    {
      MHD_destroy_post_processor (con_info->postprocessor);        
      if (con_info->answerstring) free (con_info->answerstring);
    }
  
  free (con_info);
  *con_cls = NULL;   
}

static int controlid = 0;
void sigcatch(int sig) {
    printf("---signal %d: ", sig);
    if(sig == SIGUSR2) controlid = 3; 
    usleep(1000);
}

int HttpRequestHandler::Start() {

  if (SIG_ERR == signal(SIGUSR2, sigcatch)) {
        printf("failed to set signal handler.n");
        exit(1);
  }

  struct MHD_Daemon *daemon;
    
      /*
      while( controlid != 1 && controlid !=3 ) usleep(1000);
      printf(""); // TODO if remove, it does not work...

      if(controlid == 3) {
         // 3: terminated 
         printf("Server Terminated\n");
	 break;
      }
      */

      this->Setup();

      daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION, PORT, NULL, NULL,
                             &answer_to_connection, this,
                             //MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
			     MHD_OPTION_NOTIFY_COMPLETED, &request_completed,
                             MHD_OPTION_END);

      if (NULL == daemon) return 1;
      printf("Server Starts \n");

      while( controlid != 3) usleep(1000);

      MHD_stop_daemon (daemon);
      printf("Server Stops (pause) \n");
  

  return 0;

}

} // singa
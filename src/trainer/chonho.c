#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

//#define PORT 8888

int answer_to_connection (void *cls,
                          struct MHD_Connection *connection, 
                          const char *url, 
                          const char *method,
                          const char *version, 
                          const char *upload_data, 
                          size_t *upload_data_size, void **con_cls)
{
  const char *page  = "<html><body>Hello, browser! Works!</body></html>";

  static int aptr;
  struct MHD_Response *response;
  int ret;

  if (0 != strcmp (method, "GET"))
    return MHD_NO;              /* unexpected method */
  if (&aptr != *con_cls)
    {
      /* do never respond on first call */
      *con_cls = &aptr;
      return MHD_YES;
    }

  *con_cls = NULL; // reset

  response = MHD_create_response_from_buffer (strlen (page),
                                              (void*) page, 
                                              MHD_RESPMEM_PERSISTENT);

  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);

  return ret;
}


int main ()
{
  struct MHD_Daemon *daemon;
  static int PORT = 8888;

  printf("%d\n", MHD_USE_SELECT_INTERNALLY);

  daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, 
                             &answer_to_connection, NULL,
                             //MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
                             MHD_OPTION_END);

  if (NULL == daemon) return 1;

  getchar (); 

  MHD_stop_daemon (daemon);
  return 0;
}

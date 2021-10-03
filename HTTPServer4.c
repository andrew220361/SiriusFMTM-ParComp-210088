// vim:ts=2:et
//===========================================================================//
//                                "HTTPServer4.c":                           //
//           Simple Concurrent/Parallel Thread Pool-Based HTTP Server        //
//===========================================================================//
#include "ServerSetup.h"
#include "ProcessHTTPReqs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

//===========================================================================//
// Thread Pool Setup:                                                        //
//===========================================================================//
// Queue of items (Socket Descrs) to be processed:
// "static" means that the variables declared below are visible to all functi-
// ones of this compilation unit (this .c file), but NOT visible to other com-
// pilation units (ie other files), to prevent "pollution" of the global name
// space:
static int const QSize  = 65536;
static int const Q[QSize];
static int       QFront =  0;
static int       QBack  = -1;

static void* ThreadBody(void* a_arg);

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  // Get the Acceptor Socket:
  int sd = ServerSetup(argc, argv);
  if (sd < 0)
    return 1;

  // Create the Pool of Threads: Default size is 100:
  int poolSize = (argc >= 3) ? atoi(argv[2]) : 100;



  // Acceptor Loop:
  while (1)
  {
    // Accept a connection, create a data exchange socket:
    int sd1 = accept(sd, NULL, NULL);
    if (sd1 < 0)
    {
      // Some error in "accept", but may be not really serious:
      if (errno == EINTR)
        // "accept" was interrupted by a signal, this is OK, just continue:
        continue;

      // Any other error:
      fprintf (stderr, "ERROR: accept failed: %s, errno=%d\n",
               strerror(errno), errno);
      return 1;
    }

    // Create a new thread which will deal with the connected client:
    // XXX: The thread will be created with default attributes:
    pthread_t th;   // Thread Handle

    int rc = pthread_create(&th, NULL, ThreadBody, &sd1);
    if (rc < 0)
    {
      fprintf(stderr, "ERROR: pthread_create failed: %s, errno=%d\n",
              strerror(errno), errno);
      return 1;
    }
    // Parent proceeds to the next "accept" immediately!
  }
  return 0;
}

//===========================================================================//
// "ThreadBody":                                                             //
//===========================================================================//
void* ThreadBody(void* a_arg)
{
  // "a_arg" is actually a ptr to client-communicating socket descr sd1:
  assert    (a_arg != NULL);
  int const* sd1ptr = (int const*) a_arg;
  int  sd1 = *sd1ptr;

  ProcessHTTPReqs(sd1);
  return NULL;    // The return value is not used
}

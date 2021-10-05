// vim:ts=2:et
//===========================================================================//
//                                "HTTPServer4.cpp":                         //
//           Simple Concurrent/Parallel Thread Pool-Based HTTP Server        //
//===========================================================================//
#include "ServerSetup.h"
#include "ProcessHTTPReqs.h"
#include "CircularBuffer.hpp"
#include "ThreadPool.hpp"
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

//===========================================================================//
// THreadPool-Related Types:                                                 //
//===========================================================================//
// Thereads consume int sokcet descrs, return nothing (void), XXX but for syn-
// tacting reasons, it must be a non-void type, so use int again:
using TP   = ThreadPool <int, int>;

// Wrapper must be (void*) -> (void*) for compatibility with PThreads API:
static void* WrapperFunc(void* a_arg);

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  // Get the Acceptor Socket:
  int sd = ServerSetup(argc, argv);
  if (sd < 0)
    return 1;

  // Create the Pool of Threads:
  // [2]: #Threads         = M
  // [3]: CircBuffCapacity = N
  //
  int M = (argc >= 3) ? atoi(argv[2]) :   128;
  int N = (argc >= 4) ? atoi(argv[3]) : 16384;

  // Create the ThreadPool:
  TP tp(M, N, WrapperFunc, ProcessHTTPReqs);

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
    // Submit an asynchronous job to the ThreadPool. We don't need a result,
    // hence NULL ptr:
    tp.SubmitJob(sd, NULL);
  }
  return 0;
}

//===========================================================================//
// "WrapperFunc":                                                            //
//===========================================================================//
void* WrapperFunc(void* a_arg)
{
  // "a_arg" is a ptr to the ThreadPool object:
  assert   (a_arg != NULL);
  TP* tp = (TP*)(a_arg);

  // Invoke the method "ThreadBody" on the "ThreadPool" obj pointed to by "tp":
  tp->ThreadBody();
  return NULL;
}

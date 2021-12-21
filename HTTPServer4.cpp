// vim:ts=2:et
//===========================================================================//
//                                "HTTPServer4.cpp":                         //
//           Simple Concurrent/Parallel Thread Pool-Based HTTP Server        //
//===========================================================================//
#include "ServerSetup.h"
#include "ProcessHTTPReqs.h"
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
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  // Args: ServerPort [ThreadPoolSize [BuffSize]]
  // Get the Acceptor Socket:
  int sd = ServerSetup(argc, argv);
  if (sd < 0)
    return 1;

  // Create the Pool of Threads: Default PoolSize is 128, BuffSize is 8192:
  int poolSize = (argc >= 3) ? atoi(argv[2]) :  128;
  int buffSize = (argc >= 4) ? atoi(argv[3]) : 8192;

  // WorkItem=int, Res=void:
  // This immediately starts the WorkerTheads:
  SiriusFMTM::ThreadPool<int, void, decltype(ProcessHTTPReqs)> tp
    (size_t(poolSize), size_t(buffSize), ProcessHTTPReqs);

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
    // Submit an asynchronous job to the ThreadPool. We don't need a result:
    tp.Submit(sd);
  }
  return 0;
}

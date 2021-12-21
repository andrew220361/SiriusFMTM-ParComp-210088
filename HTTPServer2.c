// vim:ts=2:et
//===========================================================================//
//                                "HTTPServer2.c":                           //
//              Simple Concurrent/Parallel Multi-Process HTTP Server         //
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
#include <signal.h>
#include <unistd.h>
#include <assert.h>

static void SigHandler(int a_signum);

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  // Get the Acceptor Socket:
  int sd = ServerSetup(argc, argv);
  if (sd < 0)
    return 1;

  // Setup the signal handler to get asynchronous notifications of child
  // process terminations. Ignore the return valud (void), check errno instead:
  (void) signal(SIGCHLD, SigHandler);
  assert(errno == 0);

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

    // Create a new process which will deal with the connected client:
    if (fork() == 0)
      // This is a Child Process which will actually service the request, and
      // then terminate:
      return ProcessHTTPReqs(sd1);

    // In parent: close the socket "sd1", otherwise it will not be fully closed
    // in child:
    close(sd1);

    // Parent proceeds to the next "accept" immediately!
  }
  return 0;
}

//===========================================================================//
// "SigHandler":                                                             //
//===========================================================================//
void SigHandler(int a_signum)
{
  assert(a_signum == SIGCHLD);
  fputs("INFO: Received SIGCHLD\n", stderr);

  // Check (non-blocking) if any children have terminated, to prevent them from
  // turning into zombies:
  waitpid(-1, NULL, WNOHANG);
}


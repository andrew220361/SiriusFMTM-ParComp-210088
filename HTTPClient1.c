#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    fputs("ARGUMENT: URL\n", stderr);
    return 1;
  }
  //-------------------------------------------------------------------------//
  // Parse the URL:                                                          //
  //-------------------------------------------------------------------------//
  // For example:
  // http://hostname:port/path
  //
  char* url = argv[1];

  // Find the "://" separator:
  char* protoEnd = strstr(url, "://");
  if (protoEnd == NULL)
  {
    fprintf(stderr, "Invalid URL: %s\n", url);
    return 1;
  }
  // Dirty trick: DESTRUCTIVE PARSING:
  // Currently, protoEnd points to ':' followed by "//..."
  // Overwrite by '\0' to terminate the proto:
  *protoEnd = '\0';
  char const* proto = url;
  url = protoEnd + 3;  // Now points to the hostname

  // XXX: Currently, only "http" proto is supported:
  if (strcmp(proto, "http") != 0)
  {
    fprintf(stderr, "Unsupported Protocol: %s\n", proto);
    return 1;
  }
  char* hostPortEnd = strchr(url, '/');
  char* path        = NULL;
 
  if (hostPortEnd == NULL)
    path = "/";       // By default
  else
  {
    size_t  pathLen = strlen(hostPortEnd); // W/o  ending '\0'!
    path =  malloc(pathLen + 1);           // With ending '\0'!
    (void)  strncpy(path, hostPortEnd, pathLen + 1);
    *hostPortEnd    = '\0';
  }

  // Have we got port?
  char const* hostName = url;
  int         port     = 0;
  char* colon = strchr(url, ':');

  if (colon == NULL)
    // Port not specified, use the default one (for HTTP):
    port = 80;
  else
  {
    // Replace ':' with '\0', thus terminating the hostName:
    *colon = '\0';
    // colon+1 points to port digits:
    port   = atoi(colon + 1);
    if (port <= 0)
    {
      fprintf(stderr, "Invalid Port: %d\n", port);
      return 1;
    }
  }
  // Parsing Done:
  //-------------------------------------------------------------------------//
  // Connect to hostName:                                                    //
  //-------------------------------------------------------------------------//
  // Get the hostIP: Make DNS enquiery (XXX: BLOCKING, NON-THREAD-SAFE!):
  //
  struct hostent* he = gethostbyname(hostName);
  if (he == NULL || he->h_length == 0)
  {
    fprintf(stderr, "Cannot resolve HostName: %s\n", hostName);
    return 1;
  }
  /*
  fprintf(stderr, "Proto    = %s\n", proto);
  fprintf(stderr, "HostName = %s\n", hostName);
  fprintf(stderr, "Port     = %d\n", port);
  fprintf(stderr, "Path     = %s\n", path);
  */
  return 0;
}

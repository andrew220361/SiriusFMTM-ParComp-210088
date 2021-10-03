#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

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
    fprintf(stderr, "ERROR: Invalid URL: %s\n", url);
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
    fprintf(stderr, "ERROR: Unsupported Protocol: %s\n", proto);
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
      fprintf(stderr, "ERROR: Invalid Port: %d\n", port);
      return 1;
    }
  }
  // Parsing Done:
  //-------------------------------------------------------------------------//
  // Get the hostIP: Make DNS enquiery (XXX: BLOCKING, NON-THREAD-SAFE!):    //
  //-------------------------------------------------------------------------//
  struct hostent* he = gethostbyname(hostName);
  int nAddrs = 0;
  for (; he != NULL && he->h_addr_list[nAddrs] != NULL; ++nAddrs) ;

  if (he == NULL || he->h_length !=4 || he->h_addrtype != AF_INET ||
      nAddrs == 0)
  {
    fprintf(stderr, "ERROR: Cannot resolve HostName: %s\n", hostName);
    return 1;
  }
  // Get a random address from the list received:
  struct timeval tv;
  (void) gettimeofday(&tv, NULL);
  int h  = (int)tv.tv_usec % nAddrs;
  struct in_addr const* ia = (struct in_addr const*)(he->h_addr_list[h]);

  //-------------------------------------------------------------------------//
  // Connect to HostIP+Port:                                                 //
  //-------------------------------------------------------------------------//
  int sd = socket(AF_INET, SOCK_STREAM, 0);  // Default protocol: TCP
  if (sd < 0)
  {
    fprintf(stderr, "ERROR: Cannot create socket: %s\n", strerror(errno));
    return 1;
  }
  // Can also bind this socket to a LocalIP+LocalPort, but this is not strictly
  // necessary...
  // Make the server address:
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_addr   = *ia;
  sa.sin_port   = htons((uint16_t)port);

  int rc = 0;
  while (1)
  {
    rc = connect(sd, (struct sockaddr const*)(&sa), sizeof(sa));
    if (rc < 0)
    {
      if (errno == EINTR)
        continue;  // Innocent error

      // Any other error: exit:
      fprintf(stderr, "ERROR: Cannot connect to server: SD=%d, IP=%s, Port=%d: "
              "%s, errno=%d\n",
              sd, inet_ntoa(sa.sin_addr), port, strerror(errno), errno);
      close(sd);
      return 1;
    }
  }
  // If we got here, TCP connect was successful!

  //-------------------------------------------------------------------------//
  // Initialise the HTTP session:                                            //
  //-------------------------------------------------------------------------//
  // Send the HTTP/1.1 Request:
  // GET Path HTTP/1.1
  // Headers...
  char sendBuff[1024];

  snprintf(sendBuff, sizeof(sendBuff),
          "GET %s HTTP/1.1\r\n"
          "Host: %s\r\n"
          "Connection: Close\r\n"
          "\r\n",
          path, hostName);

  fprintf(stderr, "INFO: Sending the HTTP Request:\n%s", sendBuff);

  rc = send(sd, sendBuff, strlen(sendBuff), 0);

  // We expect all data in "sendBuff" to be sent at once. In general, this may
  // not be the case:
  if (rc != strlen(sendBuff))
  {
    fprintf(stderr, "ERROR: Cannot send HTTP Req: Only %d bytes sent, %s, "
            "errno=%d\n", rc, strerror(errno), errno);
    close(sd);
    return 1;
  }
  //-------------------------------------------------------------------------//
  // Get the response:                                                       //
  //-------------------------------------------------------------------------//
  char recvBuff[8192];
  while (1)
  {
    rc = recv(sd, recvBuff, sizeof(recvBuff)-1, 0);  // 1 byte for 0-terminator
    // rc >  0: #bytes received
    // rc <  0: error
    // rc == 0: the server has closed connection
    if (rc < 0)
    {
      if (errno == EINTR)
        continue;  // Innocent, just try again

      // ANy other error: exit:
      fprintf(stderr, "ERROR: recv failed: %s, errno=%d\n",
              strerror(errno), errno);
      close(sd);
      return 1;
    }
    else
    if (rc == 0)
      break;

    // Recv successful! 0-terminate the string and print what we got:
    recvBuff[rc] = '\0';

    // Don't use "puts" or so, use binary write to the standard output:
    (void) write(1, recvBuff, rc);
  }
  //-------------------------------------------------------------------------//
  // Clean-up:                                                               //
  //-------------------------------------------------------------------------//
  close(sd);
  return 0;
}

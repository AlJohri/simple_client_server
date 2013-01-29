#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100
#define MAXCONNECTIONS 100
#define BACKLOG 10

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock;
  struct sockaddr_in sa,sa2;
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */

  if (*argv[1] == 'k') {
    minet_init(MINET_KERNEL);
  }
  else if (*argv[1] == 'u') {
    minet_init(MINET_USER);
  }
  else {
    minet_perror("Use k or u for first argument.\n");
    exit(-1);
  }

  if ((sock = minet_socket(SOCK_STREAM)) < 0) {
    minet_perror("socket");
    exit(-1);
  }
  maxfd = sock;

  /* set server address*/

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(server_port);

  /* bind listening socket */

  if (minet_bind(sock, &sa) < 0) {
    minet_perror("bind");
    exit(-1);
  }
 
  /* start listening */

  if (minet_listen(sock, BACKLOG) < 0) {
    minet_perror("listen");
    exit(-1);
  }

  //socklen_t len = sizeof(sa2);
  
  FD_ZERO(&connections);
  FD_ZERO(&readlist);

  FD_SET(sock, &connections);

  /* connection handling loop */
  while(1)
  {
    /* create read list */

    readlist = connections;

    /* do a select */

    if ((rc = minet_select(maxfd + 1, &readlist, 0, 0, 0)) < 0) {
      minet_perror("select");
      exit(-1);
    }
    /* process sockets that are ready */

    if (rc > 0) {
      for (i = 0; i <= maxfd; i++) {
	      if (FD_ISSET(i, &readlist)) {
          /* for the accept socket, add accepted connection to connections */
          if (i == sock)
          {
	          if ((rc = minet_accept(sock, &sa2)) < 0) {
	            minet_perror("accept");
	            continue; //exit(-1);
	          }
	          FD_SET(rc, &connections);
	          if (rc > maxfd)
	            maxfd = rc;
          }
          else /* for a connection socket, handle the connection */
          {
	          if ((rc = handle_connection(i)) < 0) {
	            minet_perror("hc");
	            exit(-1);
	          }
	          FD_CLR(i, &connections);
          }
        }
      }
      rc = 0;
    }
  }
}

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  int datalen=0;
  char *ok_response_f = (char*) "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = (char*) "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/

  memset(buf, 0, sizeof(buf));

  if (minet_read(sock2, buf, sizeof(buf)) < 0) {
    minet_perror("recv");
    return -1;
  }
  
  char *rsp = buf + 2;

  while (!(rsp[0] == '\n' && rsp[-2] == '\n')) {
    if (rsp == buf + strlen(buf)) {
      if (minet_read(sock2, rsp, sizeof(buf) - strlen(buf)) < 0) {
	      minet_perror("recv");
	      return -1;
      }
    }
    rsp++;
  }
  
  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/

  sscanf(buf, "%*s %s", (char*) &buf);

    /* try opening the file */

  memset(filename, 0, FILENAMESIZE + 1);

  getcwd(filename, FILENAMESIZE);
  filename[strlen(filename)] = '/';
  strncpy(filename + strlen(filename), buf, strlen(buf));

  ok = (!stat(filename, &filestat));

  /* send response */
  if (ok)
  {
    /* send headers */
    sprintf(ok_response, ok_response_f, filestat.st_size);
    FILE* pFile = fopen(filename, "r");
    memset(buf, 0, BUFSIZE);
    fread(buf, sizeof(char), BUFSIZE, pFile);
    datalen = strlen(buf) + strlen(ok_response) + 1;
    headers = (char*) malloc(strlen(buf) + strlen(ok_response) + 1);
    strncpy(headers, ok_response, strlen(ok_response));
    strncpy(headers + strlen(ok_response), buf, strlen(buf));
    while (send(sock2, headers, datalen, 0) > 0)
        datalen = fread(headers, sizeof(char), datalen, pFile);
    fclose(pFile);
    free(headers);
    /* send file */
  }
  else	// send error response
  {
    if (send(sock2, notok_response, strlen(notok_response), 0) < 0) {
      minet_perror("send");
      return -1;
    }
  }

  /* close socket and free space */

  close(sock2);

  return 0;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}


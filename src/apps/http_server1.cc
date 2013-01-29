#include "minet_socket.h"
#include "minet_wrapper.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100
#define BACKLOG 10

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int parseRequest(char*, char*, struct stat*);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;

  /* parse command line args */
  if (argc != 3) { fprintf(stderr, "usage: http_server1 k|u port\n"); exit(-1); }
  if ((server_port = atoi(argv[2])) < 1500) { fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port); exit(-1); }
  if (tolower(*argv[1]) != 'k' && tolower(*argv[1]) != 'u') fprintf(stderr, "Use k or u for first argument.\n");

  /* initialize and make socket */
  if (tolower(*argv[1]) == 'k') Minet_init(MINET_KERNEL);
  else if (tolower(*argv[1]) == 'u') Minet_init(MINET_USER);
  sock = Minet_socket(SOCK_STREAM);

  /* set server address*/
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(server_port);

  Minet_bind(sock, &sa); // bind listening socket
  Minet_listen(sock, BACKLOG); // start listening

  /* connection handling loop */
  while(true)
  {
    sock2 = Minet_accept(sock, &sa2); // accept connection
    handle_connection(sock2); // handle connection
  }
}

int handle_connection(int sock2)
{

  char* ok_response_f = (char*) "HTTP/1.0 200 OK\r\nContent-type: text/plain\r\nContent-length: %d \r\n\r\n";
  char* notok_response_f = (char*) "HTTP/1.0 404 FILE NOT FOUND\r\nContent-type: text/html\r\n\r\n<html><body bgColor=black text=white>\n<h2>404 FILE NOT FOUND</h2>\n</body></html>\n";
  
  char buf[BUFSIZE+1]; memset(buf, 0, sizeof(buf)); // declare buffer and memset the buffer
  
  if (!Minet_read(sock2, buf, BUFSIZE)) return -1;

  /* parse request to get filename (assuming GET request and filename with no spaces) */
  char filename[FILENAMESIZE+1]; struct stat filestat; // declare filename buffer and memset it. declare filestat struct
  bool ok = parseRequest(buf, filename, &filestat);

  /* send response */
  if (ok) {
    /* construct header */
    char ok_response[100];
    sprintf(ok_response, ok_response_f, filestat.st_size);

    /* construct file */
    FILE* pFile = fopen(filename, "r");
    fread(buf, sizeof(char), BUFSIZE, pFile);

    /* construct resposne */
    int datalen = strlen(ok_response) + strlen(buf) + 1;
    char *response = (char*) malloc(datalen);
    strncpy(response, ok_response, strlen(ok_response));
    strncpy(response + strlen(ok_response), buf, strlen(buf));

    /* send resposne bytes */
    if (writenbytes(sock2, response, datalen) < 0) { Minet_close(sock2); return -1; }

    fclose(pFile);
    free(response);

  }
  // send error response
  else if (writenbytes(sock2, notok_response_f, strlen(notok_response_f)) < 0) { Minet_close(sock2); return -1; }

  /* close socket and free space */
  Minet_close(sock2);

  return 0;
}

int parseRequest(char* buf, char* filename, struct stat* filestat) {
  memset(filename, 0, FILENAMESIZE + 1); // memset filename
  buf = strtok(buf, " "); buf = strtok(NULL, " "); // put the request's filename in buf
  //sscanf(buf, "%*s %s", (char*) &buf);
  getcwd(filename, FILENAMESIZE); // fill filename with current working directory (filepath)
  filename[strlen(filename)] = '/'; // put '/' at the end of filepath
  strncpy(filename + strlen(filename), buf, strlen(buf)); // append the request's filename to the end of filepath
  memset(buf, 0, BUFSIZE);
  return (!stat(filename, filestat)); // if file exists, return true
}

int readnbytes(int fd,char *buf,int size)
{
  int rc, totalread; rc = totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0) totalread += rc;
  return (rc < 0) ? -1 : totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc, totalwritten; rc = totalwritten = 0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0) totalwritten += rc;
  return (rc < 0) ? -1 : totalwritten;
}
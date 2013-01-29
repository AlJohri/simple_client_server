#include "minet_socket.h"
#include "minet_wrapper.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <string>


#define FILENAMESIZE 100
#define BUFSIZE 1024
#define BACKLOG 10

typedef enum {NEW,READING_HEADERS,WRITING_RESPONSE,READING_FILE,WRITING_FILE,CLOSED} states;
typedef struct connection_s connection;
typedef struct connection_list_s connection_list;

struct connection_s
{
  int sock;
  int fd;
  char filename[FILENAMESIZE+1];
  char buf[BUFSIZE+1];
  char *endheaders;
  bool ok;
  long filelen;
  states state;
  int headers_read,response_written,file_read,file_written;

  connection *next;
};


struct connection_list_s
{
  connection *first,*last;
};

void add_connection(int,connection_list *);
void insert_connection(int,connection_list *);
void init_connection(connection *con);

int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
void read_headers(connection *);
void write_response(connection *);
void read_file(connection *);
void write_file(connection *);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  //int rc;
  fd_set readlist,writelist;
  connection_list connections;
  connections.first = connections.last = NULL;
  struct timeval tv; //time interval
  tv.tv_sec = 2;
  tv.tv_usec = 500000;
  int maxfd;

  /* parse command line args */
  if (argc != 3) { fprintf(stderr, "usage: http_server1 k|u port\n"); exit(-1); }
  if ((server_port = atoi(argv[2])) < 1500) { fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port); exit(-1); }
  if (tolower(*argv[1]) != 'k' && tolower(*argv[1]) != 'u') fprintf(stderr, "Use k or u for first argument.\n");

  /* initialize and make socket */
  if (tolower(*argv[1]) == 'k') Minet_init(MINET_KERNEL);
  else if (tolower(*argv[1]) == 'u') Minet_init(MINET_USER);
  sock = Minet_socket(SOCK_STREAM);
  maxfd = sock;

  /* set server address*/
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(server_port);

  Minet_bind(sock, &sa); /* bind listening socket */  
  Minet_listen(sock, BACKLOG); /* start listening */
  
  FD_ZERO(&readlist);
  FD_ZERO(&writelist);
  add_connection(sock, &connections);
  minet_set_nonblocking(sock);

  /* connection handling loop */
  while(true) {

    /* create read and write lists */
    fd_set readlist, writelist;

    for (connection* i = connections.first; i != NULL; i = i->next) {
      if (i->state == READING_HEADERS || i->state == READING_FILE)
        FD_SET(i->sock, &readlist);
      if (i->state == WRITING_RESPONSE || i->state == WRITING_FILE)
        FD_SET(i->sock, &writelist);
    }

    FD_SET(sock, &readlist);

    /* do a select */
    if (minet_select(maxfd + 1, &readlist, &writelist, 0, 0) < 0) minet_perror("select"); //&tv
    else { /* process sockets and file descriptors that are ready */
      for (int i = 0; i <= maxfd; i++) { // i is a file descriptor

        if (i == sock) {

          if(FD_ISSET(sock, &readlist)) {
            if (!(sock2 = Minet_accept(sock, &sa2))) continue;
            else {
              minet_set_nonblocking(sock2);
              insert_connection(sock2, &connections);
              FD_SET(sock2, &readlist);
              if (sock2 > maxfd) maxfd = sock2;
            }
          }

        }

        if (FD_ISSET(i, &readlist)) {
          //if (minet_can_read_now(i)) {
            /* ---------------------------------------------------------- */

            // j is the connection in the open connection_list that matches i 
            for (connection* j = connections.first; j != NULL; j = j->next) {
              if(i == j->sock) {
                read_headers(j);
              }
              else if (i == j->fd) {
                read_file(j);
              }
            }

            /* ---------------------------------------------------------- */
          //}
        }

        if (FD_ISSET(i, &writelist)) {
          if (minet_can_write_now(i)) {
            /* ---------------------------------------------------------- */

            // j is the connection in the open connection_list that matches i 
            for (connection* j = connections.first; j != NULL; j = j->next) {
              if(i == j->sock) {
                write_response(j);
              }
              else if (i == j->fd) {
                write_file(j);
              }
            }

            /* ---------------------------------------------------------- */
          }
        }        
      }
    }
  }
}

void read_headers(connection *con)
{
  /* get request and headers*/
  int rc = minet_read(con->sock, con->buf+con->headers_read,BUFSIZE);
  if (rc < 0) { 
    if (errno == EAGAIN) {
      con->state = READING_HEADERS;
      return;
    }
    else { 
      minet_perror("read"); 
      return; // ????????????????????????????????????
    }
  }
  else if (rc == 0) { // connection is closed on client side
    con->state = CLOSED;
    minet_close(con->sock);
  }
  else {
    con->headers_read += rc;
    con->state = READING_HEADERS;
    con->endheaders = strstr(con->buf,"\r\n\r\n");

    if (con->endheaders) { // Header Complete 
      std::string filename;
      std::string buffer = con->buf;
      
      filename = buffer.substr(4,buffer.find(" ", 4)-4); // get file name (get request, no spaces in fname)
      if(filename[0] == '\\' || filename[0] == '/') filename = filename.substr(1); // remove slash in the beginning
      con->ok = !!(con->fd = open(con->filename, O_RDONLY)); // open file to check if it exists

      if (con->ok) {
        minet_set_nonblocking(con->fd); // set to non-blocking
        struct stat filestat; fstat(con->fd, &filestat); // get filestat
        con->filelen = filestat.st_size; // get filesize
        strcpy(con->filename, filename.data()); //copy name of file
        // if(con->fd > maxfd) maxfd = con->fd; // increase maxfd if needed ?????????????????????????????????
      }
      write_response(con);
    }
    else return;
  }
}

void write_response(connection *con)
{
  char *ok_response_f = (char *) "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char *notok_response_f = (char *) "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  /* send response */
  if (con->ok)
  {
    /* send headers */
    memset(con->buf, 0, BUFSIZE); // memset buffer
    sprintf(con->buf, ok_response_f, con->filelen); //construct header and put it in buffer
    int rc = writenbytes(con->sock, con->buf, strlen(con->buf) + 1);
    if (rc < 0) { 
      if (errno == EAGAIN) {
        con->state = WRITING_RESPONSE;
        return;
      }
      else {
        minet_perror("write"); 
        return; // ????????????????????????????????????
      }
    }
    else if (rc == 0) {
      con->state = CLOSED;
      minet_close(con->sock);       
    }
    else {
      con->response_written += rc;
      con->state = WRITING_RESPONSE;
    }

    /* write file */
    write_file(con);
  }
  else
  {
    if (writenbytes(con->sock, notok_response_f, strlen(notok_response_f)) < 0) { 
      con->state = CLOSED;
      minet_close(con->sock);
      return;
    }
  }  
}

void read_file(connection *con)
{
  int rc;

  /* send file */
  rc = read(con->fd,con->buf,BUFSIZE);
  if (rc < 0)
  { 
    if (errno == EAGAIN)
      return;
    fprintf(stderr,"error reading requested file %s\n",con->filename);
    return;
  }
  else if (rc == 0)
  {
    con->state = CLOSED;
    minet_close(con->sock);
  }
  else
  {
    con->file_read = rc;
    con->state = WRITING_FILE;
    write_file(con);
  }
}

void write_file(connection *con)
{
  int towrite = con->file_read;
  int written = con->file_written;
  int rc = writenbytes(con->sock, con->buf+written, towrite-written);
  if (rc < 0)
  {
    if (errno == EAGAIN)
      return;
    minet_perror("error writing response ");
    con->state = CLOSED;
    minet_close(con->sock);
    return;
  }
  else
  {
    con->file_written += rc;
    if (con->file_written == towrite)
    {
      con->state = READING_FILE;
      con->file_written = 0;
      read_file(con);
    }
    else
      printf("shouldn't happen\n");
  }
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0) {
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
  
  if (rc < 0) {
    return -1;
  }
  else
    return totalwritten;
}


// inserts a connection in place of a closed connection
// if there are no closed connections, appends the connection 
// to the end of the list

void insert_connection(int sock,connection_list *con_list)
{
  connection *i;
  for (i = con_list->first; i != NULL; i = i->next)
  {
    if (i->state == CLOSED)
    {
      i->sock = sock;
      i->state = NEW;
      return;
    }
  }
  add_connection(sock,con_list);
}
 
void add_connection(int sock,connection_list *con_list)
{
  connection *con = (connection *) malloc(sizeof(connection));
  con->next = NULL;
  con->state = NEW;
  con->sock = sock;
  init_connection(con);
  if (con_list->first == NULL)
    con_list->first = con;
  if (con_list->last != NULL)
  {
    con_list->last->next = con;
    con_list->last = con;
  }
  else
    con_list->last = con;
}

void init_connection(connection *con)
{
  con->headers_read = 0;
  con->response_written = 0;
  con->file_read = 0;
  con->file_written = 0;
}

/*


    // minet_set_nonblocking(fd);
    O_NONBLOCK
    // minet_can_write_now(int sockfd);
    // minet_can_read_now(int sockfd);
    //fcntl(fd, F_SETFL, O_NONBLOCK)
    //fcntl(fd, F_GETFL) | O_ACCMODE


 */
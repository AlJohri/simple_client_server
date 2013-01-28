#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    memset(buf, 0, BUFSIZE + 1);
   
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(-1);
    }

    // Do DNS lookup
    /* Hint: use gethostbyname() */

    site = gethostbyname(server_name);

    /* set address */

    sa.sin_family = AF_INET;
    bcopy((char*) site->h_addr, (char*) &sa.sin_addr.s_addr, site->h_length);
    sa.sin_port = htons(server_port);
    
    //getaddrinfo(site->h_addr, server_port, NULL, &sa);

    /* connect socket */
   
    if (connect(sock, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
	perror("connect");
	exit(-1);
    }

    /* send request */

    req = (char *) malloc(15 + strlen(server_path));
    sprintf(req, "GET %s HTTP/1.0\n\n", server_path);
    if (send(sock, req, strlen(req), 0) < 0) {
	perror("send");
	exit(-1);
    }

    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    
    FD_ZERO(&set);
    FD_SET(sock, &set);

    if (minet_select(sock + 1, &set, NULL, NULL, NULL) < 0) {
	perror("select");
	exit(-1);
    }

    /* first read loop -- read headers */
    
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200
    
    if (recv(sock, buf, BUFSIZE, 0) < 0) {
	perror("recv");
	exit(-1);
    }
    sscanf(buf, "%*s %d", &rc);

    ok = (rc == 200);
    /* print first part of response */

    /* second read loop -- print out the rest of the response */
    
    /*close socket and deinitialize */
    char *rsp = buf;

    while (!(rsp[0] == '\n' && rsp[-2] == '\n'))
	rsp++;

    free(req);

    if (ok) {
	fprintf(wheretoprint, "%s", rsp);
	while ((datalen = recv(sock, buf, BUFSIZE, 0)) > 0) {
	    buf[datalen] = '\0';
	    fprintf(wheretoprint, "%s", buf);
	}
	close(sock);
    	sock = NULL;
	return 0;
    } else {
	close(sock);
    	sock = NULL;
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}



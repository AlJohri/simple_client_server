#ifndef minet_wrapper
#define minet_wrapper

#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Inline Header Function */

void Minet_perror(const char *s) { minet_perror(s); exit(-1); }
void Minet_init(minet_socket_types type) { minet_init(type); }
void Minet_bind(int sockfd, struct sockaddr_in *myaddr) { if (minet_bind(sockfd, myaddr) < 0) Minet_perror("bind"); }
void Minet_listen(int sockfd, int backlog) { if (minet_listen(sockfd, backlog) < 0) Minet_perror("listen"); }
void Minet_close(int sockfd) { minet_close(sockfd); }

int Minet_socket(int type) {
	int socket;
	if ((socket = minet_socket(type)) < 0) Minet_perror("socket"); // (this exits)
	return socket;
}

int Minet_accept(int sockfd, struct sockaddr_in *addr) {
	int sock;
	if ((sock = minet_accept(sockfd, addr)) < 0) Minet_perror("accept"); // (this exits)
	return sock;
}

// do not exit(-1) on error, minet_perror is deliberate
int Minet_read(int fd, char *buf, int len) {
	int rc; 
	if ((rc = minet_read(fd, buf, len)) < 0) minet_perror("read");
	return rc;
}

#endif
#include <arpa/inet.h>		  /* inet_ntoa */
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

#define LISTENQ  1024  /* second argument to listen() */

/* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

unsigned get_num(int fd){
	int ret;
	char cnum[NUM_LINE_SIZE_BUF];
	ret = readn(fd, cnum, NUM_LINE_SIZE_BUF);
	if(ret < NUM_LINE_SIZE_BUF)
		perror(__func__);
	return atoi(cnum);
}

char* get_path(int fd, size_t len)
{
	int ret;
	char* path = (char*) malloc(len);
	ret = readn(fd, path, len);
	if(ret < len)
		perror(__func__);
	return path;
}

void __process(int fd, struct sockaddr_in *clientaddr){
	int path_size;
	char *exec_path;

	printf("accept request, fd is %d, pid is %d\n", fd, getpid());
	//resp_port = get_num(fd);
	path_size = get_num(fd);
	exec_path = get_path(fd, path_size);

	//log_access(status, clientaddr, &req);
	printf("exec path is %s\n", exec_path);

	char cfd [4];
	sprintf(cfd, "%d", fd);
	setenv("POPCORN_SOCK_FD", cfd, 1);
	setenv("POPCORN_REMOTE_START", "1", 1);

	execl(exec_path, exec_path, NULL);
	//execl("/usr/bin/gdb", "-iex \"set auto-load safe-path /\"", "-ex run", "--args", exec_path, NULL);

	perror(__func__);
}

void process(int fd, struct sockaddr_in *clientaddr){

        int pid = fork();

        if (pid == 0) {         //  child
                __process(fd, clientaddr);
        } else if (pid < 0) {   //  error
            perror("fork");
        }

	return; //parent
}


int open_listenfd(int port){
	int listenfd, optval=1;
	struct sockaddr_in serveraddr;

	/* Create a socket descriptor */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	/* Eliminates "Address already in use" error from bind. */
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
				   (const void *)&optval , sizeof(int)) < 0)
		return -1;

	// 6 is TCP's protocol number
	// enable this, much faster : 4000 req/s -> 17000 req/s
	if (setsockopt(listenfd, 6, TCP_CORK,
				   (const void *)&optval , sizeof(int)) < 0)
		return -1;

	/* Listenfd will be an endpoint for all requests to port
	   on any IP address for this host */
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);
	if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
		return -1;

	/* Make it a listening socket ready to accept connection requests */
	if (listen(listenfd, LISTENQ) < 0)
		return -1;
	return listenfd;
}

int main(int argc, char** argv){
	int default_port = 9999,
		listenfd,
		connfd;
	struct sockaddr_in clientaddr;
	socklen_t clientlen = sizeof clientaddr;

	if(argc == 2) {
		default_port = atoi(argv[1]);
	}

	listenfd = open_listenfd(default_port);
	if (listenfd > 0) {
		printf("listen on port %d, fd is %d\n", default_port, listenfd);
	} else {
		perror("ERROR");
		exit(listenfd);
	}

	signal(SIGCHLD, SIG_IGN);/* so that childrens dies directly */

	// Ignore SIGPIPE signal, so if browser cancels the request, it
	// won't kill the whole process.
	//signal(SIGPIPE, SIG_IGN);

	while(1){
		connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
		process(connfd, &clientaddr);
		//close(connfd);
	}
}

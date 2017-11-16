#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "config.h"
#include "communicate.h"

#define PATH_MAX 4096


static int server_sock_fd = 0;
static int ori_to_remote_sock = 0;

#define MAX_NUM_CHAR_SIZE 32
typedef int (*cmd_func_t) (char* arg, int size);


static ssize_t						 /* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;   /* and call write() again */
			else
				return (-1);	/* error */
		 }

		 nleft -= nwritten;
		 ptr += nwritten;
	}
	return (n);
}

static ssize_t						 /* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n)
{
	size_t  nleft;
	ssize_t nread;
	char   *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;	  /* and call read() again */
			else
				return (-1);
		} else if (nread == 0)
			break;			  /* EOF */

		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft);		 /* return >= 0 */
}



static int send_page(char* arg, int size)
{
	void *addr = (void*) atol(arg);	
	/*TODO: make sure it is the same page size on both arch!? */
	writen(server_sock_fd, addr, sysconf(_SC_PAGE_SIZE));
	return 0;
}

static int print_text(char* arg, int size)
{
	fwrite(arg, size, sizeof(char), stdout);
	return 0;
}


/* commands table */
cmd_func_t cmd_funcs[]  = {send_page, print_text};

int __handle_commands(int sockfd)
{
	int n;
	int size;
	enum comm_cmd cmd;
	char buff[MAX_NUM_CHAR_SIZE+1];

	printf("Entering function %s\n", __func__);

	n = readn(sockfd, buff, CMD_SIZE);
	if(n<0)
		perror("cmd_read");
	buff[n]='\0';
	cmd = (enum comm_cmd) atoi(buff);
	printf("%s: cmd read %d, %s\n", __func__, (int)cmd, buff);

	n = readn(sockfd, buff, ARG_SIZE_SIZE);
	if(n<0)
		perror("arg_size");
	buff[n]='\0';
	size = atoi(buff);
	printf("%s: size read %d, %s\n", __func__, (int)size, buff);

	char* arg = malloc(size+1);
	n = readn(sockfd, arg, size);
	if(n<0)
		perror("arg_size");

	arg[n]='\0';
	cmd_funcs[cmd](arg, size);

	return 0;
}

int handle_commands(int sockfd)
{
	printf("Entering function %s\n", __func__);
	server_sock_fd = sockfd;
	while(1){
		__handle_commands(sockfd);
	}
}

int send_cmd(enum comm_cmd cmd, char *arg, int size)
{
	int n;
	char buff[MAX_NUM_CHAR_SIZE+1];

	printf("sending a command\n");

	snprintf(buff, CMD_SIZE+1, "%d", (int) cmd);
	n = writen(ori_to_remote_sock, buff, CMD_SIZE);
	if(n<0)
		perror("cmd_write");
	printf("cmd written %s\n", buff);

	snprintf(buff, ARG_SIZE_SIZE+1, "%d", (int) size);
	n = writen(ori_to_remote_sock, buff, ARG_SIZE_SIZE);
	if(n<0)
		perror("arg_size  write");

	n = writen(ori_to_remote_sock, arg, size);
	if(n<0)
		perror("arg_size write");

	return 0;
}

int send_cmd_rsp(enum comm_cmd cmd, char *arg, int size, void* resp, int resp_size)
{

	send_cmd(cmd, arg, size);
	int n;

	n = readn(ori_to_remote_sock, resp, resp_size);
	if(n<0)
		perror("resp_read");

	return 0;
}

//TODO: don't always open a link see if one already exist?
// or do it in the origine_init funciton below
int comm_migrate(int nid)
{
	int sockfd = 0, n = 0;
	struct sockaddr_in serv_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Error : Could not create socket \n");
		return 1;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(DEFAULT_PORT);

	if(inet_pton(AF_INET, nodes[nid], &serv_addr.sin_addr)<=0)
	{
		printf("\n inet_pton error occured\n");
		return 1;
	}

	if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
	   printf("\n Error : Connect Failed \n");
	   return 1;
	}

	/* get the path to the binary. TODO: consider the arch extension */
	int ps;
	char *path = calloc(PATH_MAX, 1);//TODO: private malloc!
	ps = readlink("/proc/self/exe", path, PATH_MAX);
	if(ps==PATH_MAX)
		perror("path max");
	else
		printf("path is %s, size %ld\n", path, strlen(path));
	ps++;

	/* Write path size */
	char path_size[NUM_LINE_SIZE_BUF+1];
	//snprintf(path_size, NUM_LINE_SIZE_BUF, "%."NUM_LINE_SIZE_BUF_STRING"d",ps);
	snprintf(path_size, NUM_LINE_SIZE_BUF+1, "%.8d",ps);
	printf("pthsize ori %d %.9s\n", ps, path_size);
	n = writen(sockfd, path_size, NUM_LINE_SIZE_BUF);
	printf("\n %d bytes written\n", n);
	/* Write the path */
	n = writen(sockfd, path, ps);
	printf("\n %d bytes written\n", n);

	handle_commands(sockfd);

	//close(sockfd);//?
	return 0;
}

static void test()
{
	int ret;
        char msg[] = "Hello world from prog\n";
        ret = send_cmd(PRINT_ST, msg, strlen(msg));
        if(ret < 0)
                perror(__func__);

}

static int remote_init()
{
        char *cfd = getenv("POPCORN_SOCK_FD");

        ori_to_remote_sock = atoi(cfd);

        printf("%s: %d\n", __func__, ori_to_remote_sock);

	test();
        //close(fd);

        printf("%s: end\n", __func__);

	return 0;
}

static int origin_init()
{
	return 0;
}

int comm_init(int remote)
{
	if(remote)
		remote_init();
	else
		origin_init();
	return 0;
}

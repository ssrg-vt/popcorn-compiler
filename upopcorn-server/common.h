#pragma once
#include <errno.h>
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

#define str(s) #s
#define NUM_LINE_SIZE_BUF_STRING str(NUM_LINE_SIZE_BUF)
#define NUM_LINE_SIZE_BUF 8
#define DEFAULT_PORT 9999

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

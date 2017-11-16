#include <stdlib.h>
#include <stdio.h>

int dsm_init(int);
int comm_init(int);

void upopcorn_init()
{
        int ret;
	int remote;
        char *cfd = getenv("POPCORN_SOCK_FD");
        char *start_remote = getenv("POPCORN_REMOTE_START");

	if(start_remote)
                remote = atoi(start_remote);
        else
                remote = 0;

        ret = dsm_init(remote);
	if(ret)
		perror("dsm_init");
	comm_init(remote);
	if(ret)
		perror("comm_init");

}

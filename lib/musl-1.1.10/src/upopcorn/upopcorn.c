#include <stdlib.h>

int dsm_init(int);
int comm_init(int);

int upopcorn_init()
{
        int ret;
	int remote;
        char *cfd = getenv("POPCORN_SOCK_FD");
        char *start_remote = getenv("POPCORN_REMOTE_START");
	if(start_remote)
                remote = atoi(start_remote);
        else
                remote = 0;

        dsm_init(remote);

	comm_init(remote);
}

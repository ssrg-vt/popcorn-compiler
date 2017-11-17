#include <stdlib.h>
#include <stdio.h>

int dsm_init(int);
int comm_init(int);
int migrate_init(int);

//static void __attribute__((constructor)) __upopcorn_init(void);

//static void __attribute__((constructor)) 
void __upopcorn_init(void)
{
        int ret;
	int remote;
        char *start_remote = getenv("POPCORN_REMOTE_START");

	printf("%s start\n", __func__);
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
	migrate_init(remote);
	if(ret)
		perror("comm_init");
}

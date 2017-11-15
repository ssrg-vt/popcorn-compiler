#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include "pmparser.h"

#if 0
extern int _private_start, _private_end;
void *private_start = &_private_start;
void *private_end = &_private_end;
elif 1

#else
extern int __tdata_start, __tbss_end;
void *private_start = &__tdata_start;
void *private_end = &__tbss_end;
#endif

//extern int _sdata, _edata;
void *sdata = 0;//&_sdata;
void *edata = 0;//&_edata;

#define ERR_CHECK(func) if(func) perror(__LINE__);
int
dsm_protect(void *addr, unsigned long length)
{
	if(mprotect(addr, length, PROT_NONE))
			perror(__func__);
}

void fault_handler(int sig, siginfo_t *info, void *ucontext)
{
	//printf("%s: address %p\n", __func__, info->si_addr);
	procmap_t* map=NULL;
	void *addr=info->si_addr;
	pmparser_get(addr, &map, NULL);
	printf("%s: address %p\n", __func__, info->si_addr);
	if(mprotect(map->addr_start, map->length, PROT_READ| PROT_WRITE))
			perror(__func__);

}


int catch_signal()
{
	sigset_t set;
	struct sigaction sa;
	ERR_CHECK(sigemptyset(&set));
	ERR_CHECK(sigaddset(&set, SIGSEGV));

	sa.sa_sigaction = fault_handler;
	sa.sa_mask = set;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_restorer = NULL;

	ERR_CHECK(sigaction(SIGSEGV, &sa, NULL));

	return 0;
}

int dsm_init()
{
	int ret;
	procmap_t* map=NULL;

	//while(__hold) usleep(1000);

	printf("dsm_init private start %p, end %p\n", private_start, private_end);
	catch_signal();
	printf("dsm_init data start %p, end %p\n", sdata, edata);
	
	pmparser_init();

	ret = pmparser_parse(-1);
	if(ret){
		printf ("[map]: cannot parse the memory map of %d\n", getpid());
		return -1;
	}

	
	/* Set all writable regions as absent to make sure 	*
	 * that the content is fetched remotely. 		*/
	while((map=pmparser_next())!=NULL){
		pmparser_print(map,0);
		printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n"); 

		//if(map->addr_start<=sdata && map->addr_end>=edata)
		if(map->addr_start>=private_start && map->addr_end<=private_end)
		{
			/*
			if(map->addr_start == private_start)
				map->addr_start= private_end;//start the addr at the end of addr
			else if(map->addr_end == private_end)
				map->length-= private_end-private_start;//reduce the size
			else
				printf("error dsm private region\n");
			//assert(map->prot.is_w);
			*/
			printf("pdata section found ans skipped!\n");
			continue;

		}
		if(strstr(map->pathname, "stack") != NULL) {
			printf("stack section found ans skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "heap") != NULL) {
			printf("heap section found ans skipped!\n");
			continue;
		}
		if(map->prot.is_w)
			dsm_protect(map->addr_start, map->length);
		if(!map->prot.is_p)
			printf("Not prrivate region are not supported?\n");
	}
	
	printf("dsm_init done\n");

	return 0;
}

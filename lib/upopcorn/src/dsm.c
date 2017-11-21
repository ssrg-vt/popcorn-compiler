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
#include "config.h"
#include "communicate.h"


extern unsigned long __pmalloc_start;

extern int __tdata_start, __tbss_end;
void *private_start = &__tdata_start;
void *private_end = &__tbss_end;

#define ERR_CHECK(func) if(func) perror(__func__);
int
dsm_protect(void *addr, unsigned long length)
{
	if(mprotect(addr, length, PROT_NONE))
			perror(__func__);
	return 0;
}

int dsm_get_page(void* raddr, void* buffer, int page_size)
{
	char ca[NUM_LINE_SIZE_BUF+1];
	snprintf(ca, NUM_LINE_SIZE_BUF, "%ld", (long) raddr);
	printf("%s: %p == %s\n", __func__, raddr, ca);
	return send_cmd_rsp(GET_PAGE, ca, sizeof(ca), buffer, page_size);
}

int dsm_copy_stack(void* addr)
{
	printf("%s: address %p\n", __func__, addr);

	addr = PAGE_ALIGN(addr);

	printf("%s: aligned address %p\n", __func__, addr);

	if((mmap(addr, page_size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_GROWSDOWN,
		-1, 0) == MAP_FAILED))
			perror(__func__);

	/* Copy content from remote into the temporary page */
	dsm_get_page(addr, addr, page_size);

	printf("%s: done %p\n", __func__, addr);
	return 0;
}

#define CHECK_ERR(err) if(err) printf("%s:%d error!!!", __func__, __LINE__);

int dsm_get_map(void* addr, procmap_t **map, struct page_s **page)
{
	int err;
	procmap_t *new_map;
	err = pmparser_get(addr, map, NULL);
	if(!err)
		return 0;

	new_map = pmparser_new();

	char ca[NUM_LINE_SIZE_BUF+1];
	snprintf(ca, NUM_LINE_SIZE_BUF, "%ld", (long) addr);
	printf("%s: %p == %s, map %p map size %ld\n", __func__, addr, ca, new_map, sizeof(*new_map));
	err= send_cmd_rsp(GET_PMAP, ca, sizeof(ca),
				new_map, sizeof(*new_map));
	CHECK_ERR(err);
	pmparser_insert(new_map);

	printf("printing received pmap\n");
	pmparser_print(new_map, 0);

	mmap(new_map->addr_start, new_map->length, PROT_NONE,
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	return 0;
}

volatile int hold_real_fault=1;
void fault_handler(int sig, siginfo_t *info, void *ucontext)
{
	procmap_t* map=NULL;
	void *addr=info->si_addr;

	printf("%s: address %p\n", __func__, info->si_addr);
	if(addr == NULL)
	{
		while(hold_real_fault);
	}

	assert(PAGE_SIZE == page_size);

	addr = PAGE_ALIGN(addr);

	dsm_get_map(addr, &map, NULL);

	printf("%s: aligned address %p\n", __func__, addr);

	/*TODO: make the next two function atomic */
	if(mprotect(addr, page_size, PROT_READ | PROT_WRITE))
			perror(__func__);

	/* Copy content from remote into the temporary page */
	dsm_get_page(addr, addr, page_size);

	/*
	//Whole region!
	if(mprotect(map->addr_start, map->length, PROT_READ| PROT_WRITE))
			perror(__func__);
	*/

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

int dsm_init_pmap()
{
	int ret;

	pmparser_init();

	ret = pmparser_parse(-1);
	if(ret){
		printf ("[map]: cannot parse the memory map of %d\n", getpid());
		return -1;
	}
	return 0;
}

int dsm_init_remote()
{
	int ret;
	procmap_t* map=NULL;

	//while(__hold) usleep(1000);

	printf("dsm_init private start %p, end %p\n", private_start, private_end);
	catch_signal();

	dsm_init_pmap();

	printf("dsm_init pmalloc start %p\n", (void*)__pmalloc_start);

	/* Set all writable regions as absent to make sure 	*
	 * that the content is fetched remotely. 		*/
	while((map=pmparser_next())!=NULL){
		pmparser_print(map,0);
		printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n");

		if(map->addr_start>=private_start && map->addr_end<=private_end)
		{
			printf("pdata section found and skipped!\n");
			continue;

		}else
		{
			if(map->addr_start>=private_start && map->addr_start<=private_end)
			{
				printf("section start lie in the boundary of the private data!\n");
				printf("section skipped!\n");
				continue;
			}
			if(map->addr_end>=private_start && map->addr_end<=private_end)
			{
				printf("section end lie in the boundary of the private data!\n");
				printf("section skipped!\n");
				continue;
			}

		}
		if((unsigned long)map->addr_start == __pmalloc_start) {
			printf("pmalloc section found and skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "stack") != NULL) {
			printf("stack section found and skipped!\n");
			continue;
		}
		/*
		if(strstr(map->pathname, "heap") != NULL) {
			printf("heap section found and skipped!\n");
			continue;
		}*/

		if(map->prot.is_w)
			dsm_protect(map->addr_start, map->length);

		if(!map->prot.is_p)
			printf("Not prrivate region are not supported?\n");
	}

	printf("dsm_init done\n");

	return 0;

}

void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off);
void *mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	void* ret;
	ret = __mmap(start, len,prot, flags, fd, off);
	pmparser_free();
	dsm_init_pmap();
	return ret;
}

int dsm_init(int remote_start)
{
	printf("%s: remote start = %d\n", __func__, remote_start);
	if(remote_start)
                dsm_init_remote();
	else
		dsm_init_pmap();
	return 0;
}

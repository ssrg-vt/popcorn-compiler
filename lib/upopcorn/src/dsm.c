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
void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off);

extern int __tdata_start, __tbss_end;
void *private_start = &__tdata_start;
void *private_end = &__tbss_end;

#define ERR_CHECK(func) if(func) do{perror(__func__); exit(-1);}while(0)
int
dsm_protect(void *addr, unsigned long length)
{
	ERR_CHECK(mprotect(addr, length, PROT_NONE));
	return 0;
}

int dsm_get_page(void* raddr, void* buffer, int page_size)
{
	char ca[NUM_LINE_SIZE_BUF+1];
	snprintf(ca, NUM_LINE_SIZE_BUF, "%ld", (long) raddr);
	up_log("%s: %p == %s\n", __func__, raddr, ca);
	return send_cmd_rsp(GET_PAGE, ca, sizeof(ca), buffer, page_size);
}


#define CHECK_ERR(err) if(err) up_log("%s:%d error!!!", __func__, __LINE__);

int dsm_get_remote_map(void* addr, procmap_t **map, struct page_s **page)
{
	int err;
	procmap_t *new_map;

	new_map = pmparser_new();

	char ca[NUM_LINE_SIZE_BUF+1];
	snprintf(ca, NUM_LINE_SIZE_BUF, "%ld", (long) addr);//conflict with vfprintf?
	up_log("%s: %p == %s, map %p map size %ld\n", __func__, addr, ca, new_map, sizeof(*new_map));
	err= send_cmd_rsp(GET_PMAP, ca, sizeof(ca),
				new_map, sizeof(*new_map));
	CHECK_ERR(err);
	pmparser_insert(new_map, 0);//FIXME: should put node id

	up_log("printing received pmap\n");
	pmparser_print(new_map, 0);

	ERR_CHECK((__mmap(new_map->addr_start, new_map->length, PROT_NONE,
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)==MAP_FAILED));
	if(map)
		*map = new_map;
	return 0;
}


int dsm_get_map(void* addr, procmap_t **map, struct page_s **page)
{
	int err;
	err = pmparser_get(addr, map, NULL);
	if(!err)
		return 0;

	return dsm_get_remote_map(addr, map, page);
}

static void unprotect_and_load_page(void* addr)
{
	/*TODO: make the next two function atomic */
	ERR_CHECK(mprotect(addr, page_size, PROT_READ | PROT_WRITE));

	/* Copy content from remote into the temporary page */
	dsm_get_page(addr, addr, page_size);

	//up_log("%s: done %p\n", __func__, addr);
}

int dsm_copy_stack(void* addr)
{
	procmap_t* map=NULL;

	up_log("%s: address %p\n", __func__, addr);

	addr = PAGE_ALIGN(addr);

	up_log("%s: aligned address %p\n", __func__, addr);

	dsm_get_remote_map(addr, &map, NULL);

	/* Copy content from remote into the temporary page */
	//for(addr=map->addr_start; addr<map->addr_end; addr+=page_size)
	unprotect_and_load_page(addr);

	/*unprotect lower addresses of the stack: new pages are allocated locally
	 * These page are important for the fault handler to execute correctly */
	/* Does the stack transf. lib use part of the stack? lower part maybe ?*/ 
	ERR_CHECK(mprotect(map->addr_start, addr-map->addr_start, 
						PROT_READ | PROT_WRITE));

	up_log("%s: done %p\n", __func__, addr);
	return 0;
}

volatile int hold_real_fault=1;
void fault_handler(int sig, siginfo_t *info, void *ucontext)
{
	procmap_t* map=NULL;
	void *addr=info->si_addr;

	//up_log("%s: address %p\n", __func__, info->si_addr);
	if(addr == NULL)
	{
		while(hold_real_fault);
	}

	assert(PAGE_SIZE == page_size);

	addr = PAGE_ALIGN(addr);

	dsm_get_map(addr, &map, NULL);

	//up_log("%s: aligned address %p\n", __func__, addr);

	unprotect_and_load_page(addr);
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

	ret = pmparser_init();

	if(ret){
		up_log ("[map]: cannot parse the memory map of %d\n", getpid());
		return -1;
	}
	return 0;
}

int dsm_init_remote()
{
	int ret;
	procmap_t* map=NULL;

	up_log("dsm_init private start %p, end %p\n", private_start, private_end);

	catch_signal();

	dsm_init_pmap();

	up_log("dsm_init pmalloc start %p\n", (void*)__pmalloc_start);

	/* Set all writable regions as absent to make sure 	*
	 * that the content is fetched remotely. 		*/
	while((map=pmparser_next())!=NULL){
		pmparser_print(map,0);
		up_log("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n");

		if(map->addr_start>=private_start && map->addr_end<=private_end)
		{
			up_log("pdata section found and skipped!\n");
			continue;

		}else
		{
			if(map->addr_start>=private_start && map->addr_start<private_end)
			{
				up_log("section start lie in the boundary of the private data!\n");
				up_log("section skipped!\n");
				continue;
			}
			if(map->addr_end>private_start && map->addr_end<=private_end)
			{
				up_log("section end lie in the boundary of the private data!\n");
				up_log("section skipped!\n");
				continue;
			}

		}
		if((unsigned long)map->addr_start == __pmalloc_start) {
			up_log("pmalloc section found and skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "stack") != NULL) {
			up_log("stack section found and skipped!\n");
			continue;
		}
		/*
		if(strstr(map->pathname, "heap") != NULL) {
			up_log("heap section found and skipped!\n");
			continue;
		}*/

		if(map->prot.is_w)
			dsm_protect(map->addr_start, map->length);

		if(!map->prot.is_p)
			up_log("Not prrivate region are not supported?\n");
	}

	up_log("dsm_init done\n");

	return 0;

}

void *mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	void* ret;
	ret = __mmap(start, len,prot, flags, fd, off);
	pmparser_update();
	return ret;
}

int dsm_init(int remote_start)
{
	up_log("%s: remote start = %d\n", __func__, remote_start);
	if(remote_start)
                dsm_init_remote();
	else
		dsm_init_pmap();
	return 0;
}

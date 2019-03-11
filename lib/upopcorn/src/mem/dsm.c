#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
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
#include <poll.h>
#include "region_db.h"
#include "config.h"
#include "communicate.h"
#include "stack_move.h"

//#define USERFAULTFD

#ifdef USERFAULTFD
	#ifdef __x86_64__
		#define __NR_userfaultfd 323
	#elif __aarch64__
		#define __NR_userfaultfd 282
	#endif

	#include <linux/userfaultfd.h>
#endif

/* FIXME! check the use of usingned long/__u64/uint64_t */



static long uffd;		  /* userfaultfd file descriptor */
extern unsigned long __pmalloc_start;
extern unsigned long __malloc_start;
void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off);

extern int __tdata_start, __tbss_end;
void *private_start = &__tdata_start;
void *private_end = &__tbss_end;

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


uint64_t DSM_GET_START_AND_SIZE(uint64_t addr, uint64_t region_start, uint64_t region_end, uint64_t *size) 
{
	uint64_t tmp_start = addr;
	unsigned long tmp_end;
	up_log("%s: region_start %p, region_end %p\n", __func__, 
		(char*)region_start,
			(char*)region_end);

	/* Aligned start and end */
	tmp_start = (unsigned long) tmp_start & ~(DSM_PAGE_SIZE - 1);
	tmp_end = (tmp_start + DSM_PAGE_SIZE);
	up_log("%s: start %p, end %p\n", __func__, 
		(char*)tmp_start, (char*)tmp_end);

	/* limit to the region boundaries */
	tmp_start = max(tmp_start, region_start);
	tmp_end = min(tmp_end, region_end);
	up_log("%s: start %p, end %p\n", __func__, 
		(char*)tmp_start, (char*)tmp_end);

	/* update return values*/
	*size = tmp_end-tmp_start;
	up_log("%s: dst %p, size %ld, end %p\n", __func__, 
		(char*)tmp_start,  *size, 
			(char*)tmp_end);

	return tmp_start;

}

static int local_fault_cnt = 0;	 /* Number of faults so far handled */
static int remote_fault_cnt = 0;	 /* Number of faults so far handled */
struct page_exchange_s
{
	uint64_t address;
	uint64_t size;
};
int send_page(char* arg, int size, void* data)
{
	struct page_exchange_s *pes = (struct page_exchange_s *)arg;
	/*size is not page size but addr size in char */
	up_log("%s: ptr = %p , size %ld\n", __func__, (void*)pes->address, pes->size);
	send_data((void*)pes->address, pes->size, data);

	//set page  as not present
	{
		region_t *map;
		ERR_CHECK(region_db_get((void*)pes->address, (region_t**)&map));
		region_print(map);
		region_set_page(map, (void*)pes->address, pes->size, 1);
	}
	return 0;
}
int dsm_get_remote_page(void* raddr, void* buffer, int page_size)
{
	static struct page_exchange_s pes;
	pes.address=(uint64_t)raddr;
	pes.size=page_size;
	up_log("%s: ptr = %p , size %ld\n", __func__, (void*)pes.address, pes.size);
	remote_fault_cnt++;
	return send_cmd_rsp(GET_PAGE,  sizeof(pes), (char*)&pes, page_size, buffer);
}

int dsm_check_page_locally(region_t *map, void* addr, int page_size)
{
	if((!map->prot.is_w && !map->remote) || 
		/*read and not remote*/ (region_is_page_present(map, addr, page_size)))
	{
		local_fault_cnt++;
		up_log("%s: fetching page locally\n", __func__);
		ERR_CHECK(mprotect(addr, page_size, PROT_READ | PROT_WRITE));
	}
	return -1;
}


#ifdef USERFAULTFD
void userfaultfd_register(void* addr, uint64_t len){

	struct uffdio_register uffdio_register;
	
	up_log("UFFD register start is %p end is %p\n", addr,addr+len);

	/* Register the memory range of the mapping we just created for
	handling by the userfaultfd object. In mode, we request to track
	missing pages (i.e., pages that have not yet been faulted in). */

	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	ERR_CHECK(ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1);
}
#endif

int dsm_protect_region(region_t *new_map, int stack)
{
	/* Register region in the kernel */
#ifdef USERFAULTFD
	if(new_map->inode || stack)
	{
#endif
		//use segfault: do we ever reach here with an inode?
		ERR_CHECK((__mmap(new_map->addr_start, new_map->length, PROT_NONE,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0)==MAP_FAILED));
#ifdef USERFAULTFD
	}else
	{	//use userfaultfd
		ERR_CHECK((__mmap(new_map->addr_start, new_map->length, PROT_READ | PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0)==MAP_FAILED));
		userfaultfd_register((void*)new_map->addr_start, new_map->length);
	}
#endif
	return 0;
}

struct pmap_exchange_s
{
	uint64_t address;
};
int send_pmap(char* arg, int size, void* data)
{
	region_t *pmap;
	struct pmap_exchange_s *pms = (struct pmap_exchange_s *)arg;
	void *addr = (void*) pms->address;
	up_log("%s: ptr = %p , size %d\n", __func__, addr, size);
	if(region_db_get(addr, (region_t**)&pmap))
	{
		/* To avoid this redirection, we should update at each region creation:
		 * mmap (done in dsm), malloc, pmalloc (?), thread creation, ? */ 
		/* or assuming single-threaded app, just do it at migration? */
		region_db_update();
		if(region_db_get(addr, (region_t**)&pmap))
		{
			up_log("map not found!!!");
		}
	}
	up_log("%s: map = %p , size %ld\n", __func__, pmap, sizeof(region_t));
	pmap->remote=1;
	//if(!pmap->prot.is_w)//RO
	if(!pmap->region_pages)//tmp fix for stack
		region_init_pages(pmap, 1);
	send_data(pmap, sizeof(region_t), data);
	return 0;
}
int dsm_get_remote_map(void* addr, region_t **map, struct page_s **page, int stack)
{
	int err;
	region_t *new_map;
	struct pmap_exchange_s pms;

	new_map = region_new(1);
	pms.address = (uint64_t) addr;
	up_log("%s: addr %p, map %p map size %ld\n", __func__, addr, new_map, sizeof(*new_map));

	err = send_cmd_rsp(GET_PMAP, sizeof(pms), (void*)&pms,
				sizeof(*new_map), new_map);
	new_map->region_pages = NULL;/*remote pointer is invalid here */
	region_init_pages(new_map, 0);
	CHECK_ERR(err);
	region_db_insert(new_map, 0);//FIXME: should put node id

	up_log("printing received pmap\n");
	region_print(new_map);

	dsm_protect_region(new_map, stack);

	if(map)
		*map = new_map;
	return 0;
}

int dsm_get_map(void* addr, region_t **map, struct page_s **page)
{
	int err;
	err = region_db_get(addr, map);
	if(!err)
		return 0;

	return dsm_get_remote_map(addr, map, page, 0);
}


/* addr: faulting address; map: coresponding map; write: write fault or read */
static void unprotect_and_load_page(void* addr, region_t* map)
{
	uint64_t size;
	up_log("%s: loading %p\n", __func__, addr);

	addr = (void*) DSM_GET_START_AND_SIZE((uint64_t)addr, (uint64_t) map->addr_start, (uint64_t)map->addr_end, &size);

	if(!dsm_check_page_locally(map, addr, size))
		return;


	//TODO: support DSM_PAGE_SIZE
	/*TODO: make the next two function atomic */
	ERR_CHECK(mprotect(addr, size, PROT_READ | PROT_WRITE));

	/* Copy content from remote into the temporary page */
	dsm_get_remote_page(addr, addr, size);

	region_set_page(map, addr, size, 1);

	up_log("%s: done %p\n", __func__, addr);
}

int dsm_copy_stack(void* addr)
{
	region_t* map=NULL;

	up_log("%s: address %p\n", __func__, addr);

	addr = SYS_PAGE_ALIGN(addr);

	up_log("%s: aligned address %p\n", __func__, addr);

	dsm_get_remote_map(addr, &map, NULL, 1);

	set_thread_stack(map->addr_start, map->length);

	/* Copy content from remote into the temporary page */
	//for(addr=map->addr_start; addr<map->addr_end; addr+=page_size)
//#ifndef USERFAULTFD
	unprotect_and_load_page(addr, map);

	/*unprotect lower addresses of the stack: new pages are allocated locally
	 * These page are important for the fault handler to execute correctly */
	/* Does the stack transf. lib use part of the stack? lower part maybe ?*/ 
	ERR_CHECK(mprotect(map->addr_start, addr - map->addr_start, 
						PROT_READ | PROT_WRITE));
//#endif

	up_log("%s: done %p\n", __func__, addr);
	return 0;
}

#ifdef USERFAULTFD
static void uffd_test(void)
{
	int ret;
        char msg[] = "Hello world from UFFD thread\n";
	up_log("sending UFFD hello\n");
        ret = send_cmd(PRINT_ST, strlen(msg), msg);
        if(ret < 0)
                perror(__func__);
}

static volatile void* userfaultfd_stack_base=NULL;
static void *
fault_handler_thread(void *arg)
{
	void* sp = NULL;
	region_t* map=NULL;
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;
	size_t page_size;

	/* used to track the address of the stack */
	userfaultfd_stack_base=&sp;

	page_size = DSM_PAGE_SIZE;

	up_log("userfaultfd_stack_base is (%p), page size %d\n",
			userfaultfd_stack_base, page_size);

	/* Create a page that will be copied into the faulting region */

	if (page == NULL) {
		page = pmalloc(page_size);
		ERR_CHECK(!page);
	}

	uffd_test();

	/* Loop, handling incoming events on the userfaultfd
	   file descriptor */
	for (;;) {

		/* See what poll() tells us about the userfaultfd */
		struct pollfd pollfd;
		int nready;
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");

		up_log("\nfault_handler_thread():\n");
		up_log("	poll() returns: nready = %d; "
				"POLLIN = %d; POLLERR = %d\n", nready,
				(pollfd.revents & POLLIN) != 0,
				(pollfd.revents & POLLERR) != 0);

		/* Read an event from the userfaultfd */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			up_log("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}
		if (nread == -1)
			errExit("read");

		/* We expect only one kind of event; verify that assumption */

		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		/* Display info about the page-fault event */
		up_log("	UFFD_EVENT_PAGEFAULT event: ");
		up_log("flags = %llx; ", msg.arg.pagefault.flags);
		up_log("address = %llx\n", msg.arg.pagefault.address);


		/* We need the boundaries of the region */
		ERR_CHECK(region_db_get((void*)msg.arg.pagefault.address, &map));

		/* We need to handle page faults in units of pages(!).
		   So, round faulting address down to page boundary */
		uint64_t size;
		void* addr;
		addr= (void*)DSM_GET_START_AND_SIZE((uint64_t)msg.arg.pagefault.address, 
				(uint64_t) map->addr_start, (uint64_t) map->addr_end, &size);

		if(!dsm_check_page_locally(map, addr, size))//??? does it work on userfaultfd
			continue;
		
		/* get remote page content */
		dsm_get_remote_page((void*)addr, page, size);

		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) addr;
		uffdio_copy.len=size;

/*
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
										   ~(page_size - 1);
		unsigned long tmp_end = (uffdio_copy.dst + page_size);

		uffdio_copy.dst = max(uffdio_copy.dst, (unsigned long) map->addr_start);
		tmp_end = min((unsigned long)(map->addr_end), tmp_end);
        	uffdio_copy.len = (tmp_end-uffdio_copy.dst);
*/
		up_log("%s: dst %p, size %lld, end %p\n", __func__, 
			(char*)uffdio_copy.dst, uffdio_copy.len, 
				(char*)uffdio_copy.dst+uffdio_copy.len);

		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		up_log("(uffdio_copy.copy returned %lld)\n",
				uffdio_copy.copy);
	}
}
#endif

volatile int hold_real_fault=1;
void fault_handler(int sig, siginfo_t *info, void *ucontext)
{
	region_t* map=NULL;
	void *addr=info->si_addr;

	up_log("%s: address %p\n", __func__, info->si_addr);
	if(addr == NULL)
	{
		while(hold_real_fault);
	}

	//assert(PAGE_SIZE == page_size);

	addr = SYS_PAGE_ALIGN(addr);

	dsm_get_map(addr, &map, NULL);

	//up_log("%s: aligned address %p\n", __func__, addr);

#ifdef USERFAULTFD 
	/* use userfaultfd unless it's the stack or region backed by file */
	if((map->inode) || ((strstr(map->pathname, "stack") != NULL))) 
#endif
		unprotect_and_load_page(addr, map);
	
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


#ifdef USERFAULTFD
static pthread_attr_t tattr;
int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size);
void userfaultfd_init(void)
{
	up_log("%s: init...\n", __func__);


	struct uffdio_api uffdio_api;
	pthread_t thr;      /* ID of thread that handles page faults */
	/* Create and enable userfaultfd object */
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");

	/* Create a thread that will process the userfaultfd events */
	void *base;
	int ret;
#define STACK_SIZE (PTHREAD_STACK_MIN + 0x4000)
	base = (void *) pmalloc(STACK_SIZE+SYS_PAGE_SIZE);
	/* setting a new stack: size/address */
	ret = pthread_attr_init(&tattr);
	if (ret != 0) {
		errno = ret;
		errExit("pthread_create");
	}
	up_log("before alignement userfaultfd_stack_base is (%p)\n", base);
	base=SYS_PAGE_ALIGN((base+SYS_PAGE_SIZE));
	up_log("aligned userfaultfd_stack_base is (%p)\n", base);
	ret = pthread_attr_setstack(&tattr, base, STACK_SIZE);
	if (ret != 0) {
		errno = ret;
		errExit("pthread_create");
	}
	/* the actual creation */
	ret = pthread_create(&thr, &tattr, fault_handler_thread, NULL);
	if (ret != 0) {
		errno = ret;
		errExit("pthread_create");
	}
	up_log("%s: done init\n", __func__);
}
#else
void userfaultfd_init(void){}
#endif


static int
dsm_protect(void *addr, unsigned long length)
{
	ERR_CHECK(mprotect(addr, length, PROT_NONE));
	return 0;
}


#ifdef DSM_STOP_DEBUG
void dsm_stop_debug()
{
	static volatile int hold_remote_init=0;
	while(hold_remote_init)
		;
}
#else
void dsm_stop_debug(){}
#endif

int catch_mecanism_initialized=0;
int dsm_catch_fault()
{
	if(catch_mecanism_initialized)
	{
		return 0;
	}

	dsm_stop_debug();

	userfaultfd_init();

	/* Needed even when using USERFAULTFD, 
	 * we need to catch signal: absent region, 
	 * file backed region (...?) */
	catch_signal();

	catch_mecanism_initialized=1;
	return 0;

}

uintptr_t stack_get_pointer();
/* dsm_control over default region: protect if write and not private *
   remotely fetched region are always protected */
/* first_and_local: first time called locally (first time called across all nodes)*/
int dsm_control_access(int update, int first, int local)
{
	int skip_next=0;
	int ret;
	region_t* map=NULL;
	uintptr_t sp = stack_get_pointer();

	dsm_catch_fault();

	up_log("dsm_init private start %p, end %p\n", private_start, private_end);
	up_log("dsm_init pmalloc start %p\n", (void*)__pmalloc_start);
	up_log("dsm_init malloc start %p\n", (void*)__malloc_start);

	if(update)
		region_db_update();
		

	/* Set all writable regions as absent to make sure 	*
	 * that the content is fetched remotely. 		*/
	while((map=region_db_next())!=NULL)
	{
		int local_malloc_arena = 0;
		assert(map->prot.is_p && "shared region are not supported");

		if(!map->prot.is_w && !map->remote) {
			/*read and not remote*/
			/* we are skipping (at least): vvar, vsyscall and vdso */
			up_log("RO section found and skipped!\n");
			continue;
		}

		if(map->addr_start<=private_start && map->addr_end>=private_end)
		{
			up_log("pdata section found and skipped!\n");
			continue;

		}else
		{
			//Are the signs corrent < or >?
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
		if(((unsigned long) map->addr_start <= __pmalloc_start)
			&& ((unsigned long)map->addr_end > __pmalloc_start))
		{
			/* pmalloc region includes USERFAULTFD stack if used */
			local_malloc_arena = 1;
			up_log("pmalloc section found and protected? %s!\n", first?"no":"yes");
			continue;
		}

		if(((unsigned long) map->addr_start <= sp)
			&& ((unsigned long)map->addr_end > sp))
		{
			up_log("stack pointer found in region no protection!\n");
			continue;
		}
#if 1
		if((first) && ((unsigned long) map->addr_start <= __malloc_start)
			&& ((unsigned long)map->addr_end > __malloc_start))
		{
			up_log("malloc section found and skipped!\n");
			continue;
		}
#endif
		if(strstr(map->pathname, "stack") != NULL) {
			up_log("stack section found and skipped!\n");
			continue;
		}
		/*
		if(strstr(map->pathname, "heap") != NULL) {
			up_log("heap section found and skipped!\n");
			continue;
		}*/

		if(first)
			region_init_pages(map, (local || local_malloc_arena));/*done only if not already done */
		
		if(first && (local || local_malloc_arena))
			continue;// no protection needed
			
		up_log("Protecting start is %p end is %p\n", map->addr_start, 
							map->addr_start+map->length);
		up_log("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		region_print(map);
		up_log("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n");


		dsm_protect(map->addr_start, map->length);

	}

	up_log("dsm_init done\n");

	return 0;



}


int dsm_init_remote()
{
	region_db_init();
	dsm_control_access(0, 1, 0);
	return 0;
}


int dsm_init_local()
{
	//up_log("%s:%d\n", __func__, __LINE__);
	region_db_init();
	dsm_control_access(0, 1, 1);
	return 0;
}

int dsm_init(int remote_start)
{
	up_log("%s: remote_start = %d\n", __func__, remote_start);
	if(remote_start)
		dsm_init_remote();
	else
		dsm_init_local();
	return 0;
}

void *mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	void* ret;
	ret = __mmap(start, len,prot, flags, fd, off);
	region_db_update();//FIXME: do we need it?
	return ret;
}

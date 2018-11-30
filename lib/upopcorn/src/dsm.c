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
#include <linux/userfaultfd.h>
#include "pmparser.h"
#include "config.h"
#include "communicate.h"


#ifdef __x86_64__
	#define __NR_userfaultfd 323
#elif __aarch64__
	#define __NR_userfaultfd 282
#endif


#define USERFAULTFD


/* FIXME! check the use of usingned long/__u64/uint64_t */



static long uffd;		  /* userfaultfd file descriptor */
extern unsigned long __pmalloc_start;
void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off);

extern int __tdata_start, __tbss_end;
void *private_start = &__tdata_start;
void *private_end = &__tbss_end;

#define CHECK_ERR(err) if(err) up_log("%s:%d error!!!", __func__, __LINE__);
#define ERR_CHECK(func) if(func) do{perror(__func__); exit(-1);}while(0)
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


uint64_t DSM_UPDATE_START_SIZE(uint64_t addr, uint64_t region_start, uint64_t region_end, uint64_t *size) 
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

struct page_exchange_s
{
	uint64_t address;
	uint64_t size;
};
int send_page(char* arg, int size)
{
	struct page_exchange_s *pes = (struct page_exchange_s *)arg;
	/*size is not page size but addr size in char */
	up_log("%s: ptr = %p , size %ld\n", __func__, (void*)pes->address, pes->size);
	send_data((void*)pes->address, pes->size);
	return 0;
}
int dsm_get_page(void* raddr, void* buffer, int page_size)
{
	static struct page_exchange_s pes;
	pes.address=(uint64_t)raddr;
	pes.size=page_size;
	up_log("%s: ptr = %p , size %ld\n", __func__, (void*)pes.address, pes.size);
	return send_cmd_rsp(GET_PAGE,  sizeof(pes), (char*)&pes, page_size, buffer);
}

struct pmap_exchange_s
{
	uint64_t address;
};
int send_pmap(char* arg, int size)
{
	void *pmap;
	struct pmap_exchange_s *pms = (struct pmap_exchange_s *)arg;
	void *addr = (void*) pms->address;
	up_log("%s: ptr = %p , size %d\n", __func__, addr, size);
	if(pmparser_get(addr, (procmap_t**)&pmap, NULL))
	{
		/* To avoid this redirection, we should update at each region creation:
		 * mmap (done in dsm), malloc, pmalloc (?), thread creation, ? */ 
		/* or assuming single-threaded app, just do it at migration? */
		pmparser_update();
		if(pmparser_get(addr, (procmap_t**)&pmap, NULL))
		{
			up_log("map not found!!!");
		}
	}
	up_log("%s: map = %p , size %ld\n", __func__, pmap, sizeof(procmap_t));
	send_data(pmap, sizeof(procmap_t));
	return 0;
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

int dsm_get_remote_map(void* addr, procmap_t **map, struct page_s **page, int stack)
{
	int err;
	procmap_t *new_map;
	struct pmap_exchange_s pms;

	new_map = pmparser_new();
	pms.address = (uint64_t) addr;
	up_log("%s: addr %p, map %p map size %ld\n", __func__, addr, new_map, sizeof(*new_map));

	err= send_cmd_rsp(GET_PMAP, sizeof(pms), (void*)&pms,
				sizeof(*new_map), new_map);
	CHECK_ERR(err);
	pmparser_insert(new_map, 0);//FIXME: should put node id

	up_log("printing received pmap\n");
	pmparser_print(new_map, 0);


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

	return dsm_get_remote_map(addr, map, page, 0);
}



static void unprotect_and_load_page(void* addr, procmap_t* map)
{
	uint64_t size;
	up_log("%s: loading %p\n", __func__, addr);

	addr = (void*) DSM_UPDATE_START_SIZE((uint64_t)addr, (uint64_t) map->addr_start, (uint64_t)map->addr_end, &size);

	//TODO: support DSM_PAGE_SIZE
	/*TODO: make the next two function atomic */
	ERR_CHECK(mprotect(addr, size, PROT_READ | PROT_WRITE));

	/* Copy content from remote into the temporary page */
	dsm_get_page(addr, addr, size);

	up_log("%s: done %p\n", __func__, addr);
}

int dsm_copy_stack(void* addr)
{
	procmap_t* map=NULL;

	up_log("%s: address %p\n", __func__, addr);

	addr = SYS_PAGE_ALIGN(addr);

	up_log("%s: aligned address %p\n", __func__, addr);

	dsm_get_remote_map(addr, &map, NULL, 1);

	/* Copy content from remote into the temporary page */
	//for(addr=map->addr_start; addr<map->addr_end; addr+=page_size)
//#ifndef USERFAULTFD
	unprotect_and_load_page(addr, map);

	/*unprotect lower addresses of the stack: new pages are allocated locally
	 * These page are important for the fault handler to execute correctly */
	/* Does the stack transf. lib use part of the stack? lower part maybe ?*/ 
	ERR_CHECK(mprotect(map->addr_start, addr-map->addr_start, 
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
	procmap_t* map=NULL;
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;	 /* Number of faults so far handled */
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
		ERR_CHECK(pmparser_get((void*)msg.arg.pagefault.address, &map, NULL));

		/* We need to handle page faults in units of pages(!).
		   So, round faulting address down to page boundary */
		uint64_t size;
		void* addr;
		addr= (void*)DSM_UPDATE_START_SIZE((uint64_t)msg.arg.pagefault.address, 
				(uint64_t) map->addr_start, (uint64_t) map->addr_end, &size);

		/* get remote page content */
		dsm_get_page((void*)addr, page, size);
		fault_cnt++;

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
	procmap_t* map=NULL;
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

#ifdef USERFAULTFD //TODO: only if no inode and no stack
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
#endif


static int
dsm_protect(void *addr, unsigned long length)
{
	ERR_CHECK(mprotect(addr, length, PROT_NONE));
	return 0;
}

static volatile int hold_remote_init=0;
int dsm_init_remote()
{
	int skip_next=0;
	int ret;
	procmap_t* map=NULL;

	up_log("dsm_init private start %p, end %p\n", private_start, private_end);

	while(hold_remote_init);


#ifdef USERFAULTFD
	pmparser_parse_print();
	userfaultfd_init();
#endif

	dsm_init_pmap();

	catch_signal();


	up_log("dsm_init pmalloc start %p\n", (void*)__pmalloc_start);

	/* Set all writable regions as absent to make sure 	*
	 * that the content is fetched remotely. 		*/
	while((map=pmparser_next())!=NULL){

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
#ifdef USERFAULTFD
		while(!userfaultfd_stack_base);
		if(map->addr_start<=userfaultfd_stack_base && map->addr_end>=userfaultfd_stack_base)
		{
			up_log("userfaultfd_stack_base found and skipped! (%p)\n", userfaultfd_stack_base);
			skip_next=1;
			continue;
		}
		//if(skip_next) { skip_next=0; continue; }
#endif
		if(((unsigned long) map->addr_start <= __pmalloc_start)
			&& ((unsigned long)map->addr_end >= __pmalloc_start))
		//if((unsigned long)map->addr_start == __pmalloc_start) 
		{
			up_log("pmalloc section found and skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "stack") != NULL) {
			up_log("stack section found and skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "vvar") != NULL) {
			up_log("vvar section found and skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "vdso") != NULL) {
			up_log("vdso section found and skipped!\n");
			continue;
		}
		if(strstr(map->pathname, "vsyscall") != NULL) {
			up_log("vdso section found and skipped!\n");
			continue;
		}
		/*
		if(strstr(map->pathname, "heap") != NULL) {
			up_log("heap section found and skipped!\n");
			continue;
		}*/


		if(!map->prot.is_w) {
			up_log("Protecting start is %p end is %p\n", map->addr_start,map->addr_start+map->length);
			up_log("RO section found and skipped!\n");
			continue;
		}
			
		up_log("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		pmparser_print(map,0);
		up_log("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n");

		dsm_protect(map->addr_start, map->length);

		if(!map->prot.is_p)
			up_log("prrivate region are not supported?\n");
	}

	up_log("dsm_init done\n");

	return 0;

}

void *mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	void* ret;
	ret = __mmap(start, len,prot, flags, fd, off);
	pmparser_update();//FIXME: do we need it?
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

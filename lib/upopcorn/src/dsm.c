#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
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


//#define USERFAULTFD 0



long uffd;		  /* userfaultfd file descriptor */
extern unsigned long __pmalloc_start;
void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off);

extern int __tdata_start, __tbss_end;
void *private_start = &__tdata_start;
void *private_end = &__tbss_end;

#define ERR_CHECK(func) if(func) do{perror(__func__); exit(-1);}while(0)
#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)
int
dsm_protect(void *addr, unsigned long length)
{
	ERR_CHECK(mprotect(addr, length, PROT_NONE));
	return 0;
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
	//printf("%s: ptr = %p , size %ld\n", __func__, (void*)pes->address, pes->size);
	/*TODO: make sure it is the same page size on both arch!? */
	send_data((void*)pes->address, pes->size);
	return 0;
}
int dsm_get_page(void* raddr, void* buffer, int page_size)
{
	struct page_exchange_s pes;
	//char ca[NUM_LINE_SIZE_BUF+1];
	//snprintf(ca, NUM_LINE_SIZE_BUF, "%ld", (long) raddr);
	//up_log("%s: %p == %s\n", __func__, raddr, ca);
	pes.address=(uint64_t)raddr;
	pes.size=page_size;
	//printf("%s: ptr = %p , size %ld\n", __func__, (void*)pes.address, pes.size);
	return send_cmd_rsp(GET_PAGE,  sizeof(pes), (char*)&pes, page_size, buffer);
}

#ifdef USERFAULTFD
void userfaultfd_register(void* addr, uint64_t len){

	struct uffdio_register uffdio_register;

	/* Register the memory range of the mapping we just created for
	handling by the userfaultfd object. In mode, we request to track
	missing pages (i.e., pages that have not yet been faulted in). */

	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	ERR_CHECK(ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1);
}
#endif


#define CHECK_ERR(err) if(err) up_log("%s:%d error!!!", __func__, __LINE__);

int dsm_get_remote_map(void* addr, procmap_t **map, struct page_s **page)
{
	int err;
	procmap_t *new_map;

	new_map = pmparser_new();

	char ca[NUM_LINE_SIZE_BUF+1];
	snprintf(ca, NUM_LINE_SIZE_BUF, "%ld", (long) addr);//conflict with vfprintf?
	up_log("%s: %p == %s, map %p map size %ld\n", __func__, addr, ca, new_map, sizeof(*new_map));
	err= send_cmd_rsp(GET_PMAP, sizeof(ca), ca,
				sizeof(*new_map), new_map);
	CHECK_ERR(err);
	pmparser_insert(new_map, 0);//FIXME: should put node id

	up_log("printing received pmap\n");
	pmparser_print(new_map, 0);


#ifdef USERFAULTFD
	if(new_map->inode)
	{
#endif
		//use segfault: do we ever reach here with an inode?
		ERR_CHECK((__mmap(new_map->addr_start, new_map->length, PROT_NONE,
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)==MAP_FAILED));
#ifdef USERFAULTFD
	}else
	{	//use userfaultfd
		ERR_CHECK((__mmap(new_map->addr_start, new_map->length, PROT_READ | PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)==MAP_FAILED));
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

#ifdef USERFAULTFD
static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;	 /* Number of faults so far handled */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;
	size_t page_size;


	page_size = PAGE_SIZE;

	/* Create a page that will be copied into the faulting region */

	if (page == NULL) {
		page = malloc(PAGE_SIZE);
		ERR_CHECK(!page);
	}

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

		printf("\nfault_handler_thread():\n");
		printf("	poll() returns: nready = %d; "
				"POLLIN = %d; POLLERR = %d\n", nready,
				(pollfd.revents & POLLIN) != 0,
				(pollfd.revents & POLLERR) != 0);

		/* Read an event from the userfaultfd */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
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
		printf("	UFFD_EVENT_PAGEFAULT event: ");
		printf("flags = %llx; ", msg.arg.pagefault.flags);
		printf("address = %llx\n", msg.arg.pagefault.address);

		/* get remote page content */
		dsm_get_page((void*)msg.arg.pagefault.address, page, page_size);
		fault_cnt++;

		uffdio_copy.src = (unsigned long) page;

		/* We need to handle page faults in units of pages(!).
		   So, round faulting address down to page boundary */

		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
										   ~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		printf("		(uffdio_copy.copy returned %lld)\n",
				uffdio_copy.copy);
	}
}
#endif

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

#ifdef USERFAULTFD
void* userfaultfd_stack_base=NULL;
void userfaultfd_init(void)
{
	int stack_base;
	userfaultfd_stack_base=&stack_base;
	printf("%s: init...\n", __func__);


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
	int s = pthread_create(&thr, NULL, fault_handler_thread, NULL);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}
	printf("%s: done init\n", __func__);
}
#endif

volatile int hold_remote_init=1;
int dsm_init_remote()
{
	int ret;
	procmap_t* map=NULL;

	up_log("dsm_init private start %p, end %p\n", private_start, private_end);
	while(hold_remote_init);

#ifdef USERFAULTFD
	userfaultfd_init();
#endif

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
#ifdef USERFAULTFD
		if(map->addr_start>=userfaultfd_stack_base && map->addr_end<=userfaultfd_stack_base)
		{
			up_log("userfaultfd_stack_base found and skipped!\n");
			continue;
		}
#endif
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <string.h>
#include "communicate.h"

int upopcorn_node_id;
int dsm_init(int);
int comm_init(int);
int migrate_init(int);

unsigned long system_page_size=0;//Why do we need to init this variable for here to get in the lib sections?
char arch_nodes[POPCORN_NODE_MAX][IP_FIELD]; //= {"127.0.0.1", "127.0.0.1"};
int arch_type[POPCORN_NODE_MAX]; //= { X86_64, X86_64, AARCH64, AARCH64};

void __cyg_profile_func_enter(){}
void __cyg_profile_func_exit(){}

static void read_config()
{
	int i;
	int ret;
	char path[PATH_MAX];//safe?
	char atype[ARCH_FIELD];
	FILE *file;

	ret = snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"),
						POPCORN_CONFIG_FILE);
	if(ret == PATH_MAX)
		perror(__func__);

	printf("popcorn config path is %s\n", path);

	file = fopen(path, "r");
	if(!file)
		perror("fopen config");

	i=0;
	while(fscanf(file, "%[^;];%s\n", arch_nodes[i], atype)==2)
	{
		if(strcmp("AARCH64", atype)==0)
			arch_type[i] = AARCH64;
		else if(strcmp("X86_64", atype)==0)
			arch_type[i] = X86_64;
		else
			perror("unknown node!!!\n");

		if(i >= POPCORN_NODE_MAX)
		{
			perror("maximum number of nodes reached\n");
			break;
		}
		printf("machine id %d type %s(%d) and ip %s\n",
			i, atype, arch_type[i], arch_nodes[i]);
		i++;
	}

}

/* TODO: support more than two id. Using the config file is not an option if
 * we want to support multiple upopcorn instance on the same machine. */
void upopcorn_set_node_id(int remote)
{
	if(remote)
		upopcorn_node_id = 1;
	else
		upopcorn_node_id = 0;

}

/* Each instance should have its own slice of the virtual @ space: 10 GB */
#define MALLOC_SIZE (10UL<<30) /* 10 GB */
/* We start the slicing  the space a little (2 GB) after the end of bss */
#define MALLOC_OFFSET_SIZE (2UL<<30)	/* 2 GB */


/* Each instance should have its own slice of the virtual @ space: 4 GB */
#define PMALLOC_SIZE (10UL<<30) /* 4 GB */
/* We start the slicing  the space a little (80 GB) after the end of malloc (above) */
/* This makes the size of malloc 80 GB == 8 nodes max?*/
#define PMALLOC_OFFSET_SIZE (80UL<<30)	/* 80 GB */

extern char end;
void malloc_init(void* start);
void pmalloc_init(void* start);
void upopcorn_start_malloc()
{
	unsigned long malloc_slicing_start = (unsigned long)SYS_PAGE_ALIGN(&end);//FIXME?
	unsigned long malloc_start = malloc_slicing_start + MALLOC_OFFSET_SIZE +
				(MALLOC_SIZE*upopcorn_node_id);
	malloc_init((void*)malloc_start);

	/*
	unsigned long pmalloc_slicing_start = (unsigned long)PAGE_ALIGN(malloc_start);
	unsigned long pmalloc_start = pmalloc_slicing_start + PMALLOC_OFFSET_SIZE +
				(PMALLOC_SIZE*upopcorn_node_id);
	pmalloc_init((void*)pmalloc_start);
	*/
}

//static void __attribute__((constructor)) __upopcorn_init(void);

volatile static int __hold_upop=0;
static int remote;
//void __upopcorn_destroy(void);
static void __attribute__((constructor)) 
//void 
__upopcorn_init(void)
{
        int ret;
        char *start_remote = getenv("POPCORN_REMOTE_START");

	system_page_size = sysconf(_SC_PAGE_SIZE);

	printf("%s start\n", __func__);
	if(start_remote)
                remote = atoi(start_remote);
        else
                remote = 0;

	if(remote)
		while(__hold_upop);

	upopcorn_set_node_id(remote);

	read_config();

	upopcorn_start_malloc();

	ret=comm_init(remote);
	if(ret)
		perror("comm_init");
        ret = dsm_init(remote);
	if(ret)
		perror("dsm_init");
	ret=migrate_init(remote);
	if(ret)
		perror("comm_init");

	//atexit(__upopcorn_destroy);
}

static void __attribute__((destructor)) 
//void 
__upopcorn_destroy(void)
{
	if(remote)
	{
		printf("sending exit...");
		send_cmd(SND_EXIT, 0, NULL);
	}
}

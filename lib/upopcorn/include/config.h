#pragma once

#ifdef __x86_64__
#include "arch/x86_64/migrate.h"
#elif defined __aarch64__
#include "arch/aarch64/migrate.h"
#else
# error Unknown/unsupported architecture!
#endif

#include <stack_transform.h>
//#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <sched.h>
#endif

typedef union{
		struct regset_aarch64 aarch;
		struct regset_x86_64 x86;
} regs_t;

#ifdef __cplusplus
}
#endif

#define POPCORN_CONFIG_FILE ".popcorn" /* file should be in HOME directory */
#define POPCORN_NODE_MAX 16
#define PATH_MAX 4096

/* Supported architectures */
enum arch {
  AARCH64 = 0,
  X86_64,
  NUM_ARCHES
};


#define IP_FIELD 16
#define ARCH_FIELD 12
extern char arch_nodes[POPCORN_NODE_MAX][IP_FIELD]; //= {"127.0.0.1", "127.0.0.1"};
extern int arch_type[POPCORN_NODE_MAX]; //= { X86_64, X86_64, AARCH64, AARCH64};

#define DSM_PAGE_SIZE 65536 
//#define DSM_PAGE_SIZE system_page_size
extern unsigned long system_page_size;

/* Just a security to make sure that both architecture has the same page size.
 * We should be able to support other page sizes if we send the page size to the
 * send_page function rather than the size of the address. TODO */
#define SYS_PAGE_SIZE 4096

#define ALIGN(_arg, _size) ((((unsigned long)_arg)/_size)*_size)
#define SYS_PAGE_ALIGN(_arg) (void*)ALIGN(_arg, system_page_size)


#define up_log(...) printf(__VA_ARGS__)
#define CHECK_ERR(err) if(err) up_log("%s:%d error!!!", __func__, __LINE__);
#define ERR_CHECK(func) if(func) do{perror(__func__); exit(-1);}while(0)

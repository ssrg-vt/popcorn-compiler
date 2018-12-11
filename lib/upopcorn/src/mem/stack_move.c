#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/mman.h>

#include "region_db.h"
#include "migrate.h"
#include "config.h"
#include "communicate.h"
#include "stack_move.h"

#define ERR_CHECK(func) if(func) do{perror(__func__); exit(-1);}while(0)
void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off);

#ifndef ALL_STACK_BASE
#define ALL_STACK_BASE 0x600000000000UL
#endif
#ifndef ALL_STACK_ALIGN
#define ALL_STACK_ALIGN
#endif

void* get_new_stack(unsigned long len)
{
	unsigned long ret;
	static unsigned long stack_base=ALL_STACK_BASE;

	len=(len+(len-1)) & ~(len-1);
	ERR_CHECK((__mmap((void*)stack_base, len, PROT_READ | PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0)==MAP_FAILED));
	ret=stack_base;
	stack_base+=len;
	return (void*)ret;

}

region_t *get_stack_pmp()
{
	int skip_next=0;
	int ret;
	region_t* map=NULL;

	region_db_init();


	while((map=region_db_next())!=NULL)
	{
		if(strstr(map->pathname, "stack") != NULL) {
			return map;
		}
	}
	return NULL;
}

int switch_stack(void* stack_base, void *new_stack_base, unsigned long len)
{
        unsigned long bp,sp;
        unsigned long new_bp,new_sp;
        unsigned long bp_offset,sp_offset;
        void* stack_end;
        void* new_stack_end;

        stack_end=(void*)((unsigned long)stack_base+len);

        GET_FRAME(bp,sp);

        printf("%s: stack_base %p stack frame %p and base %p\n", __func__,stack_base,(void*)bp,(void*)sp);

        bp_offset = (unsigned long)stack_end - bp;
        sp_offset = (unsigned long)stack_end - sp;

        printf("%s: stack_end %p stack frame off %p and base off %p\n", __func__,stack_end,(void*)bp_offset,(void*)sp_offset);

        memcpy(new_stack_base, stack_base, len);//TODO: copy up to sp


        new_stack_end=(void*)((unsigned long)new_stack_base+len);
        new_bp = (unsigned long)new_stack_end-bp_offset;
        new_sp = (unsigned long)new_stack_end-sp_offset;


        printf("%s: new stack base %p; new stack end %p; new stack frame %p and sp %p\n", __func__,new_stack_base,new_stack_end,(void*)new_bp,(void*)new_sp);

        SET_FRAME(new_bp,new_sp);

        return 0;
}

extern void* __popcorn_stack_base;
uintptr_t _upopcorn_stack_base;
uintptr_t _upopcorn_stack_size;

static int set_thread_stack(void *base, unsigned long len)
{
	_upopcorn_stack_base = (uintptr_t)base;
	_upopcorn_stack_size = (uintptr_t)len;
	return -1;
}

uintptr_t new_arg_addr(uintptr_t arg, uintptr_t old, uintptr_t new)
{
	arg=new+(arg-old);
	return arg;
}

void print_stack_info()
{
	int dummy;
	dummy+=1;

	printf("stack arg addr %p\n", &dummy);

}


void print_info(char **argv, char **envp)
{
	printf("argv %p; envp %p\n", argv, envp);

	{
		void *stack;
		size_t stack_size;
		pthread_attr_t attr;
		int ret;

		ret = pthread_getattr_np(pthread_self(), &attr);
		ret |= pthread_attr_getstack(&attr, &stack, &stack_size);
		if(ret == 0)
		{
			printf("pthread stack %p; stack_size %lu\n", stack, stack_size);
		}
	}

	{

		region_t *map = get_stack_pmp();
		printf("map stack %p; stack_size %lu\n", map->addr_start, map->length);
	}

}

int stack_move()
{
#if 0
	printf("%s:%d\n", __func__, __LINE__);
	region_t *map = get_stack_pmp();

        void* new_stack=get_new_stack(map->length);

	__popcorn_stack_base=(void*)new_arg_addr((uintptr_t)__popcorn_stack_base, (uintptr_t)map->addr_start, (uintptr_t)new_stack);

	print_stack_info();
	int ret=switch_stack(map->addr_start, new_stack, map->length);
	print_stack_info();

	set_thread_stack(new_stack, map->length);

	return ret;
#endif
	return 0;
}



#ifdef STACK_AND_ARGV_RELOCATION
/* Attempt to move argv, envp and stack at once */
/* PB: stack trnasformation assume that main is in the applications (stack metadata)... */
int real_main(int argc, char **argv, char **envp);
int main(int argc, char **argv, char **envp)
{
	print_info(argv, envp);
	uintptr_t base=stack_move();
	region_t *map = get_stack_pmp();
	argv=new_arg_addr(argv, map->addr_start, base);
	envp=new_arg_addr(envp, map->addr_start, base);
	real_main(argc, argv, envp);
}
#endif

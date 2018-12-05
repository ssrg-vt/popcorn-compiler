#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/mman.h>

#include "pmparser.h"
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

procmap_t *get_stack_pmp()
{
	int skip_next=0;
	int ret;
	procmap_t* map=NULL;

	pmparser_init();


	while((map=pmparser_next())!=NULL)
	{
		if(strstr(map->pathname, "stack") != NULL) {
			return map;
		}
	}
	return NULL;
}

int switch_stack(void* stack_base, void *new_stack, unsigned long len)
{
        unsigned long bp,sp;
        unsigned long new_bp,new_sp;
        unsigned long bp_offset,sp_offset;
        void* stack_end;

        stack_end=(void*)((unsigned long)stack_base-len);

        GET_FRAME(bp,sp);

        printf("%s: stack_base %p stack frame %p and base %p\n", __func__,stack_base,(void*)bp,(void*)sp);

        bp_offset = bp - (unsigned long)stack_base;
        sp_offset = sp - (unsigned long)stack_base;

        printf("%s: stack_end %p stack frame off %p and base off %p\n", __func__,stack_end,(void*)bp_offset,(void*)sp_offset);

        memcpy(new_stack, stack_base, len);//TODO: copy up to sp


        new_bp = (unsigned long)new_stack+bp_offset;
        new_sp = (unsigned long)new_stack+sp_offset;


        printf("%s: new stack base %p; new stack frame %p and sp %p\n", __func__,new_stack,(void*)new_bp,(void*)new_sp);

        SET_FRAME(new_bp,new_sp);

        return 0;
}

static int set_thread_stack(void *base, unsigned long len)
{
  pthread_attr_t attr;
  int ret;
  int retval;
  void *tb;
  unsigned long tl;

  ret = pthread_getattr_np(pthread_self(), &attr);
  ret |= pthread_attr_getstack(&attr, &tb, &tl);
  printf("%s: stack_base %p len %lu\n", __func__,tb,tl);
  ret |= pthread_attr_setstack(&attr, base, len);
  retval = ret;
  return retval;
}


int stack_move()
{
	/*
	printf("%s:%d\n", __func__, __LINE__);
	procmap_t *map = get_stack_pmp();

        void* new_stack=get_new_stack(map->length);

	set_thread_stack(map->addr_start, map->length);
	return switch_stack(map->addr_start, new_stack, map->length);
	*/
}

#ifdef TEST_STACK_MOV
int main()
{
	stack_move();
}
#endif

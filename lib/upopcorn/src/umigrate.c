#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "migrate.h"
#include "communicate.h"
#include "config.h"
#include <dsm.h>
#include <stack_transform.h>


#ifdef _TIME_REWRITE
#define TIME_REWRITE_START() \
		struct timespec start, end;		\
		unsigned long start_ns, end_ns;		\
		clock_gettime(CLOCK_MONOTONIC, &start);
#define TIME_REWRITE_END()				\
		clock_gettime(CLOCK_MONOTONIC, &end);	\
		start_ns = start.tv_sec * 1000000000 + start.tv_nsec;	\
		end_ns = end.tv_sec * 1000000000 + end.tv_nsec;		\
		printf("Stack transformation time: %ldns\n", end_ns - start_ns);
#else
#define TIME_REWRITE_START() /**/
#define TIME_REWRITE_END() /**/

#endif
regs_t regs_dst;
int get_context(void **ctx, int *size)
{
	*ctx = (void*)&regs_dst;
	*size = sizeof(regs_dst);
	return 0;
}

/* Check & invoke migration if requested. */
//__thread  TODO
int loading=0;//to distinguish between migrate and load
static void inline __new_migrate(int nid)
{
	printf("%s: entering\n", __func__);
	if(loading) // Post-migration
	{
		printf("%s: loading context\n", __func__);
		loading=0;
		return;
	}

	//Pre-migration
	const int dst_arch = arch_type[nid];
	unsigned long sp = 0, bp = 0;

	regs_t regs_src;
	GET_LOCAL_REGSET(regs_src);

	TIME_REWRITE_START()

	if (REWRITE_STACK) {
		fprintf(stderr, "Could not rewrite stack!\n");
		return;
	}

	TIME_REWRITE_END()

	if (dst_arch == X86_64) {
		regs_dst.x86.rip = __new_migrate;
		sp = (unsigned long)regs_dst.x86.rsp;
		bp = (unsigned long)regs_dst.x86.rbp;
	} else if (dst_arch == AARCH64) {
		regs_dst.aarch.pc = __new_migrate;
		sp = (unsigned long)regs_dst.aarch.sp;
		bp = (unsigned long)regs_dst.aarch.x[29];
		int i;
		for(i=0; i<32;i++)
			printf("x[%d]=%lx\n", i, regs_dst.aarch.x[i]);
	} else {
		assert(0 && "Unsupported architecture!");
	}

	comm_migrate(nid);

	assert(0 && "Couldn't migrate!");
}

void new_migrate(int nid)
{
	loading = 0;
	__new_migrate(nid);
}

/*
static void __load_context(regs_t *regs)
{
}
*/

volatile int __hold=0;
static void load_context()
{
	int ret;
	regs_t regs;
	
	while(__hold);

	printf("%s: sending cmd...\n", __func__);
	ret = send_cmd_rsp(GET_CTXT, NULL, 0, &regs, sizeof(regs));
	if(ret)
		perror(__func__);
	printf("%s: response received\n", __func__);
	//__load_context(&regs);

	unsigned long sp = 0, bp = 0;
	#ifdef __x86_64__
		sp = (unsigned long)regs.x86.rsp;
		bp = (unsigned long)regs.x86.rbp;
	#elif defined(__aarch64__)
		sp = (unsigned long)regs.aarch.sp;
		bp = (unsigned long)regs.aarch.x[29];
		int i;
		for(i=0; i<32;i++)
			printf("x[%d]=%lx\n", i, regs.aarch.x[i]);
	#endif


	printf("%s: copiying stack... %p\n", __func__,(void*)sp);
	dsm_copy_stack((void*)sp);
	printf("%s: stack received\n", __func__);

	loading =1;
	printf("%s: setting the frame received\n", __func__);

#ifdef __x86_64__
	SET_REGS_X86_64(regs.x86);//gp and fp registers
	SET_FRAME(bp, sp);
    	SET_IP_IMM(__new_migrate);
#elif defined(__aarch64__)
	SET_FP_REGS_NOCLOBBER_AARCH64(regs.aarch);
	register unsigned long arm_regs asm("x9");
	register unsigned long arm_pc asm("x10");
	register unsigned long arm_sp asm("x11");

	arm_regs = (unsigned long)regs.aarch.x;
	arm_pc = (unsigned long)regs.aarch.pc;
	arm_sp = (unsigned long)regs.aarch.sp;

/*
	asm volatile(	"ldr x9, [%0]\n" 
			"ldr x10, [%2]\n" //pc
			"ldr x11, [%1]; mov sp, x11\n" //sp
*/

	asm volatile( 
			"ldr x0 , [%0, 0 *8]\n" 
			"ldr x1 , [%0, 1 *8]\n" 
			"ldr x2 , [%0, 2 *8]\n" 
			"ldr x3 , [%0, 3 *8]\n" 
			"ldr x4 , [%0, 4 *8]\n" 
			"ldr x5 , [%0, 5 *8]\n" 
			"ldr x6 , [%0, 6 *8]\n" 
			"ldr x7 , [%0, 7 *8]\n" 
			"ldr x8 , [%0, 8 *8]\n" 
			/*"ldr %0 , [%0, 9 *8]\n" used as temp register*/
			/*"ldr x10, [%0, 10*8]\n" */
			/*"ldr x11, [%0, 11*8]\n" */
			"ldr x12, [%0, 12*8]\n" 
			"ldr x13, [%0, 13*8]\n" 
			"ldr x14, [%0, 14*8]\n" 
			"ldr x15, [%0, 15*8]\n" 
			"ldr x16, [%0, 16*8]\n" 
			"ldr x17, [%0, 17*8]\n" 
			"ldr x18, [%0, 18*8]\n" 
			"ldr x19, [%0, 19*8]\n" 
			"ldr x20, [%0, 20*8]\n" 
			"ldr x21, [%0, 21*8]\n" 
			"ldr x22, [%0, 22*8]\n" 
			"ldr x23, [%0, 23*8]\n" 
			"ldr x24, [%0, 24*8]\n" 
			"ldr x25, [%0, 25*8]\n" 
			"ldr x26, [%0, 26*8]\n" 
			"ldr x27, [%0, 27*8]\n" 
			"ldr x28, [%0, 28*8]\n" 
			"ldr x29, [%0, 29*8]\n" 
			"ldr x30, [%0, 30*8]\n" 
			//"mov x9 , 0\n" /*no need no more: temp register*/
			"mov sp, %2\n"
			//"mov x11 , 0\n" /*no need no more: temp register*/
			"br %1\n" 
			: : "r" (arm_regs), "r" (arm_pc), "r" (arm_sp));
/*
    	SET_REGS_AARCH64(regs.aarch);
	SET_FRAME(regs.aarch.x[29], regs.aarch.sp);
    	SET_IP_IMM(__new_migrate);
*/
#endif
	return;
}

static int origin_init()
{
	return 0;
}

int migrate_init(int remote)
{
	if(remote)
		load_context();
	else
		origin_init();
	return 0;
}

#if 0
int main(int argc, char* argv[])
{
	regs_t regs_src;
	register int test=0xdacadac;

	printf("%s: before test %x\n", __func__, test);
	GET_LOCAL_REGSET(regs_src);
	SET_REGS_PTR(&regs_src);
	printf("%s: after test %x\n", __func__, test);
}
#endif

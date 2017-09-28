# 1 "src/ucontext/x86_64/getcontext.S"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 1 "<command-line>" 2
# 1 "src/ucontext/x86_64/getcontext.S"
# 22 "src/ucontext/x86_64/getcontext.S"
# 1 "src/ucontext/x86_64/ucontext_i.h" 1

# 1 "./include/signal.h" 1







# 1 "./include/features.h" 1
# 9 "./include/signal.h" 2
# 28 "./include/signal.h"
# 1 "./arch/x86_64/bits/alltypes.h" 1
# 55 "./arch/x86_64/bits/alltypes.h"
typedef long time_t;
# 66 "./arch/x86_64/bits/alltypes.h"
typedef struct { union { int __i[14]; volatile int __vi[14]; unsigned long __s[7]; } __u; } pthread_attr_t;
# 101 "./arch/x86_64/bits/alltypes.h"
typedef unsigned long size_t;
# 260 "./arch/x86_64/bits/alltypes.h"
typedef long clock_t;
# 270 "./arch/x86_64/bits/alltypes.h"
struct timespec { time_t tv_sec; long tv_nsec; };





typedef int pid_t;
# 286 "./arch/x86_64/bits/alltypes.h"
typedef unsigned uid_t;
# 314 "./arch/x86_64/bits/alltypes.h"
typedef struct __pthread * pthread_t;
# 374 "./arch/x86_64/bits/alltypes.h"
typedef struct __sigset_t { unsigned long __bits[128/sizeof(long)]; } sigset_t;
# 29 "./include/signal.h" 2
# 81 "./include/signal.h"
typedef struct sigaltstack stack_t;

union sigval {
 int sival_int;
 void *sival_ptr;
};

typedef struct {
 int si_signo, si_errno, si_code;
 union {
  char __pad[128 - 2*sizeof(int) - sizeof(long)];
  struct {
   union {
    struct {
     pid_t si_pid;
     uid_t si_uid;
    } __piduid;
    struct {
     int si_timerid;
     int si_overrun;
    } __timer;
   } __first;
   union {
    union sigval si_value;
    struct {
     int si_status;
     clock_t si_utime, si_stime;
    } __sigchld;
   } __second;
  } __si_common;
  struct {
   void *si_addr;
   short si_addr_lsb;
   struct {
    void *si_lower;
    void *si_upper;
   } __addr_bnd;
  } __sigfault;
  struct {
   long si_band;
   int si_fd;
  } __sigpoll;
  struct {
   void *si_call_addr;
   int si_syscall;
   unsigned si_arch;
  } __sigsys;
 } __si_fields;
} siginfo_t;
# 150 "./include/signal.h"
struct sigaction {
 union {
  void (*sa_handler)(int);
  void (*sa_sigaction)(int, siginfo_t *, void *);
 } __sa_handler;
 sigset_t sa_mask;
 int sa_flags;
 void (*sa_restorer)(void);
};



struct sigevent {
 union sigval sigev_value;
 int sigev_signo;
 int sigev_notify;
 void (*sigev_notify_function)(union sigval);
 pthread_attr_t *sigev_notify_attributes;
 char __pad[56-3*sizeof(long)];
};





int __libc_current_sigrtmin(void);
int __libc_current_sigrtmax(void);




int kill(pid_t, int);

int sigemptyset(sigset_t *);
int sigfillset(sigset_t *);
int sigaddset(sigset_t *, int);
int sigdelset(sigset_t *, int);
int sigismember(const sigset_t *, int);

int sigprocmask(int, const sigset_t *__restrict, sigset_t *__restrict);
int sigsuspend(const sigset_t *);
int sigaction(int, const struct sigaction *__restrict, struct sigaction *__restrict);
int sigpending(sigset_t *);
int sigwait(const sigset_t *__restrict, int *__restrict);
int sigwaitinfo(const sigset_t *__restrict, siginfo_t *__restrict);
int sigtimedwait(const sigset_t *__restrict, siginfo_t *__restrict, const struct timespec *__restrict);
int sigqueue(pid_t, int, const union sigval);

int pthread_sigmask(int, const sigset_t *__restrict, sigset_t *__restrict);


void psiginfo(const siginfo_t *, const char *);
void psignal(int, const char *);




int killpg(pid_t, int);
int sigaltstack(const stack_t *__restrict, stack_t *__restrict);
int sighold(int);
int sigignore(int);
int siginterrupt(int, int);
int sigpause(int);
int sigrelse(int);
void (*sigset(int, void (*)(int)))(int);
# 229 "./include/signal.h"
typedef void (*sig_t)(int);
# 243 "./include/signal.h"
# 1 "./arch/x86_64/bits/signal.h" 1
# 36 "./arch/x86_64/bits/signal.h"
typedef long long greg_t, gregset_t[23];
typedef struct _fpstate {
 unsigned short cwd, swd, ftw, fop;
 unsigned long long rip, rdp;
 unsigned mxcsr, mxcr_mask;
 struct {
  unsigned short significand[4], exponent, padding[3];
 } _st[8];
 struct {
  unsigned element[4];
 } _xmm[16];
 unsigned padding[24];
} *fpregset_t;
struct sigcontext {
 unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
 unsigned long rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip, eflags;
 unsigned short cs, gs, fs, __pad0;
 unsigned long err, trapno, oldmask, cr2;
 struct _fpstate *fpstate;
 unsigned long __reserved1[8];
};
typedef struct {
 gregset_t gregs;
 fpregset_t fpregs;
 unsigned long long __reserved1[8];
} mcontext_t;






struct sigaltstack {
 void *ss_sp;
 int ss_flags;
 size_t ss_size;
};

typedef struct __ucontext {
 unsigned long uc_flags;
 struct __ucontext *uc_link;
 stack_t uc_stack;
 mcontext_t uc_mcontext;
 sigset_t uc_sigmask;
 unsigned long __fpregs_mem[64];
} ucontext_t;
# 244 "./include/signal.h" 2





typedef int sig_atomic_t;

void (*signal(int, void (*)(int)))(int);
int raise(int);
# 3 "src/ucontext/x86_64/ucontext_i.h" 2
# 23 "src/ucontext/x86_64/getcontext.S" 2
# 34 "src/ucontext/x86_64/getcontext.S"
ENTRY(__getcontext)


 movq %rbx, offsetof (ucontext_t, uc_mcontext.gregs[REG_RBX])(%rdi)
 movq %rbp, offsetof (ucontext_t, uc_mcontext.gregs[REG_RBP])(%rdi)
 movq %r12, offsetof (ucontext_t, uc_mcontext.gregs[REG_R12])(%rdi)
 movq %r13, offsetof (ucontext_t, uc_mcontext.gregs[REG_R13])(%rdi)
 movq %r14, offsetof (ucontext_t, uc_mcontext.gregs[REG_R14])(%rdi)
 movq %r15, offsetof (ucontext_t, uc_mcontext.gregs[REG_R15])(%rdi)

 movq %rdi, offsetof (ucontext_t, uc_mcontext.gregs[REG_RDI])(%rdi)
 movq %rsi, offsetof (ucontext_t, uc_mcontext.gregs[REG_RSI])(%rdi)
 movq %rdx, offsetof (ucontext_t, uc_mcontext.gregs[REG_RDX])(%rdi)
 movq %rcx, offsetof (ucontext_t, uc_mcontext.gregs[REG_RCX])(%rdi)
 movq %r8, offsetof (ucontext_t, uc_mcontext.gregs[REG_R8])(%rdi)
 movq %r9, offsetof (ucontext_t, uc_mcontext.gregs[REG_R9])(%rdi)

 movq (%rsp), %rcx
 movq %rcx, offsetof (ucontext_t, uc_mcontext.gregs[REG_RIP])(%rdi)
 leaq 8(%rsp), %rcx
 movq %rcx, offsetof (ucontext_t, uc_mcontext.gregs[REG_RSP])(%rdi)





 leaq offsetof (ucontext_t, __fpregs_mem)(%rdi), %rcx
 movq %rcx, offsetof (ucontext_t, uc_mcontext.fpregs)(%rdi)

 fnstenv (%rcx)
 fldenv (%rcx)
 stmxcsr offsetof (ucontext_t, __fpregs_mem.mxcsr)(%rdi)



 leaq offsetof (ucontext_t, uc_sigmask)(%rdi), %rdx
 xorl %esi,%esi

 xorl %edi, %edi



 movl $(65 / 8),%r10d
 movl $__NR_rt_sigprocmask, %eax
 syscall
 cmpq $-4095, %rax
 jae SYSCALL_ERROR_LABEL


 xorl %eax, %eax
L(pseudo_end):
 ret
PSEUDO_END(__getcontext)

weak_alias (__getcontext, getcontext)

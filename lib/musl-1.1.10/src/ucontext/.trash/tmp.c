#define _GNU_SOURCE
#include <stddef.h>
#include <signal.h>
#include <sys/ucontext.h>
void dummy(void) {
asm ("@@@name@@@SIG_BLOCK@@@value@@@%0@@@end@@@" : : "i" ((long) SIG_BLOCK));
asm ("@@@name@@@SIG_SETMASK@@@value@@@%0@@@end@@@" : : "i" ((long) SIG_SETMASK));
asm ("@@@name@@@_NSIG8@@@value@@@%0@@@end@@@" : : "i" ((long) (_NSIG / 8)));
#define ucontext(member)	offsetof (ucontext_t, member)
#define mcontext(member)	ucontext (uc_mcontext.member)
#define mreg(reg)		mcontext (gregs[REG_##reg])
asm ("@@@name@@@oRBP@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RBP)));
asm ("@@@name@@@oRSP@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RSP)));
asm ("@@@name@@@oRBX@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RBX)));
asm ("@@@name@@@oR8@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R8)));
asm ("@@@name@@@oR9@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R9)));
asm ("@@@name@@@oR10@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R10)));
asm ("@@@name@@@oR11@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R11)));
asm ("@@@name@@@oR12@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R12)));
asm ("@@@name@@@oR13@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R13)));
asm ("@@@name@@@oR14@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R14)));
asm ("@@@name@@@oR15@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (R15)));
asm ("@@@name@@@oRDI@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RDI)));
asm ("@@@name@@@oRSI@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RSI)));
asm ("@@@name@@@oRDX@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RDX)));
asm ("@@@name@@@oRAX@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RAX)));
asm ("@@@name@@@oRCX@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RCX)));
asm ("@@@name@@@oRIP@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (RIP)));
asm ("@@@name@@@oEFL@@@value@@@%0@@@end@@@" : : "i" ((long) mreg (EFL)));
asm ("@@@name@@@oFPREGS@@@value@@@%0@@@end@@@" : : "i" ((long) mcontext (fpregs)));
asm ("@@@name@@@oSIGMASK@@@value@@@%0@@@end@@@" : : "i" ((long) ucontext (uc_sigmask)));
asm ("@@@name@@@oFPREGSMEM@@@value@@@%0@@@end@@@" : : "i" ((long) ucontext (__fpregs_mem)));
asm ("@@@name@@@oMXCSR@@@value@@@%0@@@end@@@" : : "i" ((long) ucontext (__fpregs_mem.mxcsr)));
}

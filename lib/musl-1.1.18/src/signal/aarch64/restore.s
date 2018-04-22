/* Pierre: this corresponds to callign a syscall on return from signal,
 * so it's probably okay to put these in different sections */

.section .text.__restore, "ax"
.global __restore
.type __restore,%function
.align 4
__restore:
	mov x8,#139 // SYS_rt_sigreturn
	svc 0

.section .text.__restore_rt, "ax"
.global __restore_rt
.type __restore_rt,%function
.align 4
__restore_rt:
	mov x8,#139 // SYS_rt_sigreturn
	svc 0

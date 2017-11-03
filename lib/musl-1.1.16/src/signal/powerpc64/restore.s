/* Pierre: this corresponds to callign a syscall on return from signal,
 * so it's probably okay to but both in a different section */

	.section .text.__restore, "ax"
	.global __restore
	.type __restore,%function
	.align 4
__restore:
	li      0, 119 #__NR_sigreturn
	sc

	.section .text.__restore_rt, "ax"
	.global __restore_rt
	.type __restore_rt,%function
	.align 4
__restore_rt:
	li      0, 172 # __NR_rt_sigreturn
	sc

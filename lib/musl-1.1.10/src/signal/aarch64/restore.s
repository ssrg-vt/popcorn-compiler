.section .text.__restore_rt, "ax"
.align 4
.global __restore
.type __restore,%function
__restore:
.global __restore_rt
.type __restore_rt,%function
__restore_rt:
	mov x8,#139 // SYS_rt_sigreturn
	svc 0

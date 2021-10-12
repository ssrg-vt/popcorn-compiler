.section .text.__restore, "ax"
.global __restore
.type __restore, %function
.align 4
__restore:

.section .text.__restore_rt, "ax"
.global __restore_rt
.type __restore_rt, %function
.align 4
__restore_rt:
	li a7, 139 # SYS_rt_sigreturn
	scall

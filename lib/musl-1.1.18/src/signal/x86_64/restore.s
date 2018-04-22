/* 	nop */ /* Piere: we cannot have a nop here, what is the purpose of this? */

.section .text.__restore_rt, "ax"
.global __restore_rt
.type __restore_rt,@function
__restore_rt:
	mov $15, %rax
	syscall
.size __restore_rt,.-__restore_rt

	/* .text */
	.section .text.__unmapself, "ax"
	.global __unmapself
	.type   __unmapself,%function
	.align 4
__unmapself:
	li      0, 91 # __NR_munmap
	sc
	li      0, 1 #__NR_exit
	sc
	blr

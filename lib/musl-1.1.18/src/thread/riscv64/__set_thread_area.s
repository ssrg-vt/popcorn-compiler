.section .text.__set_thread_area, "ax"
.global __set_thread_area
.type   __set_thread_area, %function
.align 4
__set_thread_area:
	mv tp, a0
	li a0, 0
	ret

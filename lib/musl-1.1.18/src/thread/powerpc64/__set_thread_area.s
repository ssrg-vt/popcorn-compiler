/* .text */
.section .text.__set_thread_area, "ax"
.global __set_thread_area
.type   __set_thread_area, %function
.align 4
__set_thread_area:
	mr 13, 3
	li  3, 0
	blr


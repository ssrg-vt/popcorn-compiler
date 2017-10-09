/* Pierre: -ffunction-section obviously does not work with as so let's mimic
 * its effect by hand */
.section .text.__set_thread_area, "ax4"
.global __set_thread_area
.type   __set_thread_area,@function
__set_thread_area:
	msr tpidr_el0,x0
	mov w0,#0
	ret

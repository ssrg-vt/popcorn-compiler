/* .text */
.section .text.fabs, "ax"
.global fabs
.type   fabs,%function
.align 4
fabs:
	fabs d0, d0
	ret

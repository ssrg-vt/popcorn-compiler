/* .text */
.section .text.fabs, "ax"
.global fabs
.type   fabs,%function
fabs:
	fabs d0, d0
	ret

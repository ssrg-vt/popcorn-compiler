/* .text */
.section .text.fabsf, "ax"
.global fabsf
.type   fabsf,%function
.align 4
fabsf:
	fabs s0, s0
	ret

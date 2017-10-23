/* .text */
.section .text.fabsf, "ax"
.global fabsf
.type   fabsf,%function
fabsf:
	fabs s0, s0
	ret

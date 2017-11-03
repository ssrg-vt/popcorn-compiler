/* .text */
.section .text.sqrtf, "ax"
.global sqrtf
.type   sqrtf,%function
.align 4
sqrtf:
	fsqrt s0, s0
	ret

/* .text */
.section .text.sqrt, "ax"
.global sqrt
.type   sqrt,%function
.align 4
sqrt:
	fsqrt d0, d0
	ret

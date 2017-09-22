/* .text */
.section .text.sqrt, "ax"
.global sqrt
.type   sqrt,%function
sqrt:
	fsqrt d0, d0
	ret

.section .text.sqrtf, "ax"
.global sqrtf
.type sqrtf,@function
sqrtf:  sqrtss %xmm0, %xmm0
	ret

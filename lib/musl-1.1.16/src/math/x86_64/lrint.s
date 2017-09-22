.section .text.lrint, "ax"
.global lrint
.type lrint,@function
lrint:
	cvtsd2si %xmm0,%rax
	ret

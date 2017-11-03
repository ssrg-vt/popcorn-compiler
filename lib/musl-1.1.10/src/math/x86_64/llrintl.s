.global llrintl
.type llrintl,@function
.section .text.llrintl, "ax"
llrintl:
	fldt 8(%rsp)
	fistpll 8(%rsp)
	mov 8(%rsp),%rax
	ret

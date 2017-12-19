	.text
	.file	"hello.ll"
	.globl	main
	.align	16, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# BB#0:                                 # %entry
	pushq	%rax
.Ltmp0:
	.cfi_def_cfa_offset 16
	movl	$.Lstr, %edi
	callq	puts
	xorl	%eax, %eax
	popq	%rdx
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc

	.type	.Lstr,@object           # @str
	.section	.rodata.str1.1,"aMS",@progbits,1
.Lstr:
	.asciz	"Hello, world!"
	.size	.Lstr, 14


	.ident	"clang version 3.7.1 (tags/RELEASE_371/final 319349)"
	.section	".note.GNU-stack","",@progbits

//.text
.section .text.dlsym, "ax"
.global dlsym
.hidden __dlsym
.type dlsym,@function
dlsym:
	mov (%rsp),%rdx
	jmp __dlsym

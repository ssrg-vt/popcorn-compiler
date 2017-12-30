.section .text.dlsym, "ax"
.global dlsym
.hidden __dlsym
.type dlsym,%function
.align 4
dlsym:
	mov x2,x30
	b __dlsym

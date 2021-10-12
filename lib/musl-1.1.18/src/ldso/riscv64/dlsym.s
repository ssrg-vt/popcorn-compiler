.section .text.dlsym, "ax"
.global dlsym
.hidden __dlsym
.type dlsym, %function
.align 4
dlsym:
        mv a2, ra
        j __dlsym

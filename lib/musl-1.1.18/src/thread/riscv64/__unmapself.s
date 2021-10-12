.section .text.__unmapself, "ax"
.global __unmapself
.type __unmapself, %function
.align 4
__unmapself:
       li a7, 215 # SYS_munmap
       scall
       li a7, 93  # SYS_exit
       scall

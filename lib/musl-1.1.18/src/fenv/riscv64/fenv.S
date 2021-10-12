#ifndef __riscv_soft_float

.section .text.fgetround, "ax"
.global feclearexcept
.type feclearexcept, %function
feclearexcept:
        csrc fflags, a0
        li a0, 0
        ret

.section .text.fsetround, "ax"
.global feraiseexcept
.type feraiseexcept, %function
feraiseexcept:
        csrs fflags, a0
        li a0, 0
        ret

.section .text.fetestexcept, "ax"
.global fetestexcept
.type fetestexcept, %function
fetestexcept:
        frflags t0
        and a0, t0, a0
        ret

.section .text.fegetround, "ax"
.global fegetround
.type fegetround, %function
fegetround:
        frrm a0
        ret

.section .text.__fesetround, "ax"
.global __fesetround
.type __fesetround, %function
__fesetround:
        fsrm t0, a0
        li a0, 0
        ret

.section .text.fegetenv, "ax"
.global fegetenv
.type fegetenv, %function
fegetenv:
        frcsr t0
        sw t0, 0(a0)
        li a0, 0
        ret

.section .text.fesetenv, "ax"
.global fesetenv
.type fesetenv, %function
fesetenv:
        lw t1, 0(a0)
        fscsr t0, t1
        li a0, 0
        ret

#endif

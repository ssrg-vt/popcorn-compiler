.section .text.fegetround, "ax"
.global fegetround
.type fegetround,%function
.align 4
fegetround:
	mrs x0, fpcr
	and w0, w0, #0xc00000
	ret

.section .text.__fesetround, "ax"
.global __fesetround
.type __fesetround,%function
.align 4
__fesetround:
	mrs x1, fpcr
	bic w1, w1, #0xc00000
	orr w1, w1, w0
	msr fpcr, x1
	mov w0, #0
	ret

.section .text.fetestexcept, "ax"
.global fetestexcept
.type fetestexcept,%function
.align 4
fetestexcept:
	and w0, w0, #0x1f
	mrs x1, fpsr
	and w0, w0, w1
	ret

.section .text.feclearexcept, "ax"
.global feclearexcept
.type feclearexcept,%function
.align 4
feclearexcept:
	and w0, w0, #0x1f
	mrs x1, fpsr
	bic w1, w1, w0
	msr fpsr, x1
	mov w0, #0
	ret

.section .text.feraiseexcept, "ax"
.global feraiseexcept
.type feraiseexcept,%function
.align 4
feraiseexcept:
	and w0, w0, #0x1f
	mrs x1, fpsr
	orr w1, w1, w0
	msr fpsr, x1
	mov w0, #0
	ret

.section .text.fegetenv, "ax"
.global fegetenv
.type fegetenv,%function
.align 4
fegetenv:
	mrs x1, fpcr
	mrs x2, fpsr
	stp w1, w2, [x0]
	mov w0, #0
	ret

.section .text.fesetenv, "ax"
// TODO preserve some bits
.global fesetenv
.type fesetenv,%function
.align 4
fesetenv:
	mov x1, #0
	mov x2, #0
	cmn x0, #1
	b.eq 1f
	ldp w1, w2, [x0]
1:	msr fpcr, x1
	msr fpsr, x2
	mov w0, #0
	ret

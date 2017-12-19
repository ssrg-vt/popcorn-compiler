/* PIERRE How to proceed here? I'm afraid that we cannot place __sigsetjmp_tail 
 * in a different sections as it can be moved around by the 
 * alignement tool and we might break the instruction flow here. 
 * With the current migration constraints there is absolutely no chances to 
 * migrate while the application code is holding a reference to that symbol, 
 * however in the future the migration constraint might be relaxed,
 * in that case we might need a solution here.
 */

 	.section .text.sigsetjmp, "ax"
	.global sigsetjmp
	.global __sigsetjmp
	.type sigsetjmp,%function
	.type __sigsetjmp,%function
	.hidden __setjmp_toc
sigsetjmp:
__sigsetjmp:
	addis 2, 12, .TOC.-__sigsetjmp@ha
	addi  2,  2, .TOC.-__sigsetjmp@l
	ld    5, 24(1)   # load from the TOC slot in the caller's stack frame
	b     1f

	.localentry sigsetjmp,.-sigsetjmp
	.localentry __sigsetjmp,.-__sigsetjmp
	mr    5,  2

1:
	cmpwi cr7, 4, 0
	beq-  cr7, __setjmp_toc

	mflr  6
	std   6, 512(3)
	std   2, 512+16(3)
	std  16, 512+24(3)
	mr   16, 3

	bl __setjmp_toc

	mr   4,  3
	mr   3, 16
	ld   5, 512(3)
	mtlr 5
	ld   2, 512+16(3)
	ld  16, 512+24(3)

.hidden __sigsetjmp_tail
	b __sigsetjmp_tail

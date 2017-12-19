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
.align 4
sigsetjmp:
__sigsetjmp:
	cbz x1,setjmp

	str x30,[x0,#176]
	str x19,[x0,#176+8+8]
	mov x19,x0

	bl setjmp

	mov w1,w0
	mov x0,x19
	ldr x30,[x0,#176]
	ldr x19,[x0,#176+8+8]

.hidden __sigsetjmp_tail
	b __sigsetjmp_tail

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
.type sigsetjmp,@function
.type __sigsetjmp,@function
sigsetjmp:
__sigsetjmp:
	test %esi,%esi
	jz 1f

	popq 64(%rdi)
	mov %rbx,72+8(%rdi)
	mov %rdi,%rbx

	call setjmp@PLT

	pushq 64(%rbx)
	mov %rbx,%rdi
	mov %eax,%esi
	mov 72+8(%rbx),%rbx

.hidden __sigsetjmp_tail
	jmp __sigsetjmp_tail

1:	jmp setjmp@PLT

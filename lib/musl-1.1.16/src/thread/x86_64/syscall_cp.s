/* PIERRE How to proceed here? I'm afraid that we cannot place __cp_end 
 * and __cp_cancel in different sections as these can be moved around by the 
 * alignement tool and we might break the instruction flow here. 
 * With the current migration constraints there is absolutely no chances to 
 * migrate while the application code is holding a reference to __cp_end and
 * __cp_cancel, however in the future the migration constraint might be relaxed,
 * in that case we might need a solution here.
 */

/* .text */
.section .text.__syscall_cp_asm, "ax"
.global __cp_begin
.hidden __cp_begin
.global __cp_end
.hidden __cp_end
.global __cp_cancel
.hidden __cp_cancel
.hidden __cancel
.global __syscall_cp_asm
.hidden __syscall_cp_asm
.type   __syscall_cp_asm,@function
__syscall_cp_asm:

__cp_begin:
	mov (%rdi),%eax
	test %eax,%eax
	jnz __cp_cancel
	mov %rdi,%r11
	mov %rsi,%rax
	mov %rdx,%rdi
	mov %rcx,%rsi
	mov %r8,%rdx
	mov %r9,%r10
	mov 8(%rsp),%r8
	mov 16(%rsp),%r9
	mov %r11,8(%rsp)
	syscall
__cp_end:
	ret
__cp_cancel:
	jmp __cancel

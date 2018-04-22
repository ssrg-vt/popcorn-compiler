// __syscall_cp_asm(&self->cancel, nr, u, v, w, x, y, z)
//                  x0             x1  x2 x3 x4 x5 x6 x7

// syscall(nr, u, v, w, x, y, z)
//         x8  x0 x1 x2 x3 x4 x5

/* PIERRE How to proceed here? I'm afraid that we cannot place __cp_end 
 * and __cp_cancel in different sections as these can be moved around by the 
 * alignement tool and we might break the instruction flow here. 
 * With the current migration constraints there is absolutely no chances to 
 * migrate while the application code is holding a reference to __cp_end and
 * __cp_cancel, however in the future the migration constraint might be relaxed,
 * in that case we might need a solution here.
 */

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
.type __syscall_cp_asm,%function
.align 4
__syscall_cp_asm:
__cp_begin:
	ldr w0,[x0]
	cbnz w0,__cp_cancel
	mov x8,x1
	mov x0,x2
	mov x1,x3
	mov x2,x4
	mov x3,x5
	mov x4,x6
	mov x5,x7
	svc 0
__cp_end:
	ret
__cp_cancel:
	b __cancel

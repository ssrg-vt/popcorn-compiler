/* __cleartid_exit_nostack(tidptr, status) */
/*                         rdi     esi     */

/* syscall(SYS_futex, int *uaddr, int futex_op, int val, ...) */
/* syscall(SYS_exit, int status)                              */

.section .text.__cleartid_exit_nostack, "ax"
.global __cleartid_exit_nostack
__cleartid_exit_nostack:
  /* Save the status for after the futex wake call */
  movl %esi, %r12d

  /* Clear tid & call futex wake for joining threads.  We are *not* allowed to */
  /* touch the stack after this point. */
  movl $0, (%rdi)
  movq $129, %rsi /* FUTEX_WAKE | FUTEX_PRIVATE */
  movq $1, %rdx
  movq $202, %rax /* SYS_futex */
  syscall

1:
  /* Call SYS_exit with the status code */
  movl %r12d, %edi
  movq $60, %rax /* SYS_exit */
  syscall
  jmp 1b


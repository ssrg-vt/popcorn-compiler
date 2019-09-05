// CJP: FIXME
// __cleartid_exit_nostack(tidptr, status)
//                         a0      a1

// syscall(SYS_futex, int *uaddr, int futex_op, int val, ...)
// syscall(SYS_exit, int status)

.section .text.__cleartid_exit_nostack, "ax"
.global __cleartid_exit_nostack
__cleartid_exit_nostack:
  // Save the status for after the futex wake call
  addi s1, a1, 0

  // Clear tid & call futex wake for joining threads.  We are *not* allowed to
  // touch the stack after this point.
  sw zero, (a0)
  addi a1, zero, 129 // FUTEX_WAKE | FUTEX_PRIVATE
  addi a2, zero, 1
  addi a7, zero, 98 // SYS_futex
  ecall

loop:
  // Call SYS_exit with the status code
  addi a0, s1, 0
  addi a7, zero, 93 // SYS_exit
  ecall
  j loop


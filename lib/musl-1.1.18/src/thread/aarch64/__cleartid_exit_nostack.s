// __cleartid_exit_nostack(tidptr, status)
//                         x0      x1

// syscall(SYS_futex, int *uaddr, int futex_op, int val, ...)
// syscall(SYS_exit, int status)

.section .text.__cleartid_exit_nostack, "ax"
.global __cleartid_exit_nostack
__cleartid_exit_nostack:
  // Save the status for after the futex wake call
  mov x19, x1

  // Clear tid & call futex wake for joining threads.  We are *not* allowed to
  // touch the stack after this point.
  str wzr, [x0]
  mov x1, #129 // FUTEX_WAKE | FUTEX_PRIVATE
  mov x2, #1
  mov x8, #98 // SYS_futex
  svc #0

1:
  // Call SYS_exit with the status code
  mov w0, w19
  mov x8, #93 // SYS_exit
  svc #0
  b 1b


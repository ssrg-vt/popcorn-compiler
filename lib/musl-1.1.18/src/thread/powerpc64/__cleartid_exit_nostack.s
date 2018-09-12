# __cleartid_exit_nostack(tidptr, status)
#                         r3      r4

# syscall(SYS_futex, int *uaddr, int futex_op, int val, ...)
# syscall(SYS_exit, int status)

.section .text.__cleartid_exit_nostack, "ax"
.global __cleartid_exit_nostack
__cleartid_exit_nostack:
  # Save the status for after the futex wake call
  mr 14, 4

  # Clear tid & call futex wake for joining threads.  We are *not* allowed to
  # touch the stack after this point.
  li 4, 0
  std 4, 0(3)
  li 4, 129 # FUTEX_WAKE | FUTEX_PRIVATE
  li 5, 1
  li 0, 221
  sc

1:
  # Call SYS_exit with the status code
  mr 4, 14
  li 0, 1
  sc
  b 1b


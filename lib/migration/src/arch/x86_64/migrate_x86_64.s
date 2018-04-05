.extern pthread_migrate_args
.extern __migrate_shim_internal
.extern crash_x86_64

.section .text.__migrate_fixup_x86_64, "ax"
.globl __migrate_fixup_x86_64
.type __migrate_fixup_x86_64,@function
__migrate_fixup_x86_64:
.ifdef __x86_64__
  /*
   * Get a reference to the migration arguments struct, which is of the form:
   *
   *   struct shim_data {
   *     void (*callback)(void *);
   *     void *callback_data;
   *     void *regset;
   *   };
   *
   * We need a reference to the regset, which we can then use to restore the
   * callee-saved registers.
   *
   * Note: we don't need to save caller-saved registers -- due to the stack
   * transformation process, all caller-saved registers should have been saved
   * to the stack in the caller's frame.
   */
  call pthread_migrate_args
  mov (%rax), %rdx /* Get struct shim_data pointer */
  test %rdx, %rdx
  jz .Lcrash

  /*
   * RAX points to the base of the shim_data struct.  First, load the regset
   * pointer.  Next, load the callee-saved R* registers.  See
   * <compiler repo>/lib/stack_transformation/include/arch/x86_64/regs.h for
   * the register set layout.
   *
   * According to the ABI, registers RBX, RBP, R12 - R15 are callee-saved.
   */
  mov 16(%rdx), %rdx /* Get regset pointer */
  mov 32(%rdx), %rbx
  mov 56(%rdx), %rbp
  mov 104(%rdx), %r12
  mov 112(%rdx), %r13
  mov 120(%rdx), %r14
  mov 128(%rdx), %r15

  /* Cleanup & return to C! */
  jmp __migrate_shim_internal

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (rdi): return value from pthread_migrate_args
   *  arg 2 (rsi): struct shim_data pointer
   *  arg 3 (rdx): return address
   */
  mov %rax, %rdi
  mov %rdx, %rsi
  mov (%rsp), %rdx
  call crash_x86_64

.endif


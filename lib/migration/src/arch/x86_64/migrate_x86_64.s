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
   *     void *post_syscall;
   *   };
   *
   * We use the regset to restore the callee-saved registers and post_syscall
   * as the destination at which we return to normal execution.
   */
  push %rsp /* Align stack pointer */

  /*
   * In a homogeneous migration, the stack pointer was already at the correct
   * alignment and we just ruined it. Push a sentinel value to both set us at
   * the correct alignment and to signal an extra pop is needed at cleanup.
   */
  test %spl, %spl
  jz .Lpost_align
  push $0

.Lpost_align:
  call pthread_migrate_args
  test %rax, %rax
  jz .Lcrash
  mov (%rax), %rdx /* Get struct shim_data pointer */
  test %rdx, %rdx
  jz .Lcrash
  mov 16(%rdx), %rcx /* Get regset pointer */
  test %rcx, %rcx
  jz .Lcrash

  /*
   * According to the ABI, registers RBX, RBP, R12 - R15 are callee-saved.
   *
   * See <compiler repo>/lib/stack_transformation/include/arch/x86_64/regs.h
   * for the register set layout.
   *
   */
  mov 32(%rcx), %rbx
  mov 56(%rcx), %rbp
  mov 104(%rcx), %r12
  mov 112(%rcx), %r13
  mov 120(%rcx), %r14
  mov 128(%rcx), %r15

  /* Check if we need an extra pop to clean up correctly */
  pop %rcx
  test %rcx, %rcx
  jnz .Lcleanup
  pop %rcx

.Lcleanup:
  /* Cleanup & return to C! */
  mov %rcx, %rsp
  xor %rax, %rax /* Return successful migration */
  mov 24(%rdx), %rdx /* Load post_syscall target PC */
  jmp *%rdx

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (rdi): return value from pthread_migrate_args
   *  arg 2 (rsi): struct shim_data pointer
   *  arg 3 (rdx): regset_pointer
   *  arg 4 (rcx): return address
   */
  mov %rax, %rdi
  mov %rdx, %rsi
  mov %rcx, %rdx
  mov 0(%rsp), %rcx
  test %rcx, %rcx
  jz .Lhomogeneous_stack
  mov 8(%rsp), %rcx
  jmp .Lcrash_call
.Lhomogeneous_stack:
  mov 16(%rsp), %rcx
.Lcrash_call:
  call crash_x86_64

.endif


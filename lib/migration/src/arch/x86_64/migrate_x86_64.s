.extern popcorn_migrate_args
.extern crash_x86_64

/* Specify GNU-stack to allow the linker to automatically set
 * noexecstack.  The Popcorn kernel forbids pages set with the
 * execute bit from migrating.  */
.section .note.GNU-stack,"",%progbits

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
  mov popcorn_migrate_args(%rip), %rax
  test %rax, %rax
  jz .Lcrash
  mov 16(%rax), %rdx /* Get regset pointer */
  test %rdx, %rdx
  jz .Lcrash

  /*
   * According to the ABI, registers RBX, RBP, R12 - R15 are callee-saved.
   *
   * See <compiler repo>/lib/stack_transformation/include/arch/x86_64/regs.h
   * for the register set layout.
   *
   */
  mov 32(%rdx), %rbx
  mov 56(%rdx), %rbp
  mov 104(%rdx), %r12
  mov 112(%rdx), %r13
  mov 120(%rdx), %r14
  mov 128(%rdx), %r15

  /* Check if we need an extra pop to clean up correctly */
  pop %rdx
  test %rdx, %rdx
  jnz .Lcleanup
  pop %rdx

.Lcleanup:
  /* Cleanup & return to C! */
  mov %rdx, %rsp
  mov 24(%rax), %rdx /* Load post_syscall target PC */
  xor %rax, %rax /* Return successful migration */
  jmp *%rdx

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (rdi): struct shim_data pointer
   *  arg 2 (rsi): regset_pointer
   *  arg 3 (rdx): return address
   */
  mov %rax, %rdi
  mov %rdx, %rsi
  mov 0(%rsp), %rdx
  test %rdx, %rdx
  jz .Lhomogeneous_stack
  mov 8(%rsp), %rdx
  jmp .Lcrash_call
.Lhomogeneous_stack:
  mov 16(%rsp), %rdx
.Lcrash_call:
  call crash_x86_64

.else
	nop
	nop
	nop
	nop
	
.endif


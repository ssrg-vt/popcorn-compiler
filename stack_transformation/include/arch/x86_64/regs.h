/*
 * Register definitions and macros for access for x86-64.
 *
 * DWARF register number to name mappings are derived from the x86-64 ABI
 * http://www.x86-64.org/documentation/abi.pdf
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/17/2015
 */

#ifndef _REGS_X86_64_H
#define _REGS_X86_64_H

///////////////////////////////////////////////////////////////////////////////
// x86-64 structure definitions
///////////////////////////////////////////////////////////////////////////////

/*
 * Defines an abstract register set for the x86-64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for x86-64.
 */
struct regset_x86_64
{
  /* Program counter/instruction pointer */
  void* rip;

  /* General purpose registers */
  uint64_t rax, rdx, rcx, rbx,
           rsi, rdi, rbp, rsp,
           r8, r9, r10, r11,
           r12, r13, r14, r15;

  /* Multimedia-extension (MMX) registers */
  uint64_t mmx[8];

  /* Streaming SIMD Extension (SSE) registers */
  unsigned __int128 xmm[16];

  /* x87 floating point registers */
  long double st[8];

  /* Segment registers */
  uint32_t cs, ss, ds, es, fs, gs;

  /* Flag register */
  uint64_t rflags;

  // TODO control registers
};

///////////////////////////////////////////////////////////////////////////////
// DWARF register mappings
///////////////////////////////////////////////////////////////////////////////

/* General purpose x86-64 registers */
#define RAX 0
#define RDX 1
#define RCX 2
#define RBX 3
#define RSI 4
#define RDI 5
#define RBP 6
#define RSP 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15
#define RIP 16

/* Streaming SIMD Extension (SSE) registers */
#define XMM0 17
#define XMM1 18
#define XMM2 19
#define XMM3 20
#define XMM4 21
#define XMM5 22
#define XMM6 23
#define XMM7 24
#define XMM8 25
#define XMM9 26
#define XMM10 27
#define XMM11 28
#define XMM12 29
#define XMM13 30
#define XMM14 31
#define XMM15 32

///////////////////////////////////////////////////////////////////////////////
// Register access
///////////////////////////////////////////////////////////////////////////////

#ifdef __x86_64__

/* Getters & setters for varying registers & sizes */
#define GET_REG( var, reg, size ) asm volatile("mov"size" %%"reg", %0" : "=m" (var) )
#define GET_REG8( var, reg ) GET_REG( var, reg, "b" )
#define GET_REG16( var, reg ) GET_REG( var, reg, "s" )
#define GET_REG32( var, reg ) GET_REG( var, reg, "l" )
#define GET_REG64( var, reg ) GET_REG( var, reg, "q" )

#define SET_REG( var, reg, size ) asm volatile("mov"size" %0, %%"reg : : "m" (var) : reg )
#define SET_REG8( var, reg ) SET_REG( var, reg, "b" )
#define SET_REG16( var, reg ) SET_REG( var, reg, "s" )
#define SET_REG32( var, reg ) SET_REG( var, reg, "l" )
#define SET_REG64( var, reg ) SET_REG( var, reg, "q" )

/* General-purpose x86-64 registers */
#define GET_RAX( var ) GET_REG64( var, "rax" )
#define GET_RBX( var ) GET_REG64( var, "rbx" )
#define GET_RCX( var ) GET_REG64( var, "rcx" )
#define GET_RDX( var ) GET_REG64( var, "rdx" )
#define GET_RBP( var ) GET_REG64( var, "rbp" )
#define GET_RSI( var ) GET_REG64( var, "rsi" )
#define GET_RDI( var ) GET_REG64( var, "rdi" )
#define GET_RSP( var ) GET_REG64( var, "rsp" )
#define GET_R8( var ) GET_REG64( var, "r8"  )
#define GET_R9( var ) GET_REG64( var, "r9"  )
#define GET_R10( var ) GET_REG64( var, "r10" )
#define GET_R11( var ) GET_REG64( var, "r11" )
#define GET_R12( var ) GET_REG64( var, "r12" )
#define GET_R13( var ) GET_REG64( var, "r13" )
#define GET_R14( var ) GET_REG64( var, "r14" )
#define GET_R15( var ) GET_REG64( var, "r15" )

#define SET_RAX( var ) SET_REG64( var, "rax" )
#define SET_RBX( var ) SET_REG64( var, "rbx" )
#define SET_RCX( var ) SET_REG64( var, "rcx" )
#define SET_RDX( var ) SET_REG64( var, "rdx" )
#define SET_RBP( var ) SET_REG64( var, "rbp" )
#define SET_RSI( var ) SET_REG64( var, "rsi" )
#define SET_RDI( var ) SET_REG64( var, "rdi" )
#define SET_RSP( var ) SET_REG64( var, "rsp" )
#define SET_R8( var ) SET_REG64( var, "r8"  )
#define SET_R9( var ) SET_REG64( var, "r9"  )
#define SET_R10( var ) SET_REG64( var, "r10" )
#define SET_R11( var ) SET_REG64( var, "r11" )
#define SET_R12( var ) SET_REG64( var, "r12" )
#define SET_R13( var ) SET_REG64( var, "r13" )
#define SET_R14( var ) SET_REG64( var, "r14" )
#define SET_R15( var ) SET_REG64( var, "r15" )

/*
 * The instruction pointer is a little weird because you can't read it
 * directly.  The assembler replaces "$." with the address of the instruction.
 */
#define GET_RIP( var ) asm volatile("movq $., %0" : "=g" (var) )

/*
 * The only way to set the IP is through control flow operations.
 */
#define SET_RIP_REG( var ) asm volatile("jmpq *%0" : : "r" (var) )
#define SET_RIP_IMM( var ) asm volatile("movq %0, -0x8(%%rsp); jmpq *-0x8(%%rsp)" : : "i" (var) )

/* 
 * The flags register also can't be read directly.  Push its value onto the
 * stack then pop off into var.
 */
#define GET_RFLAGS( var ) asm volatile("pushf; pop %0" : "=g" (var) )

/* Segment registers */
#define GET_CS( var ) GET_REG( var, "cs", "" )
#define GET_SS( var ) GET_REG( var, "ss", "" )
#define GET_DS( var ) GET_REG( var, "ds", "" )
#define GET_ES( var ) GET_REG( var, "es", "" )
#define GET_FS( var ) GET_REG( var, "fs", "" )
#define GET_GS( var ) GET_REG( var, "gs", "" )

// TODO set segment registers -- requires syscall

/* Multimedia-extension (MMX) registers */
#define GET_XMM( var, num ) asm volatile("movsd %%xmm"num", %0" : "=m" (var) )
#define GET_XMM0( var ) GET_XMM( var, "0" )
#define GET_XMM1( var ) GET_XMM( var, "1" )
#define GET_XMM2( var ) GET_XMM( var, "2" )
#define GET_XMM3( var ) GET_XMM( var, "3" )
#define GET_XMM4( var ) GET_XMM( var, "4" )
#define GET_XMM5( var ) GET_XMM( var, "5" )
#define GET_XMM6( var ) GET_XMM( var, "6" )
#define GET_XMM7( var ) GET_XMM( var, "7" )
#define GET_XMM8( var ) GET_XMM( var, "8" )
#define GET_XMM9( var ) GET_XMM( var, "9" )
#define GET_XMM10( var ) GET_XMM( var, "10" )
#define GET_XMM11( var ) GET_XMM( var, "11" )
#define GET_XMM12( var ) GET_XMM( var, "12" )
#define GET_XMM13( var ) GET_XMM( var, "13" )
#define GET_XMM14( var ) GET_XMM( var, "14" )
#define GET_XMM15( var ) GET_XMM( var, "15" )

#define SET_XMM( var, num ) asm volatile("movsd %0, %%xmm"num : : "m" (var) : "xmm"num )
#define SET_XMM0( var ) SET_XMM( var, "0" )
#define SET_XMM1( var ) SET_XMM( var, "1" )
#define SET_XMM2( var ) SET_XMM( var, "2" )
#define SET_XMM3( var ) SET_XMM( var, "3" )
#define SET_XMM4( var ) SET_XMM( var, "4" )
#define SET_XMM5( var ) SET_XMM( var, "5" )
#define SET_XMM6( var ) SET_XMM( var, "6" )
#define SET_XMM7( var ) SET_XMM( var, "7" )
#define SET_XMM8( var ) SET_XMM( var, "8" )
#define SET_XMM9( var ) SET_XMM( var, "9" )
#define SET_XMM10( var ) SET_XMM( var, "10" )
#define SET_XMM11( var ) SET_XMM( var, "11" )
#define SET_XMM12( var ) SET_XMM( var, "12" )
#define SET_XMM13( var ) SET_XMM( var, "13" )
#define SET_XMM14( var ) SET_XMM( var, "14" )
#define SET_XMM15( var ) SET_XMM( var, "15" )

// Note: the following NOCLOBBER macros are only used for special cases, use
// the macros above for normal access
#define SET_XMM_NOCLOBBER( var, num ) asm volatile("movsd %0, %%xmm"num : : "m" (var) )
#define SET_XMM0_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "0" )
#define SET_XMM1_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "1" )
#define SET_XMM2_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "2" )
#define SET_XMM3_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "3" )
#define SET_XMM4_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "4" )
#define SET_XMM5_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "5" )
#define SET_XMM6_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "6" )
#define SET_XMM7_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "7" )
#define SET_XMM8_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "8" )
#define SET_XMM9_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "9" )
#define SET_XMM10_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "10" )
#define SET_XMM11_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "11" )
#define SET_XMM12_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "12" )
#define SET_XMM13_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "13" )
#define SET_XMM14_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "14" )
#define SET_XMM15_NOCLOBBER( var ) SET_XMM_NOCLOBBER( var, "15" )

/* Streaming SIMD Extension (SSE) registers */
// TODO

/* x87 floating point registers */
// TODO since cannot access directly, may need to empty x87 stack

/* Read all registers into a register set. */
#define READ_REGS_X86_64( regset_x86_64 ) \
{ \
  GET_RAX((regset_x86_64).rax); \
  GET_RDX((regset_x86_64).rdx); \
  GET_RCX((regset_x86_64).rcx); \
  GET_RBX((regset_x86_64).rbx); \
  GET_RBP((regset_x86_64).rbp); \
  GET_RSI((regset_x86_64).rsi); \
  GET_RDI((regset_x86_64).rdi); \
  GET_RSP((regset_x86_64).rsp); \
  GET_R8((regset_x86_64).r8); \
  GET_R9((regset_x86_64).r9); \
  GET_R10((regset_x86_64).r10); \
  GET_R11((regset_x86_64).r11); \
  GET_R12((regset_x86_64).r12); \
  GET_R13((regset_x86_64).r13); \
  GET_R14((regset_x86_64).r14); \
  GET_R15((regset_x86_64).r15); \
  GET_RIP((regset_x86_64).rip); \
  GET_RFLAGS((regset_x86_64).rflags); \
  GET_CS((regset_x86_64).cs); \
  GET_SS((regset_x86_64).ss); \
  GET_DS((regset_x86_64).ds); \
  GET_ES((regset_x86_64).es); \
  GET_FS((regset_x86_64).fs); \
  GET_GS((regset_x86_64).gs); \
  GET_XMM0((regset_x86_64).xmm[0]); \
  GET_XMM1((regset_x86_64).xmm[1]); \
  GET_XMM2((regset_x86_64).xmm[2]); \
  GET_XMM3((regset_x86_64).xmm[3]); \
  GET_XMM4((regset_x86_64).xmm[4]); \
  GET_XMM5((regset_x86_64).xmm[5]); \
  GET_XMM6((regset_x86_64).xmm[6]); \
  GET_XMM7((regset_x86_64).xmm[7]); \
  GET_XMM8((regset_x86_64).xmm[8]); \
  GET_XMM9((regset_x86_64).xmm[9]); \
  GET_XMM10((regset_x86_64).xmm[10]); \
  GET_XMM11((regset_x86_64).xmm[11]); \
  GET_XMM12((regset_x86_64).xmm[12]); \
  GET_XMM13((regset_x86_64).xmm[13]); \
  GET_XMM14((regset_x86_64).xmm[14]); \
  GET_XMM15((regset_x86_64).xmm[15]); \
}

/* Set floating-point/SIMD registers from a register set. */
#define SET_FP_REGS_X86_64( regset_x86_64 ) \
{ \
  SET_XMM0((regset_x86_64).xmm[0]); \
  SET_XMM1((regset_x86_64).xmm[1]); \
  SET_XMM2((regset_x86_64).xmm[2]); \
  SET_XMM3((regset_x86_64).xmm[3]); \
  SET_XMM4((regset_x86_64).xmm[4]); \
  SET_XMM5((regset_x86_64).xmm[5]); \
  SET_XMM6((regset_x86_64).xmm[6]); \
  SET_XMM7((regset_x86_64).xmm[7]); \
  SET_XMM8((regset_x86_64).xmm[8]); \
  SET_XMM9((regset_x86_64).xmm[9]); \
  SET_XMM10((regset_x86_64).xmm[10]); \
  SET_XMM11((regset_x86_64).xmm[11]); \
  SET_XMM12((regset_x86_64).xmm[12]); \
  SET_XMM13((regset_x86_64).xmm[13]); \
  SET_XMM14((regset_x86_64).xmm[14]); \
  SET_XMM15((regset_x86_64).xmm[15]); \
}

/*
 * Set floating-point/SIMD registers from a register set. Don't mark the
 * registers as clobbered, so the compiler won't save/restore them.
 */
// Note: this should *only* be used inside of the migration library
#define SET_FP_REGS_NOCLOBBER_X86_64( regset_x86_64 ) \
{ \
  SET_XMM0_NOCLOBBER((regset_x86_64).xmm[0]); \
  SET_XMM1_NOCLOBBER((regset_x86_64).xmm[1]); \
  SET_XMM2_NOCLOBBER((regset_x86_64).xmm[2]); \
  SET_XMM3_NOCLOBBER((regset_x86_64).xmm[3]); \
  SET_XMM4_NOCLOBBER((regset_x86_64).xmm[4]); \
  SET_XMM5_NOCLOBBER((regset_x86_64).xmm[5]); \
  SET_XMM6_NOCLOBBER((regset_x86_64).xmm[6]); \
  SET_XMM7_NOCLOBBER((regset_x86_64).xmm[7]); \
  SET_XMM8_NOCLOBBER((regset_x86_64).xmm[8]); \
  SET_XMM9_NOCLOBBER((regset_x86_64).xmm[9]); \
  SET_XMM10_NOCLOBBER((regset_x86_64).xmm[10]); \
  SET_XMM11_NOCLOBBER((regset_x86_64).xmm[11]); \
  SET_XMM12_NOCLOBBER((regset_x86_64).xmm[12]); \
  SET_XMM13_NOCLOBBER((regset_x86_64).xmm[13]); \
  SET_XMM14_NOCLOBBER((regset_x86_64).xmm[14]); \
  SET_XMM15_NOCLOBBER((regset_x86_64).xmm[15]); \
}

/* Set all registers from a register set. */
// Note: do not set RIP, RSP, RBP, and segment registers as they require
// special handling
#define SET_REGS_X86_64( regset_x86_64 ) \
{ \
  SET_RAX((regset_x86_64).rax); \
  SET_RDX((regset_x86_64).rdx); \
  SET_RCX((regset_x86_64).rcx); \
  SET_RBX((regset_x86_64).rbx); \
  SET_RSI((regset_x86_64).rsi); \
  SET_RDI((regset_x86_64).rdi); \
  SET_R8((regset_x86_64).r8); \
  SET_R9((regset_x86_64).r9); \
  SET_R10((regset_x86_64).r10); \
  SET_R11((regset_x86_64).r11); \
  SET_R12((regset_x86_64).r12); \
  SET_R13((regset_x86_64).r13); \
  SET_R14((regset_x86_64).r14); \
  SET_R15((regset_x86_64).r15); \
  SET_FP_REGS_X86_64(regset_x86_64); \
}

/* Get frame information. */
#define GET_FRAME_X86_64( bp, sp ) GET_RBP(bp); GET_RSP(sp);

/* Get current frame's size, defined as rbp-rsp. */
#define GET_FRAME_SIZE_X86_64( size ) \
  asm volatile("mov %%rbp, %0; sub %%rsp, %0" : "=g" (size) )

/* Set frame by setting rbp & rsp. */
#define SET_FRAME_X86_64( bp, sp ) \
  asm volatile("mov %0, %%rsp; mov %1, %%rbp" : : "m" (sp), "m" (bp) )

#endif /* __x86_64__ */

#endif /* _REGS_X86_64_H */


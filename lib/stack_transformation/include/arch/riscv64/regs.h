/*
 * Register definitions and macros for access for riscv64.
 *
 * DWARF register number to name mappings are derived from the RISCV psABI
 * documentation:
 * https://github.com/riscv/riscv-elf-psabi-doc/blob/master/riscv-elf.md
 *
 * Author: Cesar Philippidis
 * Date: 2/7/2020
 */

#ifndef _REGS_RISCV64_H
#define _REGS_RISCV64_H

///////////////////////////////////////////////////////////////////////////////
// riscv64 structure definitions
///////////////////////////////////////////////////////////////////////////////

/*
 * Defines an abstract register set for the riscv64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for arm64.
 */
struct regset_riscv64
{
  /* Stack pointer & program counter */
  void* sp; // x2
  void* pc;

  /* General purpose registers */
  uint64_t x[32];

  /* FPU/SIMD registers */
  uint64_t f[32];
};

///////////////////////////////////////////////////////////////////////////////
// DWARF register mappings
///////////////////////////////////////////////////////////////////////////////

#define RISCV64_NUM_REGS 128

/* General purpose riscv64 registers */
#define X0 0
#define X1 1
#define X2 2
#define X3 3
#define X4 4
#define X5 5
#define X6 6
#define X7 7
#define X8 8
#define X9 9
#define X10 10
#define X11 11
#define X12 12
#define X13 13
#define X14 14
#define X15 15
#define X16 16
#define X17 17
#define X18 18
#define X19 19
#define X20 20
#define X21 21
#define X22 22
#define X23 23
#define X24 24
#define X25 25
#define X26 26
#define X27 27
#define X28 28
#define X29 29
#define X30 30
#define X31 31

#define SP 2

/* Floating-point unit registers */
#define F0 32
#define F1 33
#define F2 34
#define F3 35
#define F4 36
#define F5 37
#define F6 38
#define F7 39
#define F8 40
#define F9 41
#define F10 42
#define F11 43
#define F12 44
#define F13 45
#define F14 46
#define F15 47
#define F16 48
#define F17 49
#define F18 50
#define F19 51
#define F20 52
#define F21 53
#define F22 54
#define F23 55
#define F24 56
#define F25 57
#define F26 58
#define F27 59
#define F28 60
#define F29 61
#define F30 62
#define F31 63

///////////////////////////////////////////////////////////////////////////////
// Register access
///////////////////////////////////////////////////////////////////////////////

#ifdef __riscv64__

/* Getters & setters for varying registers & sizes */
#define GET_REG( var, reg, size ) asm volatile("s"size" "reg", %0" : "=m" (var) )
#define GET_REG32( var, reg ) GET_REG( var, reg, "w" )
#define GET_REG64( var, reg ) GET_REG( var, reg, "d" )

#define SET_REG( var, reg, size ) asm volatile("l"size" "reg", %0" : : "m" (var) : reg )
#define SET_REG32( var, reg ) SET_REG( var, reg, "w" )
#define SET_REG64( var, reg ) SET_REG( var, reg, "d" )

/* General-purpose riscv64 registers */
#define GET_X0( var ) GET_REG64( var, "x0" )
#define GET_X1( var ) GET_REG64( var, "x1" )
#define GET_X2( var ) GET_REG64( var, "x2" )
#define GET_X3( var ) GET_REG64( var, "x3" )
#define GET_X4( var ) GET_REG64( var, "x4" )
#define GET_X5( var ) GET_REG64( var, "x5" )
#define GET_X6( var ) GET_REG64( var, "x6" )
#define GET_X7( var ) GET_REG64( var, "x7" )
#define GET_X8( var ) GET_REG64( var, "x8" )
#define GET_X9( var ) GET_REG64( var, "x9" )
#define GET_X10( var ) GET_REG64( var, "x10" )
#define GET_X11( var ) GET_REG64( var, "x11" )
#define GET_X12( var ) GET_REG64( var, "x12" )
#define GET_X13( var ) GET_REG64( var, "x13" )
#define GET_X14( var ) GET_REG64( var, "x14" )
#define GET_X15( var ) GET_REG64( var, "x15" )
#define GET_X16( var ) GET_REG64( var, "x16" )
#define GET_X17( var ) GET_REG64( var, "x17" )
#define GET_X18( var ) GET_REG64( var, "x18" )
#define GET_X19( var ) GET_REG64( var, "x19" )
#define GET_X20( var ) GET_REG64( var, "x20" )
#define GET_X21( var ) GET_REG64( var, "x21" )
#define GET_X22( var ) GET_REG64( var, "x22" )
#define GET_X23( var ) GET_REG64( var, "x23" )
#define GET_X24( var ) GET_REG64( var, "x24" )
#define GET_X25( var ) GET_REG64( var, "x25" )
#define GET_X26( var ) GET_REG64( var, "x26" )
#define GET_X27( var ) GET_REG64( var, "x27" )
#define GET_X28( var ) GET_REG64( var, "x28" )
#define GET_X29( var ) GET_REG64( var, "x29" )
#define GET_X30( var ) GET_REG64( var, "x30" )
#define GET_X31( var ) GET_REG64( var, "x31" )

#define SET_X0( var ) SET_REG64( var, "x0" )
#define SET_X1( var ) SET_REG64( var, "x1" )
#define SET_X2( var ) SET_REG64( var, "x2" )
#define SET_X3( var ) SET_REG64( var, "x3" )
#define SET_X4( var ) SET_REG64( var, "x4" )
#define SET_X5( var ) SET_REG64( var, "x5" )
#define SET_X6( var ) SET_REG64( var, "x6" )
#define SET_X7( var ) SET_REG64( var, "x7" )
#define SET_X8( var ) SET_REG64( var, "x8" )
#define SET_X9( var ) SET_REG64( var, "x9" )
#define SET_X10( var ) SET_REG64( var, "x10" )
#define SET_X11( var ) SET_REG64( var, "x11" )
#define SET_X12( var ) SET_REG64( var, "x12" )
#define SET_X13( var ) SET_REG64( var, "x13" )
#define SET_X14( var ) SET_REG64( var, "x14" )
#define SET_X15( var ) SET_REG64( var, "x15" )
#define SET_X16( var ) SET_REG64( var, "x16" )
#define SET_X17( var ) SET_REG64( var, "x17" )
#define SET_X18( var ) SET_REG64( var, "x18" )
#define SET_X19( var ) SET_REG64( var, "x19" )
#define SET_X20( var ) SET_REG64( var, "x20" )
#define SET_X21( var ) SET_REG64( var, "x21" )
#define SET_X22( var ) SET_REG64( var, "x22" )
#define SET_X23( var ) SET_REG64( var, "x23" )
#define SET_X24( var ) SET_REG64( var, "x24" )
#define SET_X25( var ) SET_REG64( var, "x25" )
#define SET_X26( var ) SET_REG64( var, "x26" )
#define SET_X27( var ) SET_REG64( var, "x27" )
#define SET_X28( var ) SET_REG64( var, "x28" )
#define SET_X29( var ) SET_REG64( var, "x29" )
#define SET_X30( var ) SET_REG64( var, "x30" )
#define SET_X31( var ) SET_REG64( var, "x31" )

/*
 * The stack pointer is a little weird because you can't read it directly into/
 * write it directly from memory.  Move it into another register which can be
 * saved in memory.
 */
#define GET_SP( var ) asm volatile("mv s1, sp; sd s1, %0" : "=m" (var) : : "s1")
#define SET_SP( var ) asm volatile("ld s1, %0; mv sp, s1" : : "m" (var) : "s1")

/*
 * The program counter also can't be read directly.  The assembler replaces
 * "." or "#." with the address of the instruction.
 */
#define GET_PC( var ) asm volatile("auipc %0, 0" : "=r" (var))

/*
 * The only way to set the PC is through control flow operations.
 */
#define SET_PC_IMM( val ) asm volatile("li %t1, %0; jr %t1" : : "i" (val) : "t1" )
#define SET_PC_REG( val ) asm volatile("jr %0" : : "r" (val) )

/* Floating-point unit (FPU)/SIMD registers */
#define GET_FP_REG( var, reg ) asm volatile("fsd "reg", %0" : "=m" (var) )
#define GET_F0( var ) GET_FP_REG( var, "f0" )
#define GET_F1( var ) GET_FP_REG( var, "f1" )
#define GET_F2( var ) GET_FP_REG( var, "f2" )
#define GET_F3( var ) GET_FP_REG( var, "f3" )
#define GET_F4( var ) GET_FP_REG( var, "f4" )
#define GET_F5( var ) GET_FP_REG( var, "f5" )
#define GET_F6( var ) GET_FP_REG( var, "f6" )
#define GET_F7( var ) GET_FP_REG( var, "f7" )
#define GET_F8( var ) GET_FP_REG( var, "f8" )
#define GET_F9( var ) GET_FP_REG( var, "f9" )
#define GET_F10( var ) GET_FP_REG( var, "f10" )
#define GET_F11( var ) GET_FP_REG( var, "f11" )
#define GET_F12( var ) GET_FP_REG( var, "f12" )
#define GET_F13( var ) GET_FP_REG( var, "f13" )
#define GET_F14( var ) GET_FP_REG( var, "f14" )
#define GET_F15( var ) GET_FP_REG( var, "f15" )
#define GET_F16( var ) GET_FP_REG( var, "f16" )
#define GET_F17( var ) GET_FP_REG( var, "f17" )
#define GET_F18( var ) GET_FP_REG( var, "f18" )
#define GET_F19( var ) GET_FP_REG( var, "f19" )
#define GET_F20( var ) GET_FP_REG( var, "f20" )
#define GET_F21( var ) GET_FP_REG( var, "f21" )
#define GET_F22( var ) GET_FP_REG( var, "f22" )
#define GET_F23( var ) GET_FP_REG( var, "f23" )
#define GET_F24( var ) GET_FP_REG( var, "f24" )
#define GET_F25( var ) GET_FP_REG( var, "f25" )
#define GET_F26( var ) GET_FP_REG( var, "f26" )
#define GET_F27( var ) GET_FP_REG( var, "f27" )
#define GET_F28( var ) GET_FP_REG( var, "f28" )
#define GET_F29( var ) GET_FP_REG( var, "f29" )
#define GET_F30( var ) GET_FP_REG( var, "f30" )
#define GET_F31( var ) GET_FP_REG( var, "f31" )

#define SET_FP_REG( var, reg, name ) asm volatile("fld "reg", %0" : : "m" (var) : name )
#define SET_F0( var ) SET_FP_REG( var, "f0", "f0" )
#define SET_F1( var ) SET_FP_REG( var, "f1", "f1" )
#define SET_F2( var ) SET_FP_REG( var, "f2", "f2" )
#define SET_F3( var ) SET_FP_REG( var, "f3", "f3" )
#define SET_F4( var ) SET_FP_REG( var, "f4", "f4" )
#define SET_F5( var ) SET_FP_REG( var, "f5", "f5" )
#define SET_F6( var ) SET_FP_REG( var, "f6", "f6" )
#define SET_F7( var ) SET_FP_REG( var, "f7", "f7" )
#define SET_F8( var ) SET_FP_REG( var, "f8", "f8" )
#define SET_F9( var ) SET_FP_REG( var, "f9", "f9" )
#define SET_F10( var ) SET_FP_REG( var, "f10", "f10" )
#define SET_F11( var ) SET_FP_REG( var, "f11", "f11" )
#define SET_F12( var ) SET_FP_REG( var, "f12", "f12" )
#define SET_F13( var ) SET_FP_REG( var, "f13", "f13" )
#define SET_F14( var ) SET_FP_REG( var, "f14", "f14" )
#define SET_F15( var ) SET_FP_REG( var, "f15", "f15" )
#define SET_F16( var ) SET_FP_REG( var, "f16", "f16" )
#define SET_F17( var ) SET_FP_REG( var, "f17", "f17" )
#define SET_F18( var ) SET_FP_REG( var, "f18", "f18" )
#define SET_F19( var ) SET_FP_REG( var, "f19", "f19" )
#define SET_F20( var ) SET_FP_REG( var, "f20", "f20" )
#define SET_F21( var ) SET_FP_REG( var, "f21", "f21" )
#define SET_F22( var ) SET_FP_REG( var, "f22", "f22" )
#define SET_F23( var ) SET_FP_REG( var, "f23", "f23" )
#define SET_F24( var ) SET_FP_REG( var, "f24", "f24" )
#define SET_F25( var ) SET_FP_REG( var, "f25", "f25" )
#define SET_F26( var ) SET_FP_REG( var, "f26", "f26" )
#define SET_F27( var ) SET_FP_REG( var, "f27", "f27" )
#define SET_F28( var ) SET_FP_REG( var, "f28", "f28" )
#define SET_F29( var ) SET_FP_REG( var, "f29", "f29" )
#define SET_F30( var ) SET_FP_REG( var, "f30", "f30" )
#define SET_F31( var ) SET_FP_REG( var, "f31", "f31" )

// Note: the following NOCLOBBER macros are only used for special cases, use
// the macros above for normal access
#define SET_FP_REG_NOCLOBBER( var, reg ) asm volatile("fld "reg", %0" : : "m" (var) )
#define SET_F0_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f0" )
#define SET_F1_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f1" )
#define SET_F2_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f2" )
#define SET_F3_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f3" )
#define SET_F4_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f4" )
#define SET_F5_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f5" )
#define SET_F6_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f6" )
#define SET_F7_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f7" )
#define SET_F8_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f8" )
#define SET_F9_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f9" )
#define SET_F10_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f10" )
#define SET_F11_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f11" )
#define SET_F12_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f12" )
#define SET_F13_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f13" )
#define SET_F14_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f14" )
#define SET_F15_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f15" )
#define SET_F16_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f16" )
#define SET_F17_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f17" )
#define SET_F18_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f18" )
#define SET_F19_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f19" )
#define SET_F20_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f20" )
#define SET_F21_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f21" )
#define SET_F22_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f22" )
#define SET_F23_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f23" )
#define SET_F24_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f24" )
#define SET_F25_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f25" )
#define SET_F26_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f26" )
#define SET_F27_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f27" )
#define SET_F28_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f28" )
#define SET_F29_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f29" )
#define SET_F30_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f30" )
#define SET_F31_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "f31" )

/* Read all registers into a register set. */
#define READ_REGS_RISCV64( regset_riscv64 ) \
{ \
  GET_X0((regset_riscv64).x[0]); \
  GET_X1((regset_riscv64).x[1]); \
  GET_X2((regset_riscv64).x[2]); \
  GET_X3((regset_riscv64).x[3]); \
  GET_X4((regset_riscv64).x[4]); \
  GET_X5((regset_riscv64).x[5]); \
  GET_X6((regset_riscv64).x[6]); \
  GET_X7((regset_riscv64).x[7]); \
  GET_X8((regset_riscv64).x[8]); \
  GET_X9((regset_riscv64).x[9]); \
  GET_X10((regset_riscv64).x[10]); \
  GET_X11((regset_riscv64).x[11]); \
  GET_X12((regset_riscv64).x[12]); \
  GET_X13((regset_riscv64).x[13]); \
  GET_X14((regset_riscv64).x[14]); \
  GET_X15((regset_riscv64).x[15]); \
  GET_X16((regset_riscv64).x[16]); \
  GET_X17((regset_riscv64).x[17]); \
  GET_X18((regset_riscv64).x[18]); \
  GET_X19((regset_riscv64).x[19]); \
  GET_X20((regset_riscv64).x[20]); \
  GET_X21((regset_riscv64).x[21]); \
  GET_X22((regset_riscv64).x[22]); \
  GET_X23((regset_riscv64).x[23]); \
  GET_X24((regset_riscv64).x[24]); \
  GET_X25((regset_riscv64).x[25]); \
  GET_X26((regset_riscv64).x[26]); \
  GET_X27((regset_riscv64).x[27]); \
  GET_X28((regset_riscv64).x[28]); \
  GET_X29((regset_riscv64).x[29]); \
  GET_X30((regset_riscv64).x[30]); \
  GET_SP((regset_riscv64).sp); \
  GET_PC((regset_riscv64).pc); \
  GET_F0((regset_riscv64).f[0]); \
  GET_F1((regset_riscv64).f[1]); \
  GET_F2((regset_riscv64).f[2]); \
  GET_F3((regset_riscv64).f[3]); \
  GET_F4((regset_riscv64).f[4]); \
  GET_F5((regset_riscv64).f[5]); \
  GET_F6((regset_riscv64).f[6]); \
  GET_F7((regset_riscv64).f[7]); \
  GET_F8((regset_riscv64).f[8]); \
  GET_F9((regset_riscv64).f[9]); \
  GET_F10((regset_riscv64).f[10]); \
  GET_F11((regset_riscv64).f[11]); \
  GET_F12((regset_riscv64).f[12]); \
  GET_F13((regset_riscv64).f[13]); \
  GET_F14((regset_riscv64).f[14]); \
  GET_F15((regset_riscv64).f[15]); \
  GET_F16((regset_riscv64).f[16]); \
  GET_F17((regset_riscv64).f[17]); \
  GET_F18((regset_riscv64).f[18]); \
  GET_F19((regset_riscv64).f[19]); \
  GET_F20((regset_riscv64).f[20]); \
  GET_F21((regset_riscv64).f[21]); \
  GET_F22((regset_riscv64).f[22]); \
  GET_F23((regset_riscv64).f[23]); \
  GET_F24((regset_riscv64).f[24]); \
  GET_F25((regset_riscv64).f[25]); \
  GET_F26((regset_riscv64).f[26]); \
  GET_F27((regset_riscv64).f[27]); \
  GET_F28((regset_riscv64).f[28]); \
  GET_F29((regset_riscv64).f[29]); \
  GET_F30((regset_riscv64).f[30]); \
  GET_F31((regset_riscv64).f[31]); \
}

/* Set floating-point/SIMD registers from a register set. */
#define SET_FP_REGS_RISCV64( regset_riscv64 ) \
{ \
  SET_F0((regset_riscv64).f[0]); \
  SET_F1((regset_riscv64).f[1]); \
  SET_F2((regset_riscv64).f[2]); \
  SET_F3((regset_riscv64).f[3]); \
  SET_F4((regset_riscv64).f[4]); \
  SET_F5((regset_riscv64).f[5]); \
  SET_F6((regset_riscv64).f[6]); \
  SET_F7((regset_riscv64).f[7]); \
  SET_F8((regset_riscv64).f[8]); \
  SET_F9((regset_riscv64).f[9]); \
  SET_F10((regset_riscv64).f[10]); \
  SET_F11((regset_riscv64).f[11]); \
  SET_F12((regset_riscv64).f[12]); \
  SET_F13((regset_riscv64).f[13]); \
  SET_F14((regset_riscv64).f[14]); \
  SET_F15((regset_riscv64).f[15]); \
  SET_F16((regset_riscv64).f[16]); \
  SET_F17((regset_riscv64).f[17]); \
  SET_F18((regset_riscv64).f[18]); \
  SET_F19((regset_riscv64).f[19]); \
  SET_F20((regset_riscv64).f[20]); \
  SET_F21((regset_riscv64).f[21]); \
  SET_F22((regset_riscv64).f[22]); \
  SET_F23((regset_riscv64).f[23]); \
  SET_F24((regset_riscv64).f[24]); \
  SET_F25((regset_riscv64).f[25]); \
  SET_F26((regset_riscv64).f[26]); \
  SET_F27((regset_riscv64).f[27]); \
  SET_F28((regset_riscv64).f[28]); \
  SET_F29((regset_riscv64).f[29]); \
  SET_F30((regset_riscv64).f[30]); \
  SET_F31((regset_riscv64).f[31]); \
}

/*
 * Set floating-point/SIMD registers from a register set. Don't mark the
 * registers as clobbered, so the compiler won't save/restore them.
 */
#define SET_FP_REGS_NOCLOBBER_RISCV64( regset_riscv64 ) \
{ \
  SET_F0_NOCLOBBER((regset_riscv64).f[0]); \
  SET_F1_NOCLOBBER((regset_riscv64).f[1]); \
  SET_F2_NOCLOBBER((regset_riscv64).f[2]); \
  SET_F3_NOCLOBBER((regset_riscv64).f[3]); \
  SET_F4_NOCLOBBER((regset_riscv64).f[4]); \
  SET_F5_NOCLOBBER((regset_riscv64).f[5]); \
  SET_F6_NOCLOBBER((regset_riscv64).f[6]); \
  SET_F7_NOCLOBBER((regset_riscv64).f[7]); \
  SET_F8_NOCLOBBER((regset_riscv64).f[8]); \
  SET_F9_NOCLOBBER((regset_riscv64).f[9]); \
  SET_F10_NOCLOBBER((regset_riscv64).f[10]); \
  SET_F11_NOCLOBBER((regset_riscv64).f[11]); \
  SET_F12_NOCLOBBER((regset_riscv64).f[12]); \
  SET_F13_NOCLOBBER((regset_riscv64).f[13]); \
  SET_F14_NOCLOBBER((regset_riscv64).f[14]); \
  SET_F15_NOCLOBBER((regset_riscv64).f[15]); \
  SET_F16_NOCLOBBER((regset_riscv64).f[16]); \
  SET_F17_NOCLOBBER((regset_riscv64).f[17]); \
  SET_F18_NOCLOBBER((regset_riscv64).f[18]); \
  SET_F19_NOCLOBBER((regset_riscv64).f[19]); \
  SET_F20_NOCLOBBER((regset_riscv64).f[20]); \
  SET_F21_NOCLOBBER((regset_riscv64).f[21]); \
  SET_F22_NOCLOBBER((regset_riscv64).f[22]); \
  SET_F23_NOCLOBBER((regset_riscv64).f[23]); \
  SET_F24_NOCLOBBER((regset_riscv64).f[24]); \
  SET_F25_NOCLOBBER((regset_riscv64).f[25]); \
  SET_F26_NOCLOBBER((regset_riscv64).f[26]); \
  SET_F27_NOCLOBBER((regset_riscv64).f[27]); \
  SET_F28_NOCLOBBER((regset_riscv64).f[28]); \
  SET_F29_NOCLOBBER((regset_riscv64).f[29]); \
  SET_F30_NOCLOBBER((regset_riscv64).f[30]); \
  SET_F31_NOCLOBBER((regset_riscv64).f[31]); \
}

/* Set all registers from a register set. */
// Note: do not set PC, SP & x29 (FBP) as they require special handling
#define SET_REGS_RISCV64( regset_riscv64 ) \
{ \
  SET_X0((regset_riscv64).x[0]); \
  SET_X1((regset_riscv64).x[1]); \
  SET_X2((regset_riscv64).x[2]); \
  SET_X3((regset_riscv64).x[3]); \
  SET_X4((regset_riscv64).x[4]); \
  SET_X5((regset_riscv64).x[5]); \
  SET_X6((regset_riscv64).x[6]); \
  SET_X7((regset_riscv64).x[7]); \
  SET_X8((regset_riscv64).x[8]); \
  SET_X9((regset_riscv64).x[9]); \
  SET_X10((regset_riscv64).x[10]); \
  SET_X11((regset_riscv64).x[11]); \
  SET_X12((regset_riscv64).x[12]); \
  SET_X13((regset_riscv64).x[13]); \
  SET_X14((regset_riscv64).x[14]); \
  SET_X15((regset_riscv64).x[15]); \
  SET_X16((regset_riscv64).x[16]); \
  SET_X17((regset_riscv64).x[17]); \
  SET_X18((regset_riscv64).x[18]); \
  SET_X19((regset_riscv64).x[19]); \
  SET_X20((regset_riscv64).x[20]); \
  SET_X21((regset_riscv64).x[21]); \
  SET_X22((regset_riscv64).x[22]); \
  SET_X23((regset_riscv64).x[23]); \
  SET_X24((regset_riscv64).x[24]); \
  SET_X25((regset_riscv64).x[25]); \
  SET_X26((regset_riscv64).x[26]); \
  SET_X27((regset_riscv64).x[27]); \
  SET_X28((regset_riscv64).x[28]); \
  SET_X30((regset_riscv64).x[30]); \
  SET_FP_REGS_RISCV64(regset_riscv64); \
}

/* Get frame information. */
// BP == s0/x8 in RISCV
#define GET_FRAME_RISCV64( bp, sp ) GET_X8(bp); GET_SP(sp);

/* Get current frame's size, defined as x8-sp. */
#define GET_FRAME_SIZE_RISCV64( size ) \
  asm volatile("mv %0, sp; sub %0, x8, %0" : "=r" (size) )

/* Set frame after stack transformation.  Simulates function entry. */
#define SET_FRAME_RISCV64( bp, sp ) \
  asm volatile("mv sp, %0; mv x8, %1;" : : "r" (sp), "r" (bp) )

#endif /* __riscv64__ */

#endif /* _REGS_RISCV64_H */


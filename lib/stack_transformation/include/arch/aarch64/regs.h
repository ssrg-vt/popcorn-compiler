/*
 * Register definitions and macros for access for aarch64.
 *
 * DWARF register number to name mappings are derived from the ARM DWARF
 * documentation:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0057b/IHI0057B_aadwarf64.pdf
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/17/2015
 */

#ifndef _REGS_AARCH64_H
#define _REGS_AARCH64_H

///////////////////////////////////////////////////////////////////////////////
// aarch64 structure definitions
///////////////////////////////////////////////////////////////////////////////

/*
 * Defines an abstract register set for the aarch64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for arm64.
 */
struct regset_aarch64
{
  /* Stack pointer & program counter */
  void* sp;
  void* pc;

  /* General purpose registers */
  uint64_t x[31];

  /* FPU/SIMD registers */
  unsigned __int128 v[32];

  // TODO ELR_mode register
};

///////////////////////////////////////////////////////////////////////////////
// DWARF register mappings
///////////////////////////////////////////////////////////////////////////////

/* General purpose aarch64 registers */
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
#define SP 31

/* Floating-point unit (FPU)/SIMD registers */
#define V0 64
#define V1 65
#define V2 66
#define V3 67
#define V4 68
#define V5 69
#define V6 70
#define V7 71
#define V8 72
#define V9 73
#define V10 74
#define V11 75
#define V12 76
#define V13 77
#define V14 78
#define V15 79
#define V16 80
#define V17 81
#define V18 82
#define V19 83
#define V20 84
#define V21 85
#define V22 86
#define V23 87
#define V24 88
#define V25 89
#define V26 90
#define V27 91
#define V28 92
#define V29 93
#define V30 94
#define V31 95

///////////////////////////////////////////////////////////////////////////////
// Register access
///////////////////////////////////////////////////////////////////////////////

#ifdef __aarch64__

/* Getters & setters for varying registers & sizes */
#define GET_REG( var, reg, size ) asm volatile("str"size" "reg", %0" : "=m" (var) )
#define GET_REG32( var, reg ) GET_REG( var, reg, "h" )
#define GET_REG64( var, reg ) GET_REG( var, reg, "" )

#define SET_REG( var, reg, size ) asm volatile("ldr"size" "reg", %0" : : "m" (var) : reg )
#define SET_REG32( var, reg ) SET_REG( var, reg, "h" )
#define SET_REG64( var, reg ) SET_REG( var, reg, "" )

/* General-purpose aarch64 registers */
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

/*
 * The stack pointer is a little weird because you can't read it directly into/
 * write it directly from memory.  Move it into another register which can be
 * saved in memory.
 */
#define GET_SP( var ) asm volatile("mov x15, sp; str x15, %0" : "=m" (var) : : "x15")
#define SET_SP( var ) asm volatile("ldr x15, %0; mov sp, x15" : : "m" (var) : "x15")

/*
 * The program counter also can't be read directly.  The assembler replaces
 * "." or "#." with the address of the instruction.
 */
#ifdef __clang__
# define GET_PC( var ) asm volatile("adr x15, .; str x15, %0" : "=m" (var) : : "x15")
#else
# define GET_PC( var ) asm volatile("mov x15, #.; str x15, %0" : "=m" (var) : : "x15")
#endif

/*
 * The only way to set the PC is through control flow operations.
 */
#define SET_PC_IMM( val ) asm volatile("b %0" : : "i" (val) )
#define SET_PC_REG( val ) asm volatile("br %0" : : "r" (val) )

/* Floating-point unit (FPU)/SIMD registers */
#define GET_FP_REG( var, reg ) asm volatile("str "reg", %0" : "=m" (var) )
#define GET_V0( var ) GET_FP_REG( var, "q0" )
#define GET_V1( var ) GET_FP_REG( var, "q1" )
#define GET_V2( var ) GET_FP_REG( var, "q2" )
#define GET_V3( var ) GET_FP_REG( var, "q3" )
#define GET_V4( var ) GET_FP_REG( var, "q4" )
#define GET_V5( var ) GET_FP_REG( var, "q5" )
#define GET_V6( var ) GET_FP_REG( var, "q6" )
#define GET_V7( var ) GET_FP_REG( var, "q7" )
#define GET_V8( var ) GET_FP_REG( var, "q8" )
#define GET_V9( var ) GET_FP_REG( var, "q9" )
#define GET_V10( var ) GET_FP_REG( var, "q10" )
#define GET_V11( var ) GET_FP_REG( var, "q11" )
#define GET_V12( var ) GET_FP_REG( var, "q12" )
#define GET_V13( var ) GET_FP_REG( var, "q13" )
#define GET_V14( var ) GET_FP_REG( var, "q14" )
#define GET_V15( var ) GET_FP_REG( var, "q15" )
#define GET_V16( var ) GET_FP_REG( var, "q16" )
#define GET_V17( var ) GET_FP_REG( var, "q17" )
#define GET_V18( var ) GET_FP_REG( var, "q18" )
#define GET_V19( var ) GET_FP_REG( var, "q19" )
#define GET_V20( var ) GET_FP_REG( var, "q20" )
#define GET_V21( var ) GET_FP_REG( var, "q21" )
#define GET_V22( var ) GET_FP_REG( var, "q22" )
#define GET_V23( var ) GET_FP_REG( var, "q23" )
#define GET_V24( var ) GET_FP_REG( var, "q24" )
#define GET_V25( var ) GET_FP_REG( var, "q25" )
#define GET_V26( var ) GET_FP_REG( var, "q26" )
#define GET_V27( var ) GET_FP_REG( var, "q27" )
#define GET_V28( var ) GET_FP_REG( var, "q28" )
#define GET_V29( var ) GET_FP_REG( var, "q29" )
#define GET_V30( var ) GET_FP_REG( var, "q30" )
#define GET_V31( var ) GET_FP_REG( var, "q31" )

#define SET_FP_REG( var, reg, name ) asm volatile("ldr "reg", %0" : : "m" (var) : name )
#define SET_V0( var ) SET_FP_REG( var, "q0", "v0" )
#define SET_V1( var ) SET_FP_REG( var, "q1", "v1" )
#define SET_V2( var ) SET_FP_REG( var, "q2", "v2" )
#define SET_V3( var ) SET_FP_REG( var, "q3", "v3" )
#define SET_V4( var ) SET_FP_REG( var, "q4", "v4" )
#define SET_V5( var ) SET_FP_REG( var, "q5", "v5" )
#define SET_V6( var ) SET_FP_REG( var, "q6", "v6" )
#define SET_V7( var ) SET_FP_REG( var, "q7", "v7" )
#define SET_V8( var ) SET_FP_REG( var, "q8", "v8" )
#define SET_V9( var ) SET_FP_REG( var, "q9", "v9" )
#define SET_V10( var ) SET_FP_REG( var, "q10", "v10" )
#define SET_V11( var ) SET_FP_REG( var, "q11", "v11" )
#define SET_V12( var ) SET_FP_REG( var, "q12", "v12" )
#define SET_V13( var ) SET_FP_REG( var, "q13", "v13" )
#define SET_V14( var ) SET_FP_REG( var, "q14", "v14" )
#define SET_V15( var ) SET_FP_REG( var, "q15", "v15" )
#define SET_V16( var ) SET_FP_REG( var, "q16", "v16" )
#define SET_V17( var ) SET_FP_REG( var, "q17", "v17" )
#define SET_V18( var ) SET_FP_REG( var, "q18", "v18" )
#define SET_V19( var ) SET_FP_REG( var, "q19", "v19" )
#define SET_V20( var ) SET_FP_REG( var, "q20", "v20" )
#define SET_V21( var ) SET_FP_REG( var, "q21", "v21" )
#define SET_V22( var ) SET_FP_REG( var, "q22", "v22" )
#define SET_V23( var ) SET_FP_REG( var, "q23", "v23" )
#define SET_V24( var ) SET_FP_REG( var, "q24", "v24" )
#define SET_V25( var ) SET_FP_REG( var, "q25", "v25" )
#define SET_V26( var ) SET_FP_REG( var, "q26", "v26" )
#define SET_V27( var ) SET_FP_REG( var, "q27", "v27" )
#define SET_V28( var ) SET_FP_REG( var, "q28", "v28" )
#define SET_V29( var ) SET_FP_REG( var, "q29", "v29" )
#define SET_V30( var ) SET_FP_REG( var, "q30", "v30" )
#define SET_V31( var ) SET_FP_REG( var, "q31", "v31" )

// Note: the following NOCLOBBER macros are only used for special cases, use
// the macros above for normal access
#define SET_FP_REG_NOCLOBBER( var, reg ) asm volatile("ldr "reg", %0" : : "m" (var) )
#define SET_V0_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q0" )
#define SET_V1_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q1" )
#define SET_V2_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q2" )
#define SET_V3_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q3" )
#define SET_V4_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q4" )
#define SET_V5_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q5" )
#define SET_V6_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q6" )
#define SET_V7_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q7" )
#define SET_V8_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q8" )
#define SET_V9_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q9" )
#define SET_V10_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q10" )
#define SET_V11_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q11" )
#define SET_V12_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q12" )
#define SET_V13_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q13" )
#define SET_V14_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q14" )
#define SET_V15_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q15" )
#define SET_V16_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q16" )
#define SET_V17_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q17" )
#define SET_V18_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q18" )
#define SET_V19_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q19" )
#define SET_V20_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q20" )
#define SET_V21_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q21" )
#define SET_V22_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q22" )
#define SET_V23_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q23" )
#define SET_V24_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q24" )
#define SET_V25_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q25" )
#define SET_V26_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q26" )
#define SET_V27_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q27" )
#define SET_V28_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q28" )
#define SET_V29_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q29" )
#define SET_V30_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q30" )
#define SET_V31_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "q31" )

/* Read all registers into a register set. */
#define READ_REGS_AARCH64( regset_aarch64 ) \
{ \
  GET_X0((regset_aarch64).x[0]); \
  GET_X1((regset_aarch64).x[1]); \
  GET_X2((regset_aarch64).x[2]); \
  GET_X3((regset_aarch64).x[3]); \
  GET_X4((regset_aarch64).x[4]); \
  GET_X5((regset_aarch64).x[5]); \
  GET_X6((regset_aarch64).x[6]); \
  GET_X7((regset_aarch64).x[7]); \
  GET_X8((regset_aarch64).x[8]); \
  GET_X9((regset_aarch64).x[9]); \
  GET_X10((regset_aarch64).x[10]); \
  GET_X11((regset_aarch64).x[11]); \
  GET_X12((regset_aarch64).x[12]); \
  GET_X13((regset_aarch64).x[13]); \
  GET_X14((regset_aarch64).x[14]); \
  GET_X15((regset_aarch64).x[15]); \
  GET_X16((regset_aarch64).x[16]); \
  GET_X17((regset_aarch64).x[17]); \
  GET_X18((regset_aarch64).x[18]); \
  GET_X19((regset_aarch64).x[19]); \
  GET_X20((regset_aarch64).x[20]); \
  GET_X21((regset_aarch64).x[21]); \
  GET_X22((regset_aarch64).x[22]); \
  GET_X23((regset_aarch64).x[23]); \
  GET_X24((regset_aarch64).x[24]); \
  GET_X25((regset_aarch64).x[25]); \
  GET_X26((regset_aarch64).x[26]); \
  GET_X27((regset_aarch64).x[27]); \
  GET_X28((regset_aarch64).x[28]); \
  GET_X29((regset_aarch64).x[29]); \
  GET_X30((regset_aarch64).x[30]); \
  GET_SP((regset_aarch64).sp); \
  GET_PC((regset_aarch64).pc); \
  GET_V0((regset_aarch64).v[0]); \
  GET_V1((regset_aarch64).v[1]); \
  GET_V2((regset_aarch64).v[2]); \
  GET_V3((regset_aarch64).v[3]); \
  GET_V4((regset_aarch64).v[4]); \
  GET_V5((regset_aarch64).v[5]); \
  GET_V6((regset_aarch64).v[6]); \
  GET_V7((regset_aarch64).v[7]); \
  GET_V8((regset_aarch64).v[8]); \
  GET_V9((regset_aarch64).v[9]); \
  GET_V10((regset_aarch64).v[10]); \
  GET_V11((regset_aarch64).v[11]); \
  GET_V12((regset_aarch64).v[12]); \
  GET_V13((regset_aarch64).v[13]); \
  GET_V14((regset_aarch64).v[14]); \
  GET_V15((regset_aarch64).v[15]); \
  GET_V16((regset_aarch64).v[16]); \
  GET_V17((regset_aarch64).v[17]); \
  GET_V18((regset_aarch64).v[18]); \
  GET_V19((regset_aarch64).v[19]); \
  GET_V20((regset_aarch64).v[20]); \
  GET_V21((regset_aarch64).v[21]); \
  GET_V22((regset_aarch64).v[22]); \
  GET_V23((regset_aarch64).v[23]); \
  GET_V24((regset_aarch64).v[24]); \
  GET_V25((regset_aarch64).v[25]); \
  GET_V26((regset_aarch64).v[26]); \
  GET_V27((regset_aarch64).v[27]); \
  GET_V28((regset_aarch64).v[28]); \
  GET_V29((regset_aarch64).v[29]); \
  GET_V30((regset_aarch64).v[30]); \
  GET_V31((regset_aarch64).v[31]); \
}

/* Set floating-point/SIMD registers from a register set. */
#define SET_FP_REGS_AARCH64( regset_aarch64 ) \
{ \
  SET_V0((regset_aarch64).v[0]); \
  SET_V1((regset_aarch64).v[1]); \
  SET_V2((regset_aarch64).v[2]); \
  SET_V3((regset_aarch64).v[3]); \
  SET_V4((regset_aarch64).v[4]); \
  SET_V5((regset_aarch64).v[5]); \
  SET_V6((regset_aarch64).v[6]); \
  SET_V7((regset_aarch64).v[7]); \
  SET_V8((regset_aarch64).v[8]); \
  SET_V9((regset_aarch64).v[9]); \
  SET_V10((regset_aarch64).v[10]); \
  SET_V11((regset_aarch64).v[11]); \
  SET_V12((regset_aarch64).v[12]); \
  SET_V13((regset_aarch64).v[13]); \
  SET_V14((regset_aarch64).v[14]); \
  SET_V15((regset_aarch64).v[15]); \
  SET_V16((regset_aarch64).v[16]); \
  SET_V17((regset_aarch64).v[17]); \
  SET_V18((regset_aarch64).v[18]); \
  SET_V19((regset_aarch64).v[19]); \
  SET_V20((regset_aarch64).v[20]); \
  SET_V21((regset_aarch64).v[21]); \
  SET_V22((regset_aarch64).v[22]); \
  SET_V23((regset_aarch64).v[23]); \
  SET_V24((regset_aarch64).v[24]); \
  SET_V25((regset_aarch64).v[25]); \
  SET_V26((regset_aarch64).v[26]); \
  SET_V27((regset_aarch64).v[27]); \
  SET_V28((regset_aarch64).v[28]); \
  SET_V29((regset_aarch64).v[29]); \
  SET_V30((regset_aarch64).v[30]); \
  SET_V31((regset_aarch64).v[31]); \
}

/*
 * Set floating-point/SIMD registers from a register set. Don't mark the
 * registers as clobbered, so the compiler won't save/restore them.
 */
#define SET_FP_REGS_NOCLOBBER_AARCH64( regset_aarch64 ) \
{ \
  SET_V0_NOCLOBBER((regset_aarch64).v[0]); \
  SET_V1_NOCLOBBER((regset_aarch64).v[1]); \
  SET_V2_NOCLOBBER((regset_aarch64).v[2]); \
  SET_V3_NOCLOBBER((regset_aarch64).v[3]); \
  SET_V4_NOCLOBBER((regset_aarch64).v[4]); \
  SET_V5_NOCLOBBER((regset_aarch64).v[5]); \
  SET_V6_NOCLOBBER((regset_aarch64).v[6]); \
  SET_V7_NOCLOBBER((regset_aarch64).v[7]); \
  SET_V8_NOCLOBBER((regset_aarch64).v[8]); \
  SET_V9_NOCLOBBER((regset_aarch64).v[9]); \
  SET_V10_NOCLOBBER((regset_aarch64).v[10]); \
  SET_V11_NOCLOBBER((regset_aarch64).v[11]); \
  SET_V12_NOCLOBBER((regset_aarch64).v[12]); \
  SET_V13_NOCLOBBER((regset_aarch64).v[13]); \
  SET_V14_NOCLOBBER((regset_aarch64).v[14]); \
  SET_V15_NOCLOBBER((regset_aarch64).v[15]); \
  SET_V16_NOCLOBBER((regset_aarch64).v[16]); \
  SET_V17_NOCLOBBER((regset_aarch64).v[17]); \
  SET_V18_NOCLOBBER((regset_aarch64).v[18]); \
  SET_V19_NOCLOBBER((regset_aarch64).v[19]); \
  SET_V20_NOCLOBBER((regset_aarch64).v[20]); \
  SET_V21_NOCLOBBER((regset_aarch64).v[21]); \
  SET_V22_NOCLOBBER((regset_aarch64).v[22]); \
  SET_V23_NOCLOBBER((regset_aarch64).v[23]); \
  SET_V24_NOCLOBBER((regset_aarch64).v[24]); \
  SET_V25_NOCLOBBER((regset_aarch64).v[25]); \
  SET_V26_NOCLOBBER((regset_aarch64).v[26]); \
  SET_V27_NOCLOBBER((regset_aarch64).v[27]); \
  SET_V28_NOCLOBBER((regset_aarch64).v[28]); \
  SET_V29_NOCLOBBER((regset_aarch64).v[29]); \
  SET_V30_NOCLOBBER((regset_aarch64).v[30]); \
  SET_V31_NOCLOBBER((regset_aarch64).v[31]); \
}

/* Set all registers from a register set. */
// Note: do not set PC, SP & x29 (FBP) as they require special handling
#define SET_REGS_AARCH64( regset_aarch64 ) \
{ \
  SET_X0((regset_aarch64).x[0]); \
  SET_X1((regset_aarch64).x[1]); \
  SET_X2((regset_aarch64).x[2]); \
  SET_X3((regset_aarch64).x[3]); \
  SET_X4((regset_aarch64).x[4]); \
  SET_X5((regset_aarch64).x[5]); \
  SET_X6((regset_aarch64).x[6]); \
  SET_X7((regset_aarch64).x[7]); \
  SET_X8((regset_aarch64).x[8]); \
  SET_X9((regset_aarch64).x[9]); \
  SET_X10((regset_aarch64).x[10]); \
  SET_X11((regset_aarch64).x[11]); \
  SET_X12((regset_aarch64).x[12]); \
  SET_X13((regset_aarch64).x[13]); \
  SET_X14((regset_aarch64).x[14]); \
  SET_X15((regset_aarch64).x[15]); \
  SET_X16((regset_aarch64).x[16]); \
  SET_X17((regset_aarch64).x[17]); \
  SET_X18((regset_aarch64).x[18]); \
  SET_X19((regset_aarch64).x[19]); \
  SET_X20((regset_aarch64).x[20]); \
  SET_X21((regset_aarch64).x[21]); \
  SET_X22((regset_aarch64).x[22]); \
  SET_X23((regset_aarch64).x[23]); \
  SET_X24((regset_aarch64).x[24]); \
  SET_X25((regset_aarch64).x[25]); \
  SET_X26((regset_aarch64).x[26]); \
  SET_X27((regset_aarch64).x[27]); \
  SET_X28((regset_aarch64).x[28]); \
  SET_X30((regset_aarch64).x[30]); \
  SET_FP_REGS_AARCH64(regset_aarch64); \
}

/* Get frame information. */
#define GET_FRAME_AARCH64( bp, sp ) GET_X29(bp); GET_SP(sp);

/* Get current frame's size, defined as x29-sp. */
#define GET_FRAME_SIZE_AARCH64( size ) \
  asm volatile("mov %0, sp; sub %0, x29, %0" : "=r" (size) )

/* Set frame after stack transformation.  Simulates function entry. */
#define SET_FRAME_AARCH64( bp, sp ) \
  asm volatile("mov sp, %0; mov x29, %1;" : : "r" (sp), "r" (bp) )

#endif /* __aarch64__ */

#endif /* _REGS_AARCH64_H */


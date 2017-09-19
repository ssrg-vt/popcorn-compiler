/*
 * Register definitions and macros for access for powerpc64.
 *
 * DWARF register number to name mappings are derived from the powerpc64 ABI v1.4 (march 2017)
 * documentation:
 * PowerPC_64bit_v2ABI_specification_rev1.4.pdf from IBM's site
 *
 * Author: Buse Yilmaz <busey@vt.edu>
 * Date: 05/06/2017
 */

#ifndef _REGS_POWERPC64_H
#define _REGS_POWERPC64_H

///////////////////////////////////////////////////////////////////////////////
// powerpc64 structure definitions
///////////////////////////////////////////////////////////////////////////////

/*
 * Defines an abstract register set for the powerpc64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for powerpc64.
 */
struct regset_powerpc64
{
  // powerpc doesn't have an explicit program counter / instruction pointer
  /* NOTES:     */
  /* r1:    SP  */
  /* r2:    TOC */
  /* r31:   FBP  */
  /* Link Register, Count Register */ 
  void* pc;
  void* lr;
  void* ctr;

  /* General purpose registers 64-bit*/
  uint64_t r[32];

  /* Floating Point registers (FPR) = VSR[0-31],[0-63] */
  /* Vector Registers (VR) = VSR[32-63],[0-127] */
  /* Vector-Scalar Registers (VR) */
  
  /* Floating point registers and vector registers 
   * physically reside in vector-scalar registers.
   * For simplicity, the floating-point registers
   * are used.
   */
//  unsigned __int128 vsr[64];
  uint64_t f[32];
};


///////////////////////////////////////////////////////////////////////////////
// DWARF register mappings
///////////////////////////////////////////////////////////////////////////////

/* General purpose powerpc64 registers */
#define R0 0
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R6 6
#define R7 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15
#define R16 16
#define R17 17
#define R18 18
#define R19 19
#define R20 20
#define R21 21
#define R22 22
#define R23 23
#define R24 24
#define R25 25
#define R26 26
#define R27 27
#define R28 28
#define R29 29
#define R30 30
#define R31 31

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

/* Vector Registers */
#define VR0 77
#define VR1 78
#define VR2 79
#define VR3 80
#define VR4 81
#define VR5 82
#define VR6 83
#define VR7 84
#define VR8 85
#define VR9 86
#define VR10 87
#define VR11 88
#define VR12 89
#define VR13 90
#define VR14 91
#define VR15 92
#define VR16 93
#define VR17 94
#define VR18 95
#define VR19 96
#define VR20 97
#define VR21 98
#define VR22 99
#define VR23 100
#define VR24 101
#define VR25 102
#define VR26 103
#define VR27 104
#define VR28 105
#define VR29 106
#define VR30 107
#define VR31 108

/* Other Registers */
#define LR 65
#define CTR 66
#define CR0 68
#define CR1 69
#define CR2 70
#define CR3 71
#define CR4 72
#define CR5 73
#define CR6 74
#define CR7 75
#define XER 76
#define VSCR 110


///////////////////////////////////////////////////////////////////////////////
// Register access
///////////////////////////////////////////////////////////////////////////////

#ifdef __powerpc64__

/* Getters & setters for varying registers & sizes */

/* op variable stands for either of the following:
 * a: algebraic
 * z: zero-padded
 * u: update
 * x: indexed
 * br: byte-reverse       v2.06
 * at: atomic             v3.0
*/

/* (a|z|u|(a&u)|(z&u)|ar|br) & x */
/* skipping instructions only valid in HV mode (hypervisor): cix */
/* skipping instructions used for synchronization: arx */

// DS and X are instruction formats
// DS: opcode RT,DS(RA)  EA=(RA|0)+D
// X : opcode RT,RA,RB   EA=(RA|0)+(RB)
#define GET_REG_X( var1, var2, reg, size) asm volatile("st"size"zx "reg",%0,%1" : "=m" (var1), "=m" (var2) )
#define GET_REG_DS( var, reg, size ) asm volatile("st"size"z "reg",%0" : "=m" (var) )

#define GET_REG8_z( var, reg ) GET_REG_DS( var, reg, "b" )
#define GET_REG8_zx( var1, var2, reg ) GET_REG_X( var1, var2, reg, "b" )

#define GET_REG16_z( var, reg ) GET_REG_DS( var, reg, "h" )
#define GET_REG16_zx( var1, var2, reg ) GET_REG_X( var1, var2, reg, "h")

#define GET_REG32_z( var, reg ) GET_REG_DS( var, reg, "w" )
#define GET_REG32_zx( var1, var2, reg ) GET_REG_X( var1, var2, reg, "w")

#define GET_REG64( var, reg ) asm volatile("std "reg",%0" : "=m" (var) )
#define GET_REG64_x( var1, var2, reg ) asm volatile("st"size"x "reg",%0,%1" : "=m" (var1), "=m"(var2) )

#define GET_REG128( var, reg ) GET_REG_DS( var, reg, "q" )


#define SET_REG_X( var1, var2, reg, size) asm volatile("l"size"zx" reg",%0,%1" : : "g" (var1), "g" (var2) : reg )
#define SET_REG_DS( var, reg, size ) asm volatile("l"size"z "reg",%0" : : "g" (var) : reg )

#define SET_REG8_z( var, reg ) SET_REG_DS( var, reg, "b" )
#define SET_REG8_zx( var1, var2, reg ) SET_REG_X( var1, var2, reg, "b" )

#define SET_REG16_z( var, reg ) SET_REG_DS( var, reg, "h" )
#define SET_REG16_zx( var1, var2, reg ) SET_REG_X( var1, var2, reg, "h")

#define SET_REG32_z( var, reg ) SET_REG_DS( var, reg, "w" )
#define SET_REG32_zx( var1, var2, reg ) SET_REG_X( var1, var2, reg, "w")

#define SET_REG64( var, reg ) asm volatile("ld "reg",%0" : : "m" (var) : reg )
//#define SET_REG64_x( var1, var2, reg ) asm volatile("l"size"zx "reg", %0,%1" : : "m" (var1), "m" (var2) : reg )

#define SET_REG128( var, reg ) GET_REG_DS( var, reg, "q" )


/* General-purpose powerpc64 registers */
#define GET_R0( var ) GET_REG64( var, "0" )
#define GET_R1( var ) GET_REG64( var, "1" )
#define GET_R2( var ) GET_REG64( var, "2" )
#define GET_R3( var ) GET_REG64( var, "3" )
#define GET_R4( var ) GET_REG64( var, "4" )
#define GET_R5( var ) GET_REG64( var, "5" )
#define GET_R6( var ) GET_REG64( var, "6" )
#define GET_R7( var ) GET_REG64( var, "7" )
#define GET_R8( var ) GET_REG64( var, "8" )
#define GET_R9( var ) GET_REG64( var, "9" )
#define GET_R10( var ) GET_REG64( var, "10" )
#define GET_R11( var ) GET_REG64( var, "11" )
#define GET_R12( var ) GET_REG64( var, "12" )
#define GET_R13( var ) GET_REG64( var, "13" )
#define GET_R14( var ) GET_REG64( var, "14" )
#define GET_R15( var ) GET_REG64( var, "15" )
#define GET_R16( var ) GET_REG64( var, "16" )
#define GET_R17( var ) GET_REG64( var, "17" )
#define GET_R18( var ) GET_REG64( var, "18" )
#define GET_R19( var ) GET_REG64( var, "19" )
#define GET_R20( var ) GET_REG64( var, "20" )
#define GET_R21( var ) GET_REG64( var, "21" )
#define GET_R22( var ) GET_REG64( var, "22" )
#define GET_R23( var ) GET_REG64( var, "23" )
#define GET_R24( var ) GET_REG64( var, "24" )
#define GET_R25( var ) GET_REG64( var, "25" )
#define GET_R26( var ) GET_REG64( var, "26" )
#define GET_R27( var ) GET_REG64( var, "27" )
#define GET_R28( var ) GET_REG64( var, "28" )
#define GET_R29( var ) GET_REG64( var, "29" )
#define GET_R30( var ) GET_REG64( var, "30" )
#define GET_R31( var ) GET_REG64( var, "31" )

#define SET_R0( var ) SET_REG64( var, "0" )
#define SET_R1( var ) SET_REG64( var, "1" )
#define SET_R2( var ) SET_REG64( var, "2" )
#define SET_R3( var ) SET_REG64( var, "3" )
#define SET_R4( var ) SET_REG64( var, "4" )
#define SET_R5( var ) SET_REG64( var, "5" )
#define SET_R6( var ) SET_REG64( var, "6" )
#define SET_R7( var ) SET_REG64( var, "7" )
#define SET_R8( var ) SET_REG64( var, "8" )
#define SET_R9( var ) SET_REG64( var, "9" )
#define SET_R10( var ) SET_REG64( var, "10" )
#define SET_R11( var ) SET_REG64( var, "11" )
#define SET_R12( var ) SET_REG64( var, "12" )
#define SET_R13( var ) SET_REG64( var, "13" )
#define SET_R14( var ) SET_REG64( var, "14" )
#define SET_R15( var ) SET_REG64( var, "15" )
#define SET_R16( var ) SET_REG64( var, "16" )
#define SET_R17( var ) SET_REG64( var, "17" )
#define SET_R18( var ) SET_REG64( var, "18" )
#define SET_R19( var ) SET_REG64( var, "19" )
#define SET_R20( var ) SET_REG64( var, "20" )
#define SET_R21( var ) SET_REG64( var, "21" )
#define SET_R22( var ) SET_REG64( var, "22" )
#define SET_R23( var ) SET_REG64( var, "23" )
#define SET_R24( var ) SET_REG64( var, "24" )
#define SET_R25( var ) SET_REG64( var, "25" )
#define SET_R26( var ) SET_REG64( var, "26" )
#define SET_R27( var ) SET_REG64( var, "27" )
#define SET_R28( var ) SET_REG64( var, "28" )
#define SET_R29( var ) SET_REG64( var, "29" )
#define SET_R30( var ) SET_REG64( var, "30" )
#define SET_R31( var ) SET_REG64( var, "31" )



/*
 * The stack pointer is directly accesible for powerpc.
 */
#define GET_SP( var ) asm volatile( "std 1, %0" : "=m" (var) : )
#define SET_SP( var ) asm volatile( "ld 1, %0" : : "m" (var) : "r1" )

/*
 * The program counter cannot be read directly for powerpc. However we can get
 * the program counter using branch instructions and LR (Link Register). 
 * PC (Program Counter) is also referred as 
 * CIA (Current Instruction Address) in Power Parlance. 
 */

#define GET_PC( var ) asm volatile( "mflr 4\n\t"        \
                                    "bcl 20,31,$+4\n\t" \
                                    "mflr 5\n\t"        \
                                    "subi 5,5,4\n\t"    \
                                    "std 5,%0\n\t"      \
                                    "mtlr 4\n\t"        \
                                    : "=m" (var) : : "r4", "r5" )


/* Getters & Setters for LR and CTR. LR is stored in the caller's frame at SP+16 */
#define GET_LR( var )  asm volatile( "mflr 22; std 22,%0" : "=m" (var) : : "r22" )
#define GET_CTR( var )  asm volatile( "mfctr 22; std 22,%0" : "=m" (var) : : "r22" )

#define SET_SAVED_LR( )  asm volatile( "ld 17,0(1) ; ld 17,16(17)" : : : "r17" )
#define SET_LR( var )  asm volatile( "ld 17,%0 ; mtlr 17" : : "m" (var) : "r17" )
#define SET_CTR( var ) asm volatile( "ld 17,%0 ; mtctr 17" : : "m" (var) : "r17" )

/*
 * The only way to set the PC is through control flow operations.
 */
//#define SET_PC_IMM( val ) asm volatile("bcl 20,31,%0" : : "i" (val) )
#define SET_PC_IMM( val ) asm volatile("b %0" : : "i" (val) );
#define SET_PC_REG( val ) asm volatile("mflr 4; mtlr %0 ; bclrl 20,31"  : : "r" (val): "r4" );

/* Floating-point unit (FPU)/SIMD registers */
#define GET_FP_REG( var, reg ) asm volatile("stfd "reg",%0" : "=m" (var) )
#define GET_F0( var ) GET_FP_REG( var, "0" )
#define GET_F1( var ) GET_FP_REG( var, "1" )
#define GET_F2( var ) GET_FP_REG( var, "2" )
#define GET_F3( var ) GET_FP_REG( var, "3" )
#define GET_F4( var ) GET_FP_REG( var, "4" )
#define GET_F5( var ) GET_FP_REG( var, "5" )
#define GET_F6( var ) GET_FP_REG( var, "6" )
#define GET_F7( var ) GET_FP_REG( var, "7" )
#define GET_F8( var ) GET_FP_REG( var, "8" )
#define GET_F9( var ) GET_FP_REG( var, "9" )
#define GET_F10( var ) GET_FP_REG( var, "10" )
#define GET_F11( var ) GET_FP_REG( var, "11" )
#define GET_F12( var ) GET_FP_REG( var, "12" )
#define GET_F13( var ) GET_FP_REG( var, "13" )
#define GET_F14( var ) GET_FP_REG( var, "14" )
#define GET_F15( var ) GET_FP_REG( var, "15" )
#define GET_F16( var ) GET_FP_REG( var, "16" )
#define GET_F17( var ) GET_FP_REG( var, "17" )
#define GET_F18( var ) GET_FP_REG( var, "18" )
#define GET_F19( var ) GET_FP_REG( var, "19" )
#define GET_F20( var ) GET_FP_REG( var, "20" )
#define GET_F21( var ) GET_FP_REG( var, "21" )
#define GET_F22( var ) GET_FP_REG( var, "22" )
#define GET_F23( var ) GET_FP_REG( var, "23" )
#define GET_F24( var ) GET_FP_REG( var, "24" )
#define GET_F25( var ) GET_FP_REG( var, "25" )
#define GET_F26( var ) GET_FP_REG( var, "26" )
#define GET_F27( var ) GET_FP_REG( var, "27" )
#define GET_F28( var ) GET_FP_REG( var, "28" )
#define GET_F29( var ) GET_FP_REG( var, "29" )
#define GET_F30( var ) GET_FP_REG( var, "30" )
#define GET_F31( var ) GET_FP_REG( var, "31" )

// TODO couldn't set reg as clobber
//#define SET_FP_REG( var, reg, name ) asm volatile("lfd "reg",%0" : : "m" (var) : name )
//#define SET_F0( var ) SET_FP_REG( var, "0", "v0" )
//#define SET_F1( var ) SET_FP_REG( var, "1", "v1" )
//#define SET_F2( var ) SET_FP_REG( var, "2", "v2" )
//#define SET_F3( var ) SET_FP_REG( var, "3", "v3" )
//#define SET_F4( var ) SET_FP_REG( var, "4", "v4" )
//#define SET_F5( var ) SET_FP_REG( var, "5", "v5" )
//#define SET_F6( var ) SET_FP_REG( var, "6", "v6" )
//#define SET_F7( var ) SET_FP_REG( var, "7", "v7" )
//#define SET_F8( var ) SET_FP_REG( var, "8", "v8" )
//#define SET_F9( var ) SET_FP_REG( var, "9", "v9" )
//#define SET_F10( var ) SET_FP_REG( var, "10", "v10" )
//#define SET_F11( var ) SET_FP_REG( var, "11", "v11" )
//#define SET_F12( var ) SET_FP_REG( var, "12", "v12" )
//#define SET_F13( var ) SET_FP_REG( var, "13", "v13" )
//#define SET_F14( var ) SET_FP_REG( var, "14", "v14" )
//#define SET_F15( var ) SET_FP_REG( var, "15", "v15" )
//#define SET_F16( var ) SET_FP_REG( var, "16", "v16" )
//#define SET_F17( var ) SET_FP_REG( var, "17", "v17" )
//#define SET_F18( var ) SET_FP_REG( var, "18", "v18" )
//#define SET_F19( var ) SET_FP_REG( var, "19", "v19" )
//#define SET_F20( var ) SET_FP_REG( var, "20", "v20" )
//#define SET_F21( var ) SET_FP_REG( var, "21", "v21" )
//#define SET_F22( var ) SET_FP_REG( var, "22", "v22" )
//#define SET_F23( var ) SET_FP_REG( var, "23", "v23" )
//#define SET_F24( var ) SET_FP_REG( var, "24", "v24" )
//#define SET_F25( var ) SET_FP_REG( var, "25", "v25" )
//#define SET_F26( var ) SET_FP_REG( var, "26", "v26" )
//#define SET_F27( var ) SET_FP_REG( var, "27", "v27" )
//#define SET_F28( var ) SET_FP_REG( var, "28", "v28" )
//#define SET_F29( var ) SET_FP_REG( var, "29", "v29" )
//#define SET_F30( var ) SET_FP_REG( var, "30", "v30" )
//#define SET_F31( var ) SET_FP_REG( var, "31", "v31" )
#define SET_FP_REG( var, reg ) asm volatile("lfd "reg",%0" : : "m" (var) :)
#define SET_F0( var ) SET_FP_REG( var, "0" )
#define SET_F1( var ) SET_FP_REG( var, "1" )
#define SET_F2( var ) SET_FP_REG( var, "2" )
#define SET_F3( var ) SET_FP_REG( var, "3" )
#define SET_F4( var ) SET_FP_REG( var, "4" )
#define SET_F5( var ) SET_FP_REG( var, "5" )
#define SET_F6( var ) SET_FP_REG( var, "6" )
#define SET_F7( var ) SET_FP_REG( var, "7" )
#define SET_F8( var ) SET_FP_REG( var, "8" )
#define SET_F9( var ) SET_FP_REG( var, "9" )
#define SET_F10( var ) SET_FP_REG( var, "10" )
#define SET_F11( var ) SET_FP_REG( var, "11" )
#define SET_F12( var ) SET_FP_REG( var, "12" )
#define SET_F13( var ) SET_FP_REG( var, "13" )
#define SET_F14( var ) SET_FP_REG( var, "14" )
#define SET_F15( var ) SET_FP_REG( var, "15" )
#define SET_F16( var ) SET_FP_REG( var, "16" )
#define SET_F17( var ) SET_FP_REG( var, "17" )
#define SET_F18( var ) SET_FP_REG( var, "18" )
#define SET_F19( var ) SET_FP_REG( var, "19" )
#define SET_F20( var ) SET_FP_REG( var, "20" )
#define SET_F21( var ) SET_FP_REG( var, "21" )
#define SET_F22( var ) SET_FP_REG( var, "22" )
#define SET_F23( var ) SET_FP_REG( var, "23" )
#define SET_F24( var ) SET_FP_REG( var, "24" )
#define SET_F25( var ) SET_FP_REG( var, "25" )
#define SET_F26( var ) SET_FP_REG( var, "26" )
#define SET_F27( var ) SET_FP_REG( var, "27" )
#define SET_F28( var ) SET_FP_REG( var, "28" )
#define SET_F29( var ) SET_FP_REG( var, "29" )
#define SET_F30( var ) SET_FP_REG( var, "30" )
#define SET_F31( var ) SET_FP_REG( var, "31" )

// Note: the following NOCLOBBER macros are only used for special cases, use
// the macros above for normal access
#define SET_FP_REG_NOCLOBBER( var, reg ) asm volatile("lfd "reg",%0" : : "m" (var) )
#define SET_F0_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "0" )
#define SET_F1_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "1" )
#define SET_F2_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "2" )
#define SET_F3_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "3" )
#define SET_F4_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "4" )
#define SET_F5_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "5" )
#define SET_F6_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "6" )
#define SET_F7_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "7" )
#define SET_F8_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "8" )
#define SET_F9_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "9" )
#define SET_F10_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "10" )
#define SET_F11_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "11" )
#define SET_F12_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "12" )
#define SET_F13_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "13" )
#define SET_F14_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "14" )
#define SET_F15_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "15" )
#define SET_F16_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "16" )
#define SET_F17_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "17" )
#define SET_F18_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "18" )
#define SET_F19_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "19" )
#define SET_F20_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "20" )
#define SET_F21_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "21" )
#define SET_F22_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "22" )
#define SET_F23_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "23" )
#define SET_F24_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "24" )
#define SET_F25_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "25" )
#define SET_F26_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "26" )
#define SET_F27_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "27" )
#define SET_F28_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "28" )
#define SET_F29_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "29" )
#define SET_F30_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "30" )
#define SET_F31_NOCLOBBER( var ) SET_FP_REG_NOCLOBBER( var, "31" )

//  GET_SP((regset_powerpc64).sp);

/* Read all registers into a register set. */
#define READ_REGS_POWERPC64( regset_powerpc64 ) \
{ \
  GET_R0((regset_powerpc64).r[0]); \
  GET_R1((regset_powerpc64).r[1]); \
  GET_R2((regset_powerpc64).r[2]); \
  GET_R3((regset_powerpc64).r[3]); \
  GET_R4((regset_powerpc64).r[4]); \
  GET_R5((regset_powerpc64).r[5]); \
  GET_R6((regset_powerpc64).r[6]); \
  GET_R7((regset_powerpc64).r[7]); \
  GET_R8((regset_powerpc64).r[8]); \
  GET_R9((regset_powerpc64).r[9]); \
  GET_R10((regset_powerpc64).r[10]); \
  GET_R11((regset_powerpc64).r[11]); \
  GET_R12((regset_powerpc64).r[12]); \
  GET_R13((regset_powerpc64).r[13]); \
  GET_R14((regset_powerpc64).r[14]); \
  GET_R15((regset_powerpc64).r[15]); \
  GET_R16((regset_powerpc64).r[16]); \
  GET_R17((regset_powerpc64).r[17]); \
  GET_R18((regset_powerpc64).r[18]); \
  GET_R19((regset_powerpc64).r[19]); \
  GET_R20((regset_powerpc64).r[20]); \
  GET_R21((regset_powerpc64).r[21]); \
  GET_R22((regset_powerpc64).r[22]); \
  GET_R23((regset_powerpc64).r[23]); \
  GET_R24((regset_powerpc64).r[24]); \
  GET_R25((regset_powerpc64).r[25]); \
  GET_R26((regset_powerpc64).r[26]); \
  GET_R27((regset_powerpc64).r[27]); \
  GET_R28((regset_powerpc64).r[28]); \
  GET_R29((regset_powerpc64).r[29]); \
  GET_R30((regset_powerpc64).r[30]); \
  GET_R31((regset_powerpc64).r[31]); \
  GET_PC((regset_powerpc64).pc); \
  GET_LR((regset_powerpc64).lr); \
  GET_CTR((regset_powerpc64).ctr); \
  GET_F0((regset_powerpc64).f[0]); \
  GET_F1((regset_powerpc64).f[1]); \
  GET_F2((regset_powerpc64).f[2]); \
  GET_F3((regset_powerpc64).f[3]); \
  GET_F4((regset_powerpc64).f[4]); \
  GET_F5((regset_powerpc64).f[5]); \
  GET_F6((regset_powerpc64).f[6]); \
  GET_F7((regset_powerpc64).f[7]); \
  GET_F8((regset_powerpc64).f[8]); \
  GET_F9((regset_powerpc64).f[9]); \
  GET_F10((regset_powerpc64).f[10]); \
  GET_F11((regset_powerpc64).f[11]); \
  GET_F12((regset_powerpc64).f[12]); \
  GET_F13((regset_powerpc64).f[13]); \
  GET_F14((regset_powerpc64).f[14]); \
  GET_F15((regset_powerpc64).f[15]); \
  GET_F16((regset_powerpc64).f[16]); \
  GET_F17((regset_powerpc64).f[17]); \
  GET_F18((regset_powerpc64).f[18]); \
  GET_F19((regset_powerpc64).f[19]); \
  GET_F20((regset_powerpc64).f[20]); \
  GET_F21((regset_powerpc64).f[21]); \
  GET_F22((regset_powerpc64).f[22]); \
  GET_F23((regset_powerpc64).f[23]); \
  GET_F24((regset_powerpc64).f[24]); \
  GET_F25((regset_powerpc64).f[25]); \
  GET_F26((regset_powerpc64).f[26]); \
  GET_F27((regset_powerpc64).f[27]); \
  GET_F28((regset_powerpc64).f[28]); \
  GET_F29((regset_powerpc64).f[29]); \
  GET_F30((regset_powerpc64).f[30]); \
  GET_F31((regset_powerpc64).f[31]); \
}

/* Set floating-point/SIMD registers from a register set. */
#define SET_FP_REGS_POWERPC64( regset_powerpc64 ) \
{ \
  SET_F0((regset_powerpc64).f[0]); \
  SET_F1((regset_powerpc64).f[1]); \
  SET_F2((regset_powerpc64).f[2]); \
  SET_F3((regset_powerpc64).f[3]); \
  SET_F4((regset_powerpc64).f[4]); \
  SET_F5((regset_powerpc64).f[5]); \
  SET_F6((regset_powerpc64).f[6]); \
  SET_F7((regset_powerpc64).f[7]); \
  SET_F8((regset_powerpc64).f[8]); \
  SET_F9((regset_powerpc64).f[9]); \
  SET_F10((regset_powerpc64).f[10]); \
  SET_F11((regset_powerpc64).f[11]); \
  SET_F12((regset_powerpc64).f[12]); \
  SET_F13((regset_powerpc64).f[13]); \
  SET_F14((regset_powerpc64).f[14]); \
  SET_F15((regset_powerpc64).f[15]); \
  SET_F16((regset_powerpc64).f[16]); \
  SET_F17((regset_powerpc64).f[17]); \
  SET_F18((regset_powerpc64).f[18]); \
  SET_F19((regset_powerpc64).f[19]); \
  SET_F20((regset_powerpc64).f[20]); \
  SET_F21((regset_powerpc64).f[21]); \
  SET_F22((regset_powerpc64).f[22]); \
  SET_F23((regset_powerpc64).f[23]); \
  SET_F24((regset_powerpc64).f[24]); \
  SET_F25((regset_powerpc64).f[25]); \
  SET_F26((regset_powerpc64).f[26]); \
  SET_F27((regset_powerpc64).f[27]); \
  SET_F28((regset_powerpc64).f[28]); \
  SET_F29((regset_powerpc64).f[29]); \
  SET_F30((regset_powerpc64).f[30]); \
  SET_F31((regset_powerpc64).f[31]); \
}

/*
 * Set floating-point/SIMD registers from a register set. Don't mark the
 * registers as clobbered, so the compiler won't save/restore them.
 */
#define SET_FP_REGS_NOCLOBBER_POWERPC64( regset_powerpc64 ) \
{ \
  SET_F0_NOCLOBBER((regset_powerpc64).f[0]); \
  SET_F1_NOCLOBBER((regset_powerpc64).f[1]); \
  SET_F2_NOCLOBBER((regset_powerpc64).f[2]); \
  SET_F3_NOCLOBBER((regset_powerpc64).f[3]); \
  SET_F4_NOCLOBBER((regset_powerpc64).f[4]); \
  SET_F5_NOCLOBBER((regset_powerpc64).f[5]); \
  SET_F6_NOCLOBBER((regset_powerpc64).f[6]); \
  SET_F7_NOCLOBBER((regset_powerpc64).f[7]); \
  SET_F8_NOCLOBBER((regset_powerpc64).f[8]); \
  SET_F9_NOCLOBBER((regset_powerpc64).f[9]); \
  SET_F10_NOCLOBBER((regset_powerpc64).f[10]); \
  SET_F11_NOCLOBBER((regset_powerpc64).f[11]); \
  SET_F12_NOCLOBBER((regset_powerpc64).f[12]); \
  SET_F13_NOCLOBBER((regset_powerpc64).f[13]); \
  SET_F14_NOCLOBBER((regset_powerpc64).f[14]); \
  SET_F15_NOCLOBBER((regset_powerpc64).f[15]); \
  SET_F16_NOCLOBBER((regset_powerpc64).f[16]); \
  SET_F17_NOCLOBBER((regset_powerpc64).f[17]); \
  SET_F18_NOCLOBBER((regset_powerpc64).f[18]); \
  SET_F19_NOCLOBBER((regset_powerpc64).f[19]); \
  SET_F20_NOCLOBBER((regset_powerpc64).f[20]); \
  SET_F21_NOCLOBBER((regset_powerpc64).f[21]); \
  SET_F22_NOCLOBBER((regset_powerpc64).f[22]); \
  SET_F23_NOCLOBBER((regset_powerpc64).f[23]); \
  SET_F24_NOCLOBBER((regset_powerpc64).f[24]); \
  SET_F25_NOCLOBBER((regset_powerpc64).f[25]); \
  SET_F26_NOCLOBBER((regset_powerpc64).f[26]); \
  SET_F27_NOCLOBBER((regset_powerpc64).f[27]); \
  SET_F28_NOCLOBBER((regset_powerpc64).f[28]); \
  SET_F29_NOCLOBBER((regset_powerpc64).f[29]); \
  SET_F30_NOCLOBBER((regset_powerpc64).f[30]); \
  SET_F31_NOCLOBBER((regset_powerpc64).f[31]); \
}

#define read_memory( regset_powerpc64 ) \
{ \
  printf("r0:%ld\n",(regset_powerpc64).r[0]); \
  printf("r1:%ld\n",(regset_powerpc64).r[1]); \
  printf("r2:%ld\n",(regset_powerpc64).r[2]); \
  printf("r3:%ld\n",(regset_powerpc64).r[3]); \
  printf("r4:%ld\n",(regset_powerpc64).r[4]); \
  printf("r5:%ld\n",(regset_powerpc64).r[5]); \
  printf("r6:%ld\n",(regset_powerpc64).r[6]); \
  printf("r7:%ld\n",(regset_powerpc64).r[7]); \
  printf("r8:%ld\n",(regset_powerpc64).r[8]); \
  printf("r9:%ld\n",(regset_powerpc64).r[9]); \
  printf("r10:%ld\n",(regset_powerpc64).r[10]); \
  printf("r11:%ld\n",(regset_powerpc64).r[11]); \
  printf("r12:%ld\n",(regset_powerpc64).r[12]); \
  printf("r13:%ld\n",(regset_powerpc64).r[13]); \
  printf("r14:%ld\n",(regset_powerpc64).r[14]); \
  printf("r15:%ld\n",(regset_powerpc64).r[15]); \
  printf("r16:%ld\n",(regset_powerpc64).r[16]); \
  printf("r17:%ld\n",(regset_powerpc64).r[17]); \
  printf("r18:%ld\n",(regset_powerpc64).r[18]); \
  printf("r19:%ld\n",(regset_powerpc64).r[19]); \
  printf("r20:%ld\n",(regset_powerpc64).r[20]); \
  printf("r21:%ld\n",(regset_powerpc64).r[21]); \
  printf("r22:%ld\n",(regset_powerpc64).r[22]); \
  printf("r23:%ld\n",(regset_powerpc64).r[23]); \
  printf("r24:%ld\n",(regset_powerpc64).r[24]); \
  printf("r25:%ld\n",(regset_powerpc64).r[25]); \
  printf("r26:%ld\n",(regset_powerpc64).r[26]); \
  printf("r27:%ld\n",(regset_powerpc64).r[27]); \
  printf("r28:%ld\n",(regset_powerpc64).r[28]); \
  printf("r29:%ld\n",(regset_powerpc64).r[29]); \
  printf("r30:%ld\n",(regset_powerpc64).r[30]); \
  printf("r31:%lx\n",(regset_powerpc64).r[31]); \
  printf("sp:%p\n",(regset_powerpc64).r[1]); \
  printf("lr:%p\n",(regset_powerpc64).lr); \
  printf("pc:%p\n",(regset_powerpc64).pc); \
}

#define read_stack_regs_from_memory( regset_powerpc64 ) \
{\
  printf("sp:%p\n",(regset_powerpc64).r[1]); \
  printf("lr:%p\n",(regset_powerpc64).lr); \
  printf("pc:%p\n",(regset_powerpc64).pc); \
  printf("fbp:%lx\n",(regset_powerpc64).r[31]); \
}

#define read_stack_regs() \
{\
  void* sp; \
  void* lr; \
  uint64_t fbp; \
  asm volatile("std 1,%0" : "=m" (sp) : :); \
  asm volatile("std 31,%0" : "=m" (fbp) : : ); \
  asm volatile("mflr 17 ; std 17,%0" : "=m" (lr) : : "r17"); \
  printf("sp:%p\n", sp); \
  printf("fbp:%lx\n", fbp); \
  printf("lr:%p\n", lr); \
}

/* Set all registers from a register set. */
// Note: do not set PC, SP(r1) and FBP(r31) as they require special handling
// TODO condition registers
#define SET_REGS_POWERPC64( regset_powerpc64 ) \
{ \
  SET_R0((regset_powerpc64).r[0]); \
  SET_R2((regset_powerpc64).r[2]); \
  SET_R3((regset_powerpc64).r[3]); \
  SET_R4((regset_powerpc64).r[4]); \
  SET_R5((regset_powerpc64).r[5]); \
  SET_R6((regset_powerpc64).r[6]); \
  SET_R7((regset_powerpc64).r[7]); \
  SET_R8((regset_powerpc64).r[8]); \
  SET_R9((regset_powerpc64).r[9]); \
  SET_R10((regset_powerpc64).r[10]); \
  SET_R11((regset_powerpc64).r[11]); \
  SET_R12((regset_powerpc64).r[12]); \
  SET_R13((regset_powerpc64).r[13]); \
  SET_R14((regset_powerpc64).r[14]); \
  SET_R15((regset_powerpc64).r[15]); \
  SET_R16((regset_powerpc64).r[16]); \
  SET_R17((regset_powerpc64).r[17]); \
  SET_R18((regset_powerpc64).r[18]); \
  SET_R19((regset_powerpc64).r[19]); \
  SET_R20((regset_powerpc64).r[20]); \
  SET_R21((regset_powerpc64).r[21]); \
  SET_R22((regset_powerpc64).r[22]); \
  SET_R23((regset_powerpc64).r[23]); \
  SET_R24((regset_powerpc64).r[24]); \
  SET_R25((regset_powerpc64).r[25]); \
  SET_R26((regset_powerpc64).r[26]); \
  SET_R27((regset_powerpc64).r[27]); \
  SET_R28((regset_powerpc64).r[28]); \
  SET_R29((regset_powerpc64).r[29]); \
  SET_R30((regset_powerpc64).r[30]); \
  SET_LR((regset_powerpc64).lr); \
  SET_CTR((regset_powerpc64).ctr); \
  SET_FP_REGS_POWERPC64(regset_powerpc64); \
}

/* Get frame information. */
#define GET_FRAME_POWERPC64( bp, sp ) GET_R31(bp); GET_SP(sp);

/* Get current frame's size can be calculated using the back-chain.
 * It's assumed that a back-chain is present in the 
 * Get back chain to a register
 * subtract current stack pointer from back chain
*/
#define GET_FRAME_SIZE_POWERPC64( size ) \
  asm volatile("ld 3,0(1) ; subf %0,1,3;" : "=r" (size) :: "r3" )

/* Set frame after stack transformation.  Simulates function entry. */
#define SET_FRAME_POWERPC64( bp, sp) \
  asm volatile("ld 1,%0; ld 31,%1" : : "m" (sp), "m" (bp) )

#endif /* __powerpc64__ */

#endif /* _REGS_powerpc64_H */


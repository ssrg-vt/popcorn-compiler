#pragma once

#if 1
#ifdef __x86_64__


        #define GET_REG( var, reg, size ) asm volatile("mov"size" %%"reg", %0" : "=m" (var) )
        #define GET_REG64( var, reg ) GET_REG( var, reg, "q" )
        #define SET_REG( var, reg, size ) asm volatile("mov"size" %0, %%"reg : : "m" (var) : reg )
        #define SET_REG64( var, reg ) SET_REG( var, reg, "q" )

        #define GET_RBP( var ) GET_REG64( var, "rbp" )
        #define GET_RSP( var ) GET_REG64( var, "rsp" )

        /* Get frame information. */
        #define GET_FRAME_X86_64( bp, sp ) GET_RBP(bp); GET_RSP(sp);

        /* Get current frame's size, defined as rbp-rsp. */
        #define GET_FRAME_SIZE_X86_64( size ) \
          asm volatile("mov %%rbp, %0; sub %%rsp, %0" : "=g" (size) )

        #define SET_RBP( var ) SET_REG64( var, "rbp" )
        /* Set frame by setting rbp & rsp. */

        #define SET_FRAME_X86_64( bp, sp ) \
          asm volatile("mov %0, %%rsp; mov %1, %%rbp" : : "m" (sp), "m" (bp) )

        #define GET_FRAME(bp,sp) GET_FRAME_X86_64(bp,sp)
#ifndef SET_FRAME
        #define SET_FRAME(bp,sp) SET_FRAME_X86_64(bp,sp)
#endif


#else   
        /* Getters & setters for varying registers & sizes */
        #define GET_REG( var, reg, size ) asm volatile("str"size" "reg", %0" : "=m" (var) )
        #define GET_REG64( var, reg ) GET_REG( var, reg, "" )

        #define GET_X29( var ) GET_REG64( var, "x29" )
        /*
         * The stack pointer is a little weird because you can't read it directly into/
         * write it directly from memory.  Move it into another register which can be
         * saved in memory.
         */
        #define GET_SP( var ) asm volatile("mov x15, sp; str x15, %0" : "=m" (var) : : "x15")
        #define SET_SP( var ) asm volatile("ldr x15, %0; mov sp, x15" : : "m" (var) : "x15")

        /* Get frame information. */
        #define GET_FRAME_AARCH64( bp, sp ) GET_X29(bp); GET_SP(sp);

        /* Get current frame's size, defined as x29-sp. */
        #define GET_FRAME_SIZE_AARCH64( size ) \
          asm volatile("mov %0, sp; sub %0, x29, %0" : "=r" (size) )

        /* Set frame after stack transformation.  Simulates function entry. */
        #define SET_FRAME_AARCH64( bp, sp ) \
          asm volatile("mov sp, %0; mov x29, %1;" : : "r" (sp), "r" (bp) )

        #define GET_FRAME(bp,sp) GET_FRAME_AARCH64(bp,sp)
#ifndef SET_FRAME
        #define SET_FRAME(bp,sp) SET_FRAME_AARCH64(bp,sp)
#endif

#endif
#endif

int set_thread_stack(void *base, unsigned long len);
int stack_move();
int stack_use_original();

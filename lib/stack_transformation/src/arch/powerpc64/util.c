/* 
 * Implementation of helper functions
 * for PowerPC.
 *
 * Author: Buse Yilmaz <busey@vt.edu>
 * Date: 06/29/2017
*/

#include "arch/powerpc64/util.h"
#include "definitions.h"
#include <stdio.h>

// Read the opcode pair pointed by PC.
// Opcodes are stored in pairs in reverse order in PC's dereferenced value.
// This is due to instructions having a fixed 32 bit length.
uint64_t* get_opcode_pair(void* pc){
  uint64_t ret_addr = (uint64_t)pc;
//  ST_INFO("Return address: %lx [get_opcode_pair]\n", (long)ret_addr);
  
  uint64_t* opcode_pair;
  opcode_pair = ret_addr;

//  ST_INFO("Opcode pair: %lx\n", (long)(*opcode_pair));
  return opcode_pair;
}

// Get lower half of the opcode pair (1st opcode)
uint64_t get_opcode_lo(uint64_t* opcode_pair_ptr){
  uint64_t opcode_pair = (long)(*opcode_pair_ptr);
  
  uint64_t mask_lo = 1;
  mask_lo = mask_lo << 32;
  mask_lo -= 1;
  uint64_t opcode_lo = (opcode_pair & mask_lo);
//  ST_INFO("opcode_lo: %lx\n", opcode_lo);

  return opcode_lo;
}

// Get upper half of the opcode pair (2nd opcode)
uint64_t get_opcode_hi(uint64_t* opcode_pair_ptr){
  uint64_t opcode_pair = (long)(*opcode_pair_ptr);

  // Set mask to 2^33-1 to get lower half of the opcode pair (1st opcode)
  uint64_t mask_lo = 1;
  mask_lo = mask_lo << 32;
  mask_lo -= 1;
  
  uint64_t mask_hi = mask_lo << 32;
  
  uint64_t opcode_hi = (((opcode_pair & mask_hi) >> 32 )& mask_lo);
//  ST_INFO("opcode_hi: %lx\n", opcode_hi);

  return opcode_hi;
}

// Below algorithm checks the opcodes of the instruction PC is pointing to
// If it's a NOP we will skip it to get the correct return address.
// Hence we counts the NOPs seen.
int count_NOPs_seen(void* pc){
  uint64_t* opcode_pair = get_opcode_pair(pc);
  uint64_t opcode_lo = get_opcode_lo(opcode_pair);
  uint64_t opcode_hi = get_opcode_hi(opcode_pair);
  
  int NOPs_seen = 0;
  // Opcode have their bytes stored in reverse order. Opcode of NOP:00 00 00 60
  long NOP = 0x60000000;
  while (opcode_lo == NOP){
    NOPs_seen++;
    if (opcode_hi == NOP){
      NOPs_seen++;
      opcode_lo = get_opcode_lo(opcode_pair);
      opcode_hi = get_opcode_hi(opcode_pair);
    }
    else {
     break;
    }
  }

//  ST_INFO("NOPs seen: %d [count_NOPs_seen]\n\n", NOPs_seen);
  return NOPs_seen;
}

void* fix_pc(void* pc){
  pc += count_NOPs_seen(pc) * 4;

  return pc;
}

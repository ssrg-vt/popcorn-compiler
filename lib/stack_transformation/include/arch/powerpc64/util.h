/*
 * Helper functions to skip NOPs after
 * function calls for PowerPC.
 *
 * Author: Buse Yilmaz <busey@vt.edu>
 * Date: 06/29/2017
*/

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>

// Read the opcode pair pointed by PC.
// Opcodes are stored in pairs in reverse order in PC's dereferenced value.
// This is due to instructions having a fixed 32 bit length.
uint64_t* get_opcode_pair(void* pc);

// Get lower half of the opcode pair (1st opcode)
uint64_t get_opcode_lo(uint64_t* opcode_pair_ptr);

// Get upper half of the opcode pair (2nd opcode)
uint64_t get_opcode_hi(uint64_t* opcode_pair_ptr);

// Below algorithm checks the opcodes of the instruction PC is pointing to
// If it's a NOP we will skip it to get the correct return address.
// Hence we counts the NOPs seen.
int count_NOPs_seen(void* pc);

// Update PC by skipping NOPs
void* fix_pc(void* pc);
#endif /* _UTIL_H */

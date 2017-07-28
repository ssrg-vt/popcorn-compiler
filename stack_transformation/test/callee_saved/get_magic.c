#include <stdint.h>

#define MAGIC_A (0x12345678UL << 32)
#define MAGIC_B 0xdeadbeef
#define MAGIC (MAGIC_A | MAGIC_B)

uint64_t get_magic_a()
{
  return MAGIC_A;
}

uint64_t get_magic_b()
{
  return MAGIC_B;
}

uint64_t get_magic()
{
  return MAGIC;
}

#include "hard_decision.h"
#include <stddef.h>
#include <snrt.h>

bool hard_decision(int8_t *hard_bits, const int8_t *soft_bits, const uint32_t offset, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  uint8_t result;
  const int8_t *address_soft = soft_bits + offset;
  int8_t *address_hard = hard_bits + offset;
  asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
  asm volatile("vmv.v.i v16, -1");

  do {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v0, (%0)" :: "r"(address_soft) : "memory"); 
    // hard bit: (x <= 0) ? 1 : 0
    asm volatile("vadd.vi v8, v0, -1");
        // "!= 0" -> v16
    asm volatile("vsrl.vi v8, v8, 7");
    asm volatile("vse8.v v8, (%0)" :: "r"(address_hard): "memory");
    asm volatile("vredminu.vs v16, v0, v16");
 
    address_soft += vl;
    address_hard += vl;
    remaining    -= vl;
  } while (remaining > 0);

  asm volatile("vmv.x.s %0, v16" : "=r"(result));
  return (result != 0);
}
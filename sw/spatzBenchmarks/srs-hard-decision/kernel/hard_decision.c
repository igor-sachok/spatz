#include "hard_decision.h"
#include <stddef.h>
#include <snrt.h>
/*
bool hard_decision(int8_t *hard_bits, const int8_t *soft_bits, const uint32_t offset, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  const int8_t *address_soft = soft_bits + offset;
  int8_t *address_hard = hard_bits + offset;
  int8_t red;
  //Initialize accumulator
  asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
  asm volatile("vmv.v.i v24, -1"); 
  do {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    // Load the vector
    asm volatile("vle8.v v0, (%0)" :: "r"(address_soft));
    // Get sign by bit shift x>0 -> 0, x<=0 -> 1
    // Add -1 to x = 0 returns 1
    asm volatile("vadd.vi v8, v0, -1");
    asm volatile("vsrl.vi v8, v8, 7");

    asm volatile("vse8.v v8, (%0)" :: "r"(address_hard));
    //Compare with 0
    asm volatile("vmsne.vi v16, v0, 0");
    asm volatile("vand.vv v24, v24, v16");
    //asm volatile("vredand.vs v24, v16, v24");
    address_soft += vl;
    address_hard += vl;
    remaining    -= vl;
  } while (remaining > 0);
    //asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(lifting_size));
    asm volatile("vmv.v.i v0, -1");
    //asm volatile("vredand.vs v8, v24, v0");   // vd=v8 (не v0!), vs2=v24, vs1=v0
    asm volatile("vmv.x.s %0, v8" : "=r"(red));
  return red;
}
*/
/*
bool hard_decision(int8_t *hard_bits, const int8_t *soft_bits, const uint32_t offset, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  const int8_t *address_soft = soft_bits + offset;
  int8_t *address_hard = hard_bits + offset;

  uint32_t accum = 0xFFFFFFFF;   // нейтральный элемент AND

  do {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v0, (%0)" :: "r"(address_soft));

    // hard bit: (x <= 0) ? 1 : 0
    asm volatile("vadd.vi v8, v0, -1");
    asm volatile("vsrl.vi v8, v8, 7");
    asm volatile("vse8.v v8, (%0)" :: "r"(address_hard));

    // "!= 0" через арифметику (без vmsne/масок): v16[i] = (v0[i]!=0) ? 0xFF : 0x00
    asm volatile("vrsub.vi v16, v0, 0");
    asm volatile("vor.vv   v16, v0, v16");
    asm volatile("vsra.vi  v16, v16, 7");

    // reduction ЭТОГО чанка — каждую итерацию, vd != vs2 (не self-ref)
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v24, v16, v0");

    uint32_t chunk_result;
    asm volatile("vmv.x.s %0, v24" : "=r"(chunk_result));
    accum &= chunk_result;   // накопление через GPR, НЕ через векторный регистр

    address_soft += vl;
    address_hard += vl;
    remaining    -= vl;
  } while (remaining > 0);

  return accum & 1;   // true, если нулей не было
}
*/

bool hard_decision(int8_t *hard_bits, const int8_t *soft_bits, const uint32_t offset, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  uint8_t result;
  const int8_t *address_soft = soft_bits + offset;
  int8_t *address_hard = hard_bits + offset;
  asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
  asm volatile("vmv.v.i v24, -1");

  do {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v0, (%0)" :: "r"(address_soft) : "memory"); 
    // "!= 0" -> v16
    asm volatile("vredminu.vs v16, v0, v24");

    // hard bit: (x <= 0) ? 1 : 0
    asm volatile("vadd.vi v8, v0, -1");
    asm volatile("vsrl.vi v8, v8, 7");
    asm volatile("vse8.v v8, (%0)" :: "r"(address_hard));

 
    address_soft += vl;
    address_hard += vl;
    remaining    -= vl;
  } while (remaining > 0);

  asm volatile("vmv.x.s %0, v16" : "=r"(result));
  return (result != 0);
}
#include "compute_check_to_var_msgs.h"
#include <stddef.h>
#include <snrt.h>

void compute_check_to_var_msgs(int8_t        *this_check_to_var,
                               int8_t  const *this_var_to_check,
                               int8_t  const *min_var_to_check_dup,
                               int8_t  const *second_min_var_to_check_dup,
                               uint8_t const *min_var_to_check_index_dup,
                               uint8_t const *sign_prod_var_to_check_dup,
                               uint32_t       shift,
                               uint32_t       var_node,
                               uint32_t const lifting_size){
  uint32_t remaining = lifting_size;
  uint32_t vl;
  const uint32_t off    = lifting_size - shift;
  int8_t  const *min_ro = min_var_to_check_dup        + off;
  int8_t  const *sec_ro = second_min_var_to_check_dup + off;
  uint8_t const *idx_ro = min_var_to_check_index_dup  + off;
  uint8_t const *sgn_ro = sign_prod_var_to_check_dup  + off;

  do {
    asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v12, (%0)" :: "r"(idx_ro)           : "memory");
    asm volatile("vmseq.vx v0, v12, %0" :: "r"(var_node));
    asm volatile("vle8.v v4,  (%0)" :: "r"(min_ro)           : "memory");
    asm volatile("vle8.v v8,  (%0)" :: "r"(sec_ro)           : "memory");
    asm volatile("vmerge.vvm v24, v4, v8, v0");
    asm volatile("vle8.v v16, (%0)" :: "r"(sgn_ro)           : "memory");
    asm volatile("vmsne.vx v1, v16, x0");
    asm volatile("vle8.v v20, (%0)" :: "r"(this_var_to_check): "memory");
    asm volatile("vmslt.vx v2, v20, x0");
    asm volatile("vmxor.mm v0, v1, v2");

    // result = final_sign ? -magnitude : +magnitude
    asm volatile("vneg.v v26, v24");
    asm volatile("vmerge.vvm v28, v24, v26, v0");
    asm volatile("vse8.v v28, (%0)" :: "r"(this_check_to_var): "memory");

 
    min_ro += vl; 
    sec_ro += vl; 
    idx_ro += vl; 
    sgn_ro += vl;
    this_var_to_check += vl; 
    this_check_to_var += vl;
    remaining         -= vl;
  } while (remaining > 0);
}
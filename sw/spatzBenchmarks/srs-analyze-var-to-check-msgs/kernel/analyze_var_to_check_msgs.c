#include "analyze_var_to_check_msgs.h"
#include <stddef.h>
#include <snrt.h>

void analyze_var_to_check_msgs(int8_t *min_var_to_check, int8_t *second_min_var_to_check, uint8_t *min_var_to_check_index, uint8_t *sign_prod_var_to_check, const int8_t *rotated_node, const uint32_t var_node, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;

  do {
    asm volatile("vsetvli %0, %1, e8, m4, ta, mu" : "=r"(vl) : "r"(remaining));

    asm volatile("vle8.v v4, (%0)" :: "r"(min_var_to_check) : "memory"); 
    asm volatile("vle8.v v8, (%0)" :: "r"(second_min_var_to_check) : "memory");
    asm volatile("vle8.v v12, (%0)" :: "r"(min_var_to_check_index) : "memory"); 
    asm volatile("vle8.v v16, (%0)" :: "r"(sign_prod_var_to_check) : "memory");
    asm volatile("vle8.v v20, (%0)" :: "r"(rotated_node) : "memory");

    // Compute abs(rotated_node) = var_to_check_abs
    asm volatile("vrsub.vi v24, v20, 0");
    asm volatile("vmax.vv v24, v20, v24");

    // Compare var_to_check_abs with min_var_to_checks = is_min
    asm volatile("vmslt.vv v0, v24, v4");

    // Compute new_second_min
    asm volatile("vmerge.vvm v28, v24, v4, v0");
    // Compute min_var_to_check_index
    asm volatile("vmerge.vxm v12, v12, %0, v0" :: "r"(var_node));
       // Compute min_var_to_checks
    asm volatile("vmerge.vvm v4, v4, v24, v0");

    // Compute is_best_two
    asm volatile("vmslt.vv v0, v24, v8");

    // Compute second_min_var_to_check
    asm volatile("vmerge.vvm v8, v8, v28, v0");

    // Compute sign_prod_var_to_check
    asm volatile("vmsle.vi v0, v20, -1");
    asm volatile("vxor.vi v16, v16, 1, v0.t");


    asm volatile("vse8.v v4,  (%0)" :: "r"(min_var_to_check)        : "memory");
    asm volatile("vse8.v v8,  (%0)" :: "r"(second_min_var_to_check) : "memory");
    asm volatile("vse8.v v12, (%0)" :: "r"(min_var_to_check_index)  : "memory");
    asm volatile("vse8.v v16, (%0)" :: "r"(sign_prod_var_to_check)  : "memory");

 
    min_var_to_check        += vl;
    second_min_var_to_check += vl;
    min_var_to_check_index  += vl;
    sign_prod_var_to_check  += vl;
    rotated_node            += vl;
    remaining               -= vl;
  } while (remaining > 0);
}
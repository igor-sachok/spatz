#include "analyze_var_to_check_msgs.h"
#include <stddef.h>
#include <snrt.h>

void analyze_var_to_check_msgs(int8_t *min_var_to_check, int8_t *second_min_var_to_check, uint8_t *min_var_to_check_index, uint8_t *sign_prod_var_to_check, const int8_t *rotated_node, const uint32_t var_node, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;

  do {
    asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v2, (%0)" :: "r"(min_var_to_check) : "memory"); 
    asm volatile("vle8.v v4, (%0)" :: "r"(second_min_var_to_check) : "memory");
    asm volatile("vle8.v v6, (%0)" :: "r"(min_var_to_check_index) : "memory"); 
    asm volatile("vle8.v v8, (%0)" :: "r"(sign_prod_var_to_check) : "memory");
    asm volatile("vle8.v v10, (%0)" :: "r"(rotated_node) : "memory");

    // Compute abs(rotated_node)
    asm volatile("vrsub.vi v12, v10, 0");
    asm volatile("vmax.vv v14, v10, v12");

    // Compare var_to_check_abs with min_var_to_checks
    asm volatile("vmsgt.vv v0, v6, v14"); 

    // Compute new_second_min
    asm volatile("vadd.vv v16, v14, v2, v0.t");
    // Compute new min_var_to_checks
    asm volatile("vadd.vv v18, v2, v14, v0.t");
    // Compute new min_var_to_check_index
    asm volatile("vadd.vx v20, v6, %0, v0.t" :: "r"(var_node));

    // Compare var_to_check_abs and second_min_var_to_check
    asm volatile("vmsgt.vv v0, v4, v14"); 

    // Compute second_min_var_to_check
    asm volatile("vadd.vv v22, v4, v16, v0.t");

    // Compute sign_prod_var_to_check
    asm volatile("vmsgt.vi v0, v10, 0");




    asm volatile("vse8.v v4, (%0)" :: "r"(this_soft_bits));

 
    min_var_to_check += vl;
    second_min_var_to_check += vl;
    min_var_to_check_index += vl;
    sign_prod_var_to_check += vl;
    rotated_node += vl;
    remaining    -= vl;
  } while (remaining > 0);
}


void compute_soft_bits(int8_t *this_soft_bits,
                       const int8_t *this_var_to_check,
                       const int8_t *this_check_to_var,
                       const uint32_t lifting_size)
{
  uint32_t remaining = lifting_size;
  uint32_t vl;

  int16_t llr_max     =  120;
  int16_t llr_min     = -120;
  int16_t llr_inf     =  127;
  int16_t llr_inf_neg = -127;

  asm volatile("vsetvli %0, %1, e16, m2, ta, mu" : "=r"(vl) : "r"(remaining));

  asm volatile("vmv.v.i v12, 0");                              /* 0.0  */

  asm volatile("vmv.v.x v14, %0" :: "r"(llr_max));
  asm volatile("vfcvt.f.x.v v14, v14");                        /*  120.0 */

  asm volatile("vmv.v.x v16, %0" :: "r"(llr_min));
  asm volatile("vfcvt.f.x.v v16, v16");                        /* -120.0 */

  asm volatile("vmv.v.x v18, %0" :: "r"(llr_inf));
  asm volatile("vfcvt.f.x.v v18, v18");                        /*  127.0 */

  asm volatile("vmv.v.x v20, %0" :: "r"(llr_inf_neg));
  asm volatile("vfcvt.f.x.v v20, v20");                        /* -127.0 */

  do {
    asm volatile("vsetvli %0, %1, e8, m1, ta, mu" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v2, (%0)" :: "r"(this_var_to_check) : "memory");
    asm volatile("vle8.v v3, (%0)" :: "r"(this_check_to_var) : "memory");

    asm volatile("vwadd.vv v4, v2, v3");                       /* sum */
    asm volatile("vwadd.vx v6, v2, x0");                       /* a   */
    asm volatile("vwadd.vx v8, v3, x0");                       /* b   */

    asm volatile("vsetvli %0, %1, e16, m2, ta, mu" : "=r"(vl) : "r"(remaining));

    asm volatile("vfcvt.f.x.v v4, v4");
    asm volatile("vfcvt.f.x.v v6, v6");
    asm volatile("vfcvt.f.x.v v8, v8");

    asm volatile("vfadd.vv v10, v4, v12");                           /* saved sum */

    /* sum > 120  ->  +inf   (vmfgt has no .vv form: 120 < sum) */
    asm volatile("vmflt.vv v0, v14, v4");
    asm volatile("vfadd.vv v4, v12, v18, v0.t");

    /* sum < -120 ->  -inf */
    asm volatile("vmflt.vv v0, v4, v16");
    asm volatile("vfadd.vv v4, v12, v20, v0.t");

    /* b == +inf -> b */
    asm volatile("vmfeq.vv v0, v8, v18");
    asm volatile("vfadd.vv v4, v8, v12, v0.t");

    /* b == -inf -> b */
    asm volatile("vmfeq.vv v0, v8, v20");
    asm volatile("vfadd.vv v4, v8, v12, v0.t");

    /* a == +inf -> a */
    asm volatile("vmfeq.vv v0, v6, v18");
    asm volatile("vfadd.vv v4, v6, v12, v0.t");

    /* a == -inf -> a */
    asm volatile("vmfeq.vv v0, v6, v20");
    asm volatile("vfadd.vv v4, v6, v12, v0.t");

    /* a == -b  <=>  raw sum == 0  ->  0 */
    asm volatile("vmfeq.vv v0, v10, v12");
    asm volatile("vfadd.vv v4, v12, v12, v0.t"); 

    asm volatile("vsetvli %0, %1, e8, m1, ta, mu" : "=r"(vl) : "r"(remaining));
    asm volatile("vfncvt.rtz.x.f.w v2, v4");
    asm volatile("vse8.v v2, (%0)" :: "r"(this_soft_bits) : "memory");

    this_var_to_check += vl;
    this_check_to_var += vl;
    this_soft_bits    += vl;
    remaining         -= vl;
  } while (remaining > 0);
}
#include "compute_var_to_check_msgs.h"
#include <stddef.h>
#include <snrt.h>

void compute_var_to_check_msgs(int8_t *this_var_to_check, const int8_t *this_soft_bits, const int8_t *this_check_to_var, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  int16_t llr_max = 120;
  int16_t llr_min = -120;
  int16_t llr_inf = 127;
  int16_t llr_inf_neg = -127;

  do {
    asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v4, (%0)" :: "r"(this_soft_bits) : "memory"); 
    asm volatile("vle8.v v8, (%0)" :: "r"(this_check_to_var) : "memory");

    asm volatile("vwsub.vv v12, v4, v8");
    asm volatile("vwadd.vx v20, v4, x0");
    asm volatile("vwadd.vx v24, v8, x0");

    asm volatile("vsetvli %0, %1, e16, m4, ta, mu" : "=r"(vl) : "r"(remaining));
    //save the initial summ
    asm volatile("vmv.v.v v16, v12");

    //Compare with max value
    asm volatile("vmsgt.vx v0, v12, %0" :: "r"(llr_max));
    //asm volatile("vmerge.vxm v12, v12, %0, v0" :: "r"(llr_inf));
    asm volatile("vadd.vx v12, v28, %0, v0.t" :: "r"(llr_max));
    //Compare with min value
    asm volatile("vmslt.vx v0, v12, %0":: "r"(llr_min));
    asm volatile("vmerge.vxm v12, v12, %0, v0" :: "r"(llr_min));

    //If val_b is inf -> return -val_b
    asm volatile("vrsub.vx v4, v24, x0");
    asm volatile("vmseq.vx v0, v24, %0" :: "r"(llr_inf));
    asm volatile("vmerge.vvm v12, v12, v4, v0");

    //If val_b is -inf -> return -val_b
    asm volatile("vmseq.vx v0, v24, %0" :: "r"(llr_inf_neg));
    asm volatile("vmerge.vvm v12, v12, v4, v0");

    //If val_a is inf -> return val_a
    asm volatile("vmseq.vx v0, v20, %0" :: "r"(llr_inf));
    asm volatile("vmerge.vvm v12, v12, v20, v0");

    //If val_a is -inf -> return val_a
    asm volatile("vmseq.vx v0, v20, %0" :: "r"(llr_inf_neg));
    asm volatile("vmerge.vvm v12, v12, v20, v0");

    // If check if initial summ gave 0 -> return 0
    asm volatile("vmseq.vi v0, v16, 0");
    asm volatile("vmerge.vim v12, v12, 0, v0");

    // This should be replaced with 1 narrowing integer instruction. 
    asm volatile("vfcvt.f.x.v v12, v12");
    asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(remaining));
    asm volatile("vfncvt.x.f.w v4, v12");
    asm volatile("vse8.v v4, (%0)" :: "r"(this_var_to_check));

 
    this_var_to_check += vl;
    this_check_to_var += vl;
    this_soft_bits += vl;
    remaining    -= vl;
  } while (remaining > 0);
}


void compute_var_to_check_msgs_float(int8_t *this_var_to_check, const int8_t *this_soft_bits, const int8_t *this_check_to_var, const uint32_t lifting_size) {

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
    asm volatile("vle8.v v2, (%0)" :: "r"(this_soft_bits) : "memory");
    asm volatile("vle8.v v3, (%0)" :: "r"(this_check_to_var) : "memory");

    asm volatile("vwsub.vv v4, v2, v3");                       /* sum */
    asm volatile("vwadd.vx v6, v2, x0");                       /* a   */
    asm volatile("vwadd.vx v8, v3, x0");                       /* b   */

    asm volatile("vsetvli %0, %1, e16, m2, ta, mu" : "=r"(vl) : "r"(remaining));

    asm volatile("vfcvt.f.x.v v4, v4");
    asm volatile("vfcvt.f.x.v v6, v6");
    asm volatile("vfcvt.f.x.v v8, v8");

    asm volatile("vfsub.vv v10, v4, v12");                           /* saved sum */
    asm volatile("vfsub.vv v22, v12, v8");                          // saved -b
    /* sum > 120  ->  +inf   (vmfgt has no .vv form: 120 < sum) */
    asm volatile("vmflt.vv v0, v14, v4");
    asm volatile("vfadd.vv v4, v12, v14, v0.t");

    /* sum < -120 ->  -inf */
    asm volatile("vmflt.vv v0, v4, v16");
    asm volatile("vfadd.vv v4, v12, v16, v0.t");

    /* b == +inf -> -b */
    asm volatile("vmfeq.vv v0, v8, v18");
    asm volatile("vfadd.vv v4, v22, v12, v0.t");

    /* b == -inf -> -b */
    asm volatile("vmfeq.vv v0, v8, v20");
    asm volatile("vfadd.vv v4, v22, v12, v0.t");

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
    asm volatile("vse8.v v2, (%0)" :: "r"(this_var_to_check) : "memory");

    this_var_to_check += vl;
    this_check_to_var += vl;
    this_soft_bits    += vl;
    remaining         -= vl;
  } while (remaining > 0);
}
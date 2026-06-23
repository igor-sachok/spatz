#include "prod.h"
#include <stddef.h>
#include <snrt.h>

void fprod_cf_v32b(const float *a, const float *b, float *out,
              const unsigned int p_start, const unsigned int p_end) {
  size_t stride = 8;
  unsigned int p = p_start;
  while (p < p_end) {
  // Stripmine and accumulate a partial reduced vector
    size_t gvl;
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
            : "=r"(gvl)
            : "r"((p_end - p)/2)
    );
    // Bump pointers
    const float *a_ = a + p;
    const float *b_ = b + p;
    const float *a_im = a + p + 1;
    const float *b_im = b + p + 1;
    float *out_re  = out + p;
    float *out_im  = out + p + 1;
    // Load chunk a and b
    // Load Re(a) and Im(a)
    asm volatile(
        "vlse32.v v0, (%0), %2\n\t"  
        "vlse32.v v4, (%1), %2"      
        : 
        : "r"(a_), "r"(a_im), "r"(stride)
    );
    // Load Re(b) and Im(b)
    asm volatile(
        "vlse32.v v8, (%0), %2\n\t"  
        "vlse32.v v12, (%1), %2"     
        : 
        : "r"(b_), "r"(b_im), "r"(stride)
    );
    //Re
    asm volatile("vfmul.vv v16, v0, v8");
    asm volatile("vfmul.vv v20, v4, v12");
    asm volatile("vfsub.vv v24, v16, v20");
    asm volatile("vsse32.v v24, (%0), %1":: "r"(out_re), "r"(stride): "memory");
    //Im
    asm volatile("vfmul.vv v16, v0, v12");
    asm volatile("vfmul.vv v20, v4, v8");
    asm volatile("vfadd.vv v28, v16, v20");
    asm volatile("vsse32.v v28, (%0), %1":: "r"(out_im), "r"(stride): "memory");

    p += gvl * 2;
  }
}
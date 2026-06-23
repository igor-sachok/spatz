#include "prod.h"
#include <stddef.h>
#include <snrt.h>

void fprod_cf_v32b(const float *a, const float *b, float *out,  float *buffer_a_re, float *buffer_a_im, float *buffer_b_re, float *buffer_b_im,
              const unsigned int p_start, const unsigned int p_end) {
  size_t stride = 8;
  unsigned int p = p_start;
  for (int i = 0; i < p_end - p_start; i++){
    buffer_a_re[i] = a[2*i];
    buffer_a_im[i] = a[2*i + 1];
    buffer_b_re[i] = b[2*i];
    buffer_b_im[i] = b[2*i + 1];
  }
  while (p < p_end) {
  // Stripmine and accumulate a partial reduced vector
    size_t gvl;
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
            : "=r"(gvl)
            : "r"((p_end - p))
    );
    // Bump pointers
    const float *a_im = a + 1;
    const float *b_im = b + 1;
    float *out_im  = out + 1;
    // Load chunk a and b
    // Load Re(a) and Im(a)
    asm volatile(
        "vlse32.v v0, (%0), %2\n\t"  
        "vlse32.v v4, (%1), %2"      
        : 
        : "r"(a), "r"(a_im), "r"(stride)
    );
    // Load Re(b) and Im(b)
    asm volatile(
        "vlse32.v v8, (%0), %2\n\t"  
        "vlse32.v v12, (%1), %2"     
        : 
        : "r"(b), "r"(b_im), "r"(stride)
    );
    asm volatile("vle32.v v0, (%0)":: "r"(buffer_a_re)); 
    asm volatile("vle32.v v4, (%0)":: "r"(buffer_a_im));
    asm volatile("vle32.v v8, (%0)":: "r"(buffer_b_re)); 
    asm volatile("vle32.v v12, (%0)":: "r"(buffer_b_im));
    //Re
    asm volatile("vfmul.vv v16, v0, v8");
    asm volatile("vfmul.vv v20, v4, v12");
    asm volatile("vfsub.vv v24, v16, v20");
    asm volatile("vsse32.v v24, (%0), %1":: "r"(out), "r"(stride): "memory");
    //Im
    asm volatile("vfmul.vv v16, v0, v12");
    asm volatile("vfmul.vv v20, v4, v8");
    asm volatile("vfadd.vv v28, v16, v20");
    asm volatile("vsse32.v v28, (%0), %1":: "r"(out_im), "r"(stride): "memory");
    a += gvl;
    b += gvl;
    out += 2*gvl;
    p += gvl;
  }
}
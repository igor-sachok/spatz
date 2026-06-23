void fdotp_cf_v32b(const float *a, const float *b, float *out_re, float *out_im, unsigned int avl) {
  const unsigned int orig_avl = avl;
  unsigned int vl;
  unsigned int stride = 8;

  float red;

  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  // Stripmine and accumulate a partial reduced vector
  do {
    const float *a_im = a + 1;
    const float *b_im = b + 1;
    // Set the vl
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vlse32.v v20, (%0), %1":: "r"(a), "r"(stride)); 
    asm volatile("vlse32.v v8, (%0), %1":: "r"(a_im), "r"(stride));
    asm volatile("vlse32.v v12, (%0), %1" :: "r"(b), "r"(stride));
    asm volatile("vlse32.v v16, (%0), %1" :: "r"(b_im), "r"(stride));
    for (int i = 0; i < 20; i++){
      asm volatile("nop");
    }
    // Multiply and accumulate
    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v20, v12");
      asm volatile("vfmacc.vv v24, v8, v16");
      asm volatile("vfmul.vv v28, v8, v12");
      asm volatile("vfnmsac.vv v28, v20, v16");
    } else {
      asm volatile("vfmacc.vv v24, v20, v12");
      asm volatile("vfmacc.vv v24, v8, v16");
      asm volatile("vfmacc.vv v28, v8, v12");
      asm volatile("vfnmsac.vv v28, v20, v16");
    }

    // Bump pointers
    a += 2*vl;
    b += 2*vl;
    avl -= vl;
  } while (avl > 0);

  // Reduce and return
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(orig_avl));
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  *out_re = red;
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v28, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  *out_im = red;
}
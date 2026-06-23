void fdotp_cf_v32b(const float *a, const float *b, float *buffer_a_re, float *buffer_a_im, float *buffer_b_re, float *buffer_b_im, float *out_re, float *out_im, unsigned int avl) {
  const unsigned int orig_avl = avl;
  unsigned int vl;
  unsigned int stride = 8;

  float red;
  for (int i = 0; i < avl; i++){
    buffer_a_re[i] = a[2*i];
    buffer_a_im[i] = a[2*i + 1];
    buffer_b_re[i] = b[2*i];
    buffer_b_im[i] = b[2*i + 1];
    //printf("%f, %f, %f, %f\n",buffer_a_re[i], buffer_a_im[i], buffer_b_re[i], buffer_b_im[i]);
  }

  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  // Stripmine and accumulate a partial reduced vector
  do {
    // Set the vl
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle32.v v20, (%0)":: "r"(buffer_a_re)); 
    asm volatile("vle32.v v8, (%0)":: "r"(buffer_a_im));
    asm volatile("vle32.v v12, (%0)":: "r"(buffer_b_re)); 
    asm volatile("vle32.v v16, (%0)":: "r"(buffer_b_im)); 
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
    buffer_a_re += vl;
    buffer_a_im += vl;
    buffer_b_re += vl;
    buffer_b_im += vl;
    avl -= vl;
  } while (avl > 0);

  // Reduce and return
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  *out_re = red;
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v28, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  *out_im = red;
}
#include <snrt.h>
//#include DATAHEADER   
float *a;
float *b;
float *out_re;
float *out_im;

void fdotp_cf_v32b(float *dotp_A_dram, float *dotp_B_dram, float *result_re, float *result_im) {
  uint32_t num_cores = snrt_cluster_core_num();
  uint32_t cid = snrt_cluster_core_idx();
  unsigned int dim = (dotp_l.M / num_cores)/2;
  const unsigned int orig_avl = dim;
  unsigned int vl;
  unsigned int stride = 8;
  float acc_re = 0.0f;
  float acc_im = 0.0f;

  float red;
    // Allocate the vectors
  if (cid == 0) {
    a = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    b = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    out_re = (float *)snrt_l1alloc(num_cores * sizeof(float));
    out_im = (float *)snrt_l1alloc(num_cores * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, dotp_A_dram, dotp_l.M * sizeof(float));
    snrt_dma_start_1d(b, dotp_B_dram, dotp_l.M * sizeof(float));
    snrt_dma_wait_all();
  }

    // Wait for all cores to finish
  snrt_cluster_hw_barrier();
    // Calculate internal pointers
  float *a_int = a + 2 * dim * cid;
  float *b_int = b + 2 * dim * cid;
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(dim));
  asm volatile("vmv.s.x v0, zero");

  // Stripmine and accumulate a partial reduced vector
  do {
    const float *a_im = a_int + 1;
    const float *b_im = b_int + 1;
    // Set the vl
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(dim));

    // Load chunk a and b
    asm volatile("vlse32.v v20, (%0), %1":: "r"(a_int), "r"(stride)); 
    asm volatile("vlse32.v v8, (%0), %1":: "r"(a_im), "r"(stride));
    asm volatile("vlse32.v v12, (%0), %1" :: "r"(b_int), "r"(stride));
    asm volatile("vlse32.v v16, (%0), %1" :: "r"(b_im), "r"(stride));
    for (int i = 0; i < 30; i++){
      asm volatile("nop");
    }
    // Multiply and accumulate
    if (dim == orig_avl) {
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
    a_int += 2*vl;
    b_int += 2*vl;
    dim -= vl;
  } while (dim > 0);

  // Reduce and return
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(orig_avl));
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  out_re[cid] = red;
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v28, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  out_im[cid] = red;
  snrt_cluster_hw_barrier();
    // Final reduction
  if (cid == 0) {
    acc_re += out_re[0];
    acc_im += out_im[0];
    for (uint32_t i = 1; i < num_cores; ++i) {
          acc_re += out_re[i];
          acc_im += out_im[i];
    }
    *result_re = acc_re;
    *result_im = acc_im;
  }
}
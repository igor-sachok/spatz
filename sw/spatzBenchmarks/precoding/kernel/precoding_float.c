#include <snrt.h>
//#include DATAHEADER   
int nof_layers = 4;
float *input_re;
float *port_weights;

void precoding_v32b(float *input_re_in_dram, float *port_weights_in_dram, float *port_re) {
  uint32_t cid = snrt_cluster_core_idx();
  uint32_t num_cores = snrt_cluster_core_num();
  unsigned int dim = (dotp_l.M / num_cores)/2;
  const unsigned int orig_avl = dim;
  unsigned int vl;
  unsigned int stride = 8;

  float red;
    // Allocate the vectors
  if (cid == 0) {
    input_re = (float *)snrt_l1alloc(nof_layers * dotp_l.M * sizeof(float));
    port_weights = (float *)snrt_l1alloc(nof_layers * 2 * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(input_re, input_re_in_dram, nof_layers * dotp_l.M * sizeof(float));
    snrt_dma_start_1d(port_weights, port_weights_in_dram, nof_layers * 2 * sizeof(float));
    snrt_dma_wait_all();
  }

    // Wait for all cores to finish
  snrt_cluster_hw_barrier();
    // Calculate internal pointers
  float *input_re_int = input_re + 2 * dim * cid;
  float *port_re_int = port_re + 2 * dim * cid;
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  switch (nof_layers){
    case 1: {
    unsigned int remaining = dim;
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }
      // Multiply
      asm volatile("vfmul.vf v8, v0, %0":: "f"(port_weights[0]));
      asm volatile("vfmul.vf v12, v4, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[0]));
      asm volatile("vfsub.vv v24, v8, v12");
      asm volatile("vfadd.vv v28, v20, v16");
      asm volatile("vsse32.v v24, (%0), %1":: "r"(port_re_int), "r"(stride): "memory");
      asm volatile("vsse32.v v28, (%0), %1":: "r"(port_re_int+1), "r"(stride): "memory");

      // Bump pointers
      input_re_int += 2*vl;
      port_re_int     += 2*vl;
      remaining -= vl;
    } while (remaining > 0);
    break;
    }

    case 2: {
          unsigned int remaining = dim;
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + dotp_l.M ), "r"(stride)); 
      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + dotp_l.M + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }
      // Multiply
      asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[0]));
      asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v24, v0, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v28, v4, %0":: "f"(port_weights[0]));
      asm volatile("vfsub.vv v0, v16, v20");
      asm volatile("vfadd.vv v4, v24, v28");

      asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[2]));
      asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[3]));
      asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[3]));
      asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[2]));
      asm volatile("vfsub.vv v8, v16, v20");
      asm volatile("vfadd.vv v12, v24, v28");
      asm volatile("vfadd.vv v0, v0, v8");
      asm volatile("vfadd.vv v4, v4, v12");

      asm volatile("vsse32.v v0, (%0), %1":: "r"(port_re_int), "r"(stride): "memory");
      asm volatile("vsse32.v v4, (%0), %1":: "r"(port_re_int+1), "r"(stride): "memory");

      // Bump pointers
      input_re_int += 2*vl;
      port_re_int     += 2*vl;
      remaining -= vl;
    } while (remaining > 0);
    break;
    }

    case 3: {
          unsigned int remaining = dim;
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + dotp_l.M ), "r"(stride)); 
      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + dotp_l.M + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }
      // Multiply
      asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[0]));
      asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v24, v0, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v28, v4, %0":: "f"(port_weights[0]));
      asm volatile("vfsub.vv v0, v16, v20");
      asm volatile("vfadd.vv v4, v24, v28");

      asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[2]));
      asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[3]));
      asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[3]));
      asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[2]));
      asm volatile("vfsub.vv v8, v16, v20");
      asm volatile("vfadd.vv v12, v24, v28");
      asm volatile("vfadd.vv v0, v0, v8");
      asm volatile("vfadd.vv v4, v4, v12");

      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + 2*dotp_l.M ), "r"(stride)); 
      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + 2*dotp_l.M + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }

      asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[4]));
      asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[5]));
      asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[5]));
      asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[4]));
      asm volatile("vfsub.vv v8, v16, v20");
      asm volatile("vfadd.vv v12, v24, v28");
      asm volatile("vfadd.vv v0, v0, v8");
      asm volatile("vfadd.vv v4, v4, v12");


      asm volatile("vsse32.v v0, (%0), %1":: "r"(port_re_int), "r"(stride): "memory");
      asm volatile("vsse32.v v4, (%0), %1":: "r"(port_re_int+1), "r"(stride): "memory");

      // Bump pointers
      input_re_int += 2*vl;
      port_re_int     += 2*vl;
      remaining -= vl;
    } while (remaining > 0);
    break;
    }

        case 4: {
          unsigned int remaining = dim;
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + dotp_l.M ), "r"(stride)); 
      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + dotp_l.M + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }
      // Multiply
      asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[0]));
      asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v24, v0, %0":: "f"(port_weights[1]));
      asm volatile("vfmul.vf v28, v4, %0":: "f"(port_weights[0]));
      asm volatile("vfsub.vv v0, v16, v20");
      asm volatile("vfadd.vv v4, v24, v28");

      asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[2]));
      asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[3]));
      asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[3]));
      asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[2]));
      asm volatile("vfsub.vv v8, v16, v20");
      asm volatile("vfadd.vv v12, v24, v28");
      asm volatile("vfadd.vv v0, v0, v8");
      asm volatile("vfadd.vv v4, v4, v12");

      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + 2*dotp_l.M ), "r"(stride)); 
      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + 2*dotp_l.M + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }

      asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[4]));
      asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[5]));
      asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[5]));
      asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[4]));
      asm volatile("vfsub.vv v8, v16, v20");
      asm volatile("vfadd.vv v12, v24, v28");
      asm volatile("vfadd.vv v0, v0, v8");
      asm volatile("vfadd.vv v4, v4, v12");

      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + 3*dotp_l.M ), "r"(stride)); 
      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + 3*dotp_l.M + 1), "r"(stride));
      for (int i = 0; i < 40; i++){
        asm volatile("nop");
      }

      asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[6]));
      asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[7]));
      asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[7]));
      asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[6]));
      asm volatile("vfsub.vv v8, v16, v20");
      asm volatile("vfadd.vv v12, v24, v28");
      asm volatile("vfadd.vv v0, v0, v8");
      asm volatile("vfadd.vv v4, v4, v12");


      asm volatile("vsse32.v v0, (%0), %1":: "r"(port_re_int), "r"(stride): "memory");
      asm volatile("vsse32.v v4, (%0), %1":: "r"(port_re_int+1), "r"(stride): "memory");

      // Bump pointers
      input_re_int += 2*vl;
      port_re_int     += 2*vl;
      remaining -= vl;
    } while (remaining > 0);
    break;
    }
  }
}
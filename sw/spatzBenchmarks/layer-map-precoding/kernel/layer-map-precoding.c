#include <snrt.h>
//#include DATAHEADER   
int nof_layers = 2;
int8_t *input_re;
float *port_weights;
int nof_ports = 2;

void layer_map_precoding_v32b(int8_t *input_re_in_dram, float *port_weights_in_dram, uint16_t *port_re) {
  uint32_t cid = snrt_cluster_core_idx();
  uint32_t num_cores = snrt_cluster_core_num();
  unsigned int dim = (dotp_l.M / num_cores)/2;
  const unsigned int orig_avl = dim;
  unsigned int vl;
  unsigned int stride = 2;
  unsigned int stride2 = nof_layers * 2;

  float red;
    // Allocate the vectors
  if (cid == 0) {
    input_re = (int8_t *)snrt_l1alloc(nof_ports * nof_layers * dotp_l.M * sizeof(int8_t));
    port_weights = (float *)snrt_l1alloc(nof_ports * nof_layers * 2 * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(input_re, input_re_in_dram, nof_ports * nof_layers * dotp_l.M * sizeof(int8_t));
    snrt_dma_start_1d(port_weights, port_weights_in_dram, nof_ports * nof_layers * 2 * sizeof(float));
    snrt_dma_wait_all();
  }

    // Wait for all cores to finish
  snrt_cluster_hw_barrier();
    // Calculate internal pointers

  for (int i_port = 0; i_port < nof_ports; i_port++){
    int8_t  *input_re_int = input_re + 2 * dim * cid;
    uint16_t *port_re_int = port_re + 2 * dim * cid;
    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    switch (nof_layers){
      case 1: {
      unsigned int remaining = dim;
      do {
        // Set the vl
        asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(remaining));
        // Load chunk a and b
        asm volatile("vlse8.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
        asm volatile("vlse8.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }
        asm volatile("vwadd.vx v8, v0, zero");
        asm volatile("vwadd.vx v12, v4, zero");
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vwadd.vx v16, v8, zero");   // v8(e16) -> v16(e32), real
        asm volatile("vwadd.vx v20, v12, zero");  // v12(e16) -> v20(e32), imag
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

        asm volatile("vfcvt.f.x.v v16, v16");   // v16 = float(real)
        asm volatile("vfcvt.f.x.v v20, v20");   // v20 = float(imag)
        // Multiply
        asm volatile("vfmul.vf v0, v16, %0":: "f"(port_weights[i_port * nof_layers * 2]));
        asm volatile("vfmul.vf v4, v20, %0":: "f"(port_weights[i_port * nof_layers * 2 + 1]));
        asm volatile("vfmul.vf v8, v16, %0":: "f"(port_weights[i_port * nof_layers * 2 + 1]));
        asm volatile("vfmul.vf v12, v20, %0":: "f"(port_weights[i_port * nof_layers * 2]));
        asm volatile("vfsub.vv v24, v0, v4");
        asm volatile("vfadd.vv v28, v8, v12");

        asm volatile("vsrl.vi v24, v24, 16");
        asm volatile("vand.vx v28, v28, %0":: "r"(0xFFFF0000));
        asm volatile("vor.vv v0, v24, v28"); 
        asm volatile("vse32.v v0, (%0)":: "r"(port_re_int + i_port * dotp_l.M): "memory");
        // Bump pointers
        input_re_int += 2*vl;
        port_re_int  += vl;
        remaining -= vl;
      } while (remaining > 0);
      break;
      }

      case 2: {
            unsigned int remaining = dim;
      do {
        // Set the vl
        asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(remaining));
        // Load chunk a and b
        asm volatile("vlse8.v v0, (%0), %1":: "r"(input_re_int), "r"(stride2));       //Layer 0 Re
        asm volatile("vlse8.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride2));   //Layer 0 Im
        asm volatile("vlse8.v v8, (%0), %1":: "r"(input_re_int + 2), "r"( stride2));  //Layer 1 Re
        asm volatile("vlse8.v v12, (%0), %1":: "r"(input_re_int + 3), "r"( stride2)); // Layer 1 Im
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }
        asm volatile("vwadd.vx v16, v0, zero");
        asm volatile("vwadd.vx v20, v4, zero");
        asm volatile("vwadd.vx v24, v8, zero");
        asm volatile("vwadd.vx v28, v12, zero");
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vwadd.vx v0, v16, zero");
        asm volatile("vwadd.vx v4, v20, zero");
        asm volatile("vwadd.vx v8, v24, zero");
        asm volatile("vwadd.vx v12, v28, zero");
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

        asm volatile("vfcvt.f.x.v v0, v0");
        asm volatile("vfcvt.f.x.v v4, v4");
        asm volatile("vfcvt.f.x.v v8, v8");
        asm volatile("vfcvt.f.x.v v12, v12");
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));
        // Multiply
        asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[i_port * nof_layers * 2]));
        asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[i_port * nof_layers * 2 + 1]));
        asm volatile("vfmul.vf v24, v0, %0":: "f"(port_weights[i_port * nof_layers * 2 + 1]));
        asm volatile("vfmul.vf v28, v4, %0":: "f"(port_weights[i_port * nof_layers * 2]));
        asm volatile("vfsub.vv v0, v16, v20");
        asm volatile("vfadd.vv v4, v24, v28");

        asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[i_port * nof_layers * 2 + 2]));
        asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[i_port * nof_layers * 2 +3]));
        asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[i_port * nof_layers * 2 + 3]));
        asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[i_port * nof_layers * 2 + 2]));
        asm volatile("vfsub.vv v8, v16, v20");
        asm volatile("vfadd.vv v12, v24, v28");
        asm volatile("vfadd.vv v0, v0, v8");
        asm volatile("vfadd.vv v4, v4, v12");      
        asm volatile("vsrl.vi v0, v0, 16");
        asm volatile("vand.vx v4, v4, %0":: "r"(0xFFFF0000));
        asm volatile("vor.vv v0, v0, v4"); 
        asm volatile("vse32.v v0, (%0)":: "r"(port_re_int + i_port * dotp_l.M): "memory");
        // Bump pointers
        input_re_int += vl * stride2;
        port_re_int  += vl;
        remaining -= vl;
      } while (remaining > 0);
      break;
      }
/*
      case 3: {
            unsigned int remaining = dim;
      do {
        // Set the vl
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

        // Load chunk a and b
        asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int + i_port * 3 * dotp_l.M), "r"(stride)); 
        asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + i_port * 3 * dotp_l.M 1), "r"(stride));
        asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + i_port * 3 * dotp_l.M + dotp_l.M ), "r"(stride)); 
        asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + i_port * 3 * dotp_l.M + dotp_l.M + 1), "r"(stride));
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }
        // Multiply
        asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[i_port][0]));
        asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[i_port][1]));
        asm volatile("vfmul.vf v24, v0, %0":: "f"(port_weights[i_port][1]));
        asm volatile("vfmul.vf v28, v4, %0":: "f"(port_weights[i_port][0]));
        asm volatile("vfsub.vv v0, v16, v20");
        asm volatile("vfadd.vv v4, v24, v28");

        asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[i_port][2]));
        asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[i_port][3]));
        asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[i_port][3]));
        asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[i_port][2]));
        asm volatile("vfsub.vv v8, v16, v20");
        asm volatile("vfadd.vv v12, v24, v28");
        asm volatile("vfadd.vv v0, v0, v8");
        asm volatile("vfadd.vv v4, v4, v12");

        asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + i_port * 3 * dotp_l.M + 2*dotp_l.M ), "r"(stride)); 
        asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + i_port * 3 * dotp_l.M + 2*dotp_l.M + 1), "r"(stride));
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }

        asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[i_port][4]));
        asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[i_port][5]));
        asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[i_port][5]));
        asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[i_port][4]));
        asm volatile("vfsub.vv v8, v16, v20");
        asm volatile("vfadd.vv v12, v24, v28");
        asm volatile("vfadd.vv v0, v0, v8");
        asm volatile("vfadd.vv v4, v4, v12");


        asm volatile("vsse32.v v0, (%0), %1":: "r"(port_re_int + i_port * dotp_l.M), "r"(stride): "memory");
        asm volatile("vsse32.v v4, (%0), %1":: "r"(port_re_int + i_port * dotp_l.M + 1), "r"(stride): "memory");

        // Bump pointers
        input_re_int += 2*vl;
        port_re_int  += 2*vl;
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
        asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M), "r"(stride)); 
        asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + 1), "r"(stride));
        asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + dotp_l.M ), "r"(stride)); 
        asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + dotp_l.M + 1), "r"(stride));
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }
        // Multiply
        asm volatile("vfmul.vf v16, v0, %0":: "f"(port_weights[i_port][0]));
        asm volatile("vfmul.vf v20, v4, %0":: "f"(port_weights[i_port][1]));
        asm volatile("vfmul.vf v24, v0, %0":: "f"(port_weights[i_port][1]));
        asm volatile("vfmul.vf v28, v4, %0":: "f"(port_weights[i_port][0]));
        asm volatile("vfsub.vv v0, v16, v20");
        asm volatile("vfadd.vv v4, v24, v28");

        asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[i_port][2]));
        asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[i_port][3]));
        asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[i_port][3]));
        asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[i_port][2]));
        asm volatile("vfsub.vv v8, v16, v20");
        asm volatile("vfadd.vv v12, v24, v28");
        asm volatile("vfadd.vv v0, v0, v8");
        asm volatile("vfadd.vv v4, v4, v12");

        asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + 2*dotp_l.M ), "r"(stride)); 
        asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + 2*dotp_l.M + 1), "r"(stride));
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }

        asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[i_port][4]));
        asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[i_port][5]));
        asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[i_port][5]));
        asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[i_port][4]));
        asm volatile("vfsub.vv v8, v16, v20");
        asm volatile("vfadd.vv v12, v24, v28");
        asm volatile("vfadd.vv v0, v0, v8");
        asm volatile("vfadd.vv v4, v4, v12");

        asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + 3*dotp_l.M ), "r"(stride)); 
        asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int + i_port * 4 * dotp_l.M + 3*dotp_l.M + 1), "r"(stride));
        for (int i = 0; i < 40; i++){
          asm volatile("nop");
        }

        asm volatile("vfmul.vf v16, v8, %0":: "f"(port_weights[i_port][6]));
        asm volatile("vfmul.vf v20, v12, %0":: "f"(port_weights[i_port][7]));
        asm volatile("vfmul.vf v24, v8, %0":: "f"(port_weights[i_port][7]));
        asm volatile("vfmul.vf v28, v12, %0":: "f"(port_weights[i_port][6]));
        asm volatile("vfsub.vv v8, v16, v20");
        asm volatile("vfadd.vv v12, v24, v28");
        asm volatile("vfadd.vv v0, v0, v8");
        asm volatile("vfadd.vv v4, v4, v12");


        asm volatile("vsse32.v v0, (%0), %1":: "r"(port_re_int + i_port * dotp_l.M), "r"(stride): "memory");
        asm volatile("vsse32.v v4, (%0), %1":: "r"(port_re_int + i_port * dotp_l.M + 1), "r"(stride): "memory");

        // Bump pointers
        input_re_int += 2*vl;
        port_re_int  += 2*vl;
        remaining -= vl;
      } while (remaining > 0);
      break;
      } */
    }
  }
}
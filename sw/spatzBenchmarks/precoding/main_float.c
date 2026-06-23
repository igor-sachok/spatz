// Copyright 2022 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>
#include <math.h>

#include DATAHEADER
#include "kernel/precoding.c"
unsigned int timer;

float *port_re;

static inline int fp_check(float *port_re, const float *a, const float *b) {
    const float threshold = 0.001f;
    for (int i = 0; i < dotp_l.M; i += 2) {
        float gold_re = 0.0f;
        float gold_im = 0.0f;
        for (int l = 0; l < nof_layers; l++) {
            // layer l начинается с a + l*dotp_l.M
            float in_re = a[l * dotp_l.M + i];
            float in_im = a[l * dotp_l.M + i + 1];
            float w_re  = b[l * 2];      // вес re для слоя l
            float w_im  = b[l * 2 + 1]; // вес im для слоя l
            gold_re += in_re * w_re - in_im * w_im;
            gold_im += in_re * w_im + in_im * w_re;
        }
        if (fabsf(port_re[i]   - gold_re) > 1000*threshold)
            printf("Error re[%d]: got %f, gold %f\n", i,   port_re[i],   gold_re);
        if (fabsf(port_re[i+1] - gold_im) > threshold)
            printf("Error im[%d]: got %f, gold %f\n", i+1, port_re[i+1], gold_im);
    }
    return 0;
}

int main() {
  //const unsigned int num_cores = snrt_cluster_core_num();
 // const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  timer = (unsigned int)-1;
  uint32_t num_cores = snrt_cluster_core_num();
  uint32_t cid = snrt_cluster_core_idx();
  const unsigned int dim = (dotp_l.M / num_cores)/2;
  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    port_re = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
  }
  snrt_cluster_hw_barrier();
  // Calculate dotp
  precoding_v32b(input_re_in_dram, port_weights_in_dram, port_re);
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  //if (cid == 0) {
   // printf("result_re [0] = %f\n, result_im[0]= %f\n,result_re [1] = %f\n, result_im[1]= %f\n", result_re[0], result_im[0],result_re[1], result_im[1]  );
  //}

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * dotp_l.M / (timer);
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    printf("\n----- (%d) sp fdotp -----\n", dotp_l.M);
    //printf("The execution took %u cycles.\n", timer);
    printf("The memory allocation and data copy took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0)
    if (fp_check(port_re, input_re_in_dram, port_weights_in_dram)) {
      return -1;
    }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}

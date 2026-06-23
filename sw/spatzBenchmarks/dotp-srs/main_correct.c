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
#include "kernel/fdotp.c"

float *a;
float *b;
float *result_re;
float *result_im;
float *buf_a_re;
float *buf_a_im;
float *buf_b_re;
float *buf_b_im;

static inline int fp_check(float *result_re, float *result_im ,const float *a, const float *b) {
  const float threshold = 0.00001;
  // Absolute value
  float gold_res_re = 0;
  float gold_res_im = 0;
  for (int i = 0; i < dotp_l.M; i+=2){
    gold_res_re += a[i]*b[i] + a[i+1]*b[i+1];
    gold_res_im += a[i+1]*b[i] - a[i]*b[i+1]; 
  }
  if (fabsf(gold_res_re - result_re[0]) > threshold){
    printf("oh shush Error: result_re = %f, while gold_res_re = %f \n", result_re[0], gold_res_re);
  }
  if (fabsf(gold_res_im - result_im[0]) > threshold){
    printf("oh shush Error: result_im = %f, while gold_res_im = %f \n", result_im[0], gold_res_im);
  }
  return 0;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  const unsigned int dim = (dotp_l.M / num_cores)/2;

  // Allocate the vectors
  if (cid == 0) {
    a = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    b = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    result_re = (float *)snrt_l1alloc(num_cores * sizeof(float));
    result_im = (float *)snrt_l1alloc(num_cores * sizeof(float));
    buf_a_re = (float *)snrt_l1alloc(num_cores * sizeof(float));
    buf_a_im = (float *)snrt_l1alloc(num_cores * sizeof(float));
    buf_b_re = (float *)snrt_l1alloc(num_cores * sizeof(float));
    buf_b_im = (float *)snrt_l1alloc(num_cores * sizeof(float));
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

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  // Calculate dotp
  float acc_re = 0.0f;
  float acc_im = 0.0f;
  fdotp_cf_v32b(a_int, b_int, &result_re[cid], &result_im[cid], dim);
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  //if (cid == 0) {
   // printf("result_re [0] = %f\n, result_im[0]= %f\n,result_re [1] = %f\n, result_im[1]= %f\n", result_re[0], result_im[0],result_re[1], result_im[1]  );
  //}

  // Final reduction
  if (cid == 0) {
    for (unsigned int i = 1; i < num_cores; ++i) {
          acc_re += result_re[i];
          acc_im += result_im[i];
    }
    result_re[0] += acc_re;
    result_im[0] += acc_im;
  }
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
  if (cid == 0) {
    printf("Now the results should be desplayed\n");
  }
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * dotp_l.M / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    printf("\n----- (%d) sp fdotp -----\n", dotp_l.M);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0)
    if (fp_check(&result_re[0], &result_im[0], a, b)) {
      return -1;
    }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}

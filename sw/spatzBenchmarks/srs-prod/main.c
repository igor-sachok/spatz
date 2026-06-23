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
#include "kernel/prod.c"

float *a;
float *b;
float *result;
float *buffer_a_re;
float *buffer_a_im;
float *buffer_b_re;
float *buffer_b_im;
float eps = 1e-5f;
int prod_check(const float *a, const float *b, float *result) {
  for (unsigned int i = 0; i + 1 < dotp_l.M; i += 2) {
    if (fabsf(a[i] * b[i] - a[i+1] * b[i+1] - result[i]) > eps ) {
      printf("Incorrect result[%i] = %f \n", i, result[i]);
       printf("Correct result[%i] = %f \n", i, a[i] * b[i] - a[i+1] * b[i+1]);
    }
    if (fabsf(a[i] * b[i+1] + a[i+1] * b[i] - result[i+1]) > eps){
      printf("Incorrect result[%i] = %f \n", i+1, result[i+1]);
      printf("Correct result[%i] = %f \n", i+1, a[i] * b[i+1] + a[i+1] * b[i]);
    }
  }
  return 0;
}


int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  const unsigned int dim = dotp_l.M / num_cores;
  unsigned int p_start, p_end;

  // Allocate the matrices
  if (cid == 0) {
    a = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    b = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    result = (float *)snrt_l1alloc(dotp_l.M * sizeof(float)); 
    buffer_a_re = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    buffer_a_im = (float *)snrt_l1alloc(dotp_l.M * sizeof(float)); 
    buffer_b_re = (float *)snrt_l1alloc(dotp_l.M * sizeof(float));
    buffer_b_im = (float *)snrt_l1alloc(dotp_l.M * sizeof(float)); 
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, dotp_A_dram, dotp_l.M * sizeof(float));
    snrt_dma_start_1d(b, dotp_B_dram, dotp_l.M * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  if (cid == 0) {
    printf("dim = %i \n", dim);
  }
  p_start = cid * dim;
  if (cid == num_cores - 1) {
    p_end = dotp_l.M;
  } else {
    p_end = (cid + 1) * dim;
  }
  // Calculate dotp
  fprod_cf_v32b(a, b, result, p_start, p_end);
  //if (cid == 0) {
    // for (unsigned int i = 0; i < 8; i += 1) {
      //printf("result[%i] = %f \n", i, result[i]);
    //}
 // }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

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
    long unsigned int performance = 1000 * 2 * dotp_l.M / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    printf("\n----- (%d) f prod -----\n", dotp_l.M);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0)
    if (prod_check(a, b, result)) {
      printf("Error!");
      return -1;
    }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}

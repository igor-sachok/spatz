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
#include "kernel/hard_decision.c"

int8_t *soft_bits;
int8_t *hard_bits;

int hard_decision_check(const int8_t *hard_bits, const int8_t *soft_bits, const uint32_t offset,
                         const uint32_t lifting_size, bool returned_result) {
  bool expected_no_zero = true;
  for (unsigned int i = 0; i < lifting_size; ++i) {
    int8_t soft = soft_bits[offset + i];
    // Expected hard bit: (soft <= 0) ? 1 : 0
    int8_t expected_hard = (soft <= 0) ? 1 : 0;
    int8_t actual_hard   = hard_bits[offset + i];
    if (expected_hard != actual_hard) {
      printf("Incorrect hard_bits[%u] = %d \n", i, actual_hard);
      printf("Correct hard_bits[%u] = %d \n", i, expected_hard);
    }
    if (soft == 0) {
      expected_no_zero = false;
    }
  }
  if (returned_result != expected_no_zero) {
    printf("Incorrect return value = %d \n", returned_result);
    printf("Correct return value = %d \n", expected_no_zero);
  }
  return 0;
}


int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    soft_bits = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t));
    hard_bits = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t)); 
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(soft_bits, hard_decision_soft_bits_dram, num_cores * lifting_size * sizeof(int8_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  bool result;

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();
  //One hard_decision per core.
  snrt_cluster_hw_barrier();
  result = hard_decision(hard_bits, soft_bits, cid * lifting_size, lifting_size);
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
    long unsigned int performance = 1000 * 3 * 2 * lifting_size / timer;
    long unsigned int utilization =
        performance / (4 * num_cores * 1);
        // data 8bit how many can IPU process? probably 4
        // (1 IPU per Spatz core)

    printf("\n----- (%d) hard_decision -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    hard_decision_check(hard_bits, soft_bits, 0, lifting_size, result);
  }
  if (cid == 0) {
   hard_decision_check(hard_bits, soft_bits, lifting_size, lifting_size, result);
  }
  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}
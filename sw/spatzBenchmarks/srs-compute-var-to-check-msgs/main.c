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
#include "kernel/compute_var_to_check_msgs.c"

int8_t *this_soft_bits;
int8_t *this_var_to_check;
int8_t *this_check_to_var;

#define LLR_MAX     120
#define LLR_INFTY   127

static bool llr_isinf(int8_t v)
{
  return (v == LLR_INFTY) || (v == -LLR_INFTY);
}

static int8_t tackle_special_sums_ref(int8_t val_a, int8_t val_b, int *found)
{
  *found = 1;

  if (val_a == -val_b) {
    return 0;
  }
  if (llr_isinf(val_a)) {
    return val_a;
  }
  if (llr_isinf(val_b)) {
    return val_b;
  }

  *found = 0;
  return 0;
}

static int8_t saturated_sum_ref(int8_t a, int8_t b)   // = operator+=
{
  int found;
  int8_t special = tackle_special_sums_ref(a, b, &found);
  if (found) {
    return special;
  }
  int tmp = (int)a + (int)b;
  if (abs(tmp) > LLR_MAX) {
    return (tmp > 0) ? (int8_t)LLR_MAX : (int8_t)(-LLR_MAX);  // ±120, НЕ ±127
  }
  return (int8_t)tmp;
}

static int8_t var_to_check_ref(int8_t a, int8_t b)    // v2c = a - b = a + (-b)
{
  return saturated_sum_ref(a, (int8_t)(-b));
}

int compute_var_to_check_check(const int8_t *var_to_check,
                               const int8_t *soft_bits,
                               const int8_t *check_to_var,
                               const uint32_t offset,
                               const uint32_t lifting_size)
{
  int errors = 0;
  for (unsigned int i = 0; i < lifting_size; ++i) {
    int8_t a        = soft_bits[offset + i];
    int8_t b        = check_to_var[offset + i];
    int8_t expected = var_to_check_ref(a, b);
    int8_t actual   = var_to_check[offset + i];
    if (expected != actual) {
      printf("Mismatch at [%u]: a=%d b=%d -> got %d, expected %d\n",
             i, a, b, actual, expected);
      ++errors;
    }
  }
  if (errors == 0) {
    printf("compute_var_to_check_msgs: OK (%u elements)\n", lifting_size);
  } else {
    printf("compute_var_to_check_msgs: %d errors out of %u\n", errors, lifting_size);
  }
  return errors;
}
/*
int main(void) {
  uint8_t vs2[16], vs1[16], vd[16], mask[16];
  size_t vl;
  size_t remaining = 16;

  // Источники: vs2 = 0xA0.., vs1 = 0x0B.. чтобы наглядно различать
  for (int i = 0; i < 16; i++) {
    vs2[i]  = 0xA0 + i;   // ветка v0=0
    vs1[i]  = 0x0B + i;   // ветка v0=1
    vd[i]   = 0xFF;       // мусор, должен перезаписаться целиком
    mask[i] = 0;
  }
  // Маска v0 упакована по битам: элемент i -> байт i/8, бит i%8.
  // Возьмём чередование 1,0,1,0,... -> mask[0]=0x55, mask[1]=0x55
  mask[0] = 0x55;  // биты 0,2,4,6 = 1  -> элементы 0,2,4,6 берут vs1
  mask[1] = 0x55;  // биты 8,10,12,14=1 -> элементы 8,10,12,14 берут vs1

  asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(remaining));
  asm volatile("vle8.v v0,  (%0)" :: "r"(mask));   // v0 = маска
  asm volatile("vle8.v v2,  (%0)" :: "r"(vs2));    // v2 = vs2 (m2 -> v2,v3)
  asm volatile("vle8.v v4,  (%0)" :: "r"(vs1));    // v4 = vs1 (m2 -> v4,v5)
  asm volatile("vmerge.vvm v6, v2, v4, v0");       // v6 = v0.mask ? vs1 : vs2
  asm volatile("vse8.v v6,  (%0)" :: "r"(vd));     // vd <- v6

  printf("vl = %zu\n", vl);
  printf("idx  v0  vs2  vs1  vd(result)  expect\n");
  for (int i = 0; i < (int)vl; i++) {
    int mbit = (mask[i / 8] >> (i % 8)) & 1;
    uint8_t exp = mbit ? vs1[i] : vs2[i];
    printf("%2d   %d  0x%02X 0x%02X    0x%02X       0x%02X %s\n",
           i, mbit, vs2[i], vs1[i], vd[i], exp,
           (vd[i] == exp) ? "OK" : "FAIL");
  }
  return 0;
} */
/*
#define N 16

int main(void) {
  int16_t src[N] = {0, 1, 119, 120, 121, 200, 127, -1,
                    -119, -120, -121, -200, -127, 500, -500, 120};
  int16_t res[N], ref[N];
  size_t vl; size_t n = N;
  int16_t hi = 120, lo = -120, inf = 127;

  asm volatile("vsetvli %0, %1, e16, m4, ta, mu" : "=r"(vl) : "r"(n));
  asm volatile("vle16.v v12, (%0)" :: "r"(src) : "memory");
  asm volatile("vmv.v.i v28, 0");
  asm volatile("vmv.v.v v16, v12");          // сохранить исходное

  asm volatile("vmsgt.vx v0, v12, %0" :: "r"(hi));      // 1
  asm volatile("vadd.vx  v12, v28, %0, v0.t" :: "r"(hi));
  asm volatile("vmslt.vx v0, v12, %0" :: "r"(lo));      // 2
  asm volatile("vadd.vx  v12, v28, %0, v0.t" :: "r"(lo));
  asm volatile("vmseq.vx v0, v16, %0" :: "r"(inf));     // 3
  asm volatile("vadd.vx  v12, v28, %0, v0.t" :: "r"(inf));
  asm volatile("vmseq.vi v0, v16, 0");                  // 4
  asm volatile("vadd.vi  v12, v28, 0, v0.t");

  asm volatile("vse16.v v12, (%0)" :: "r"(res) : "memory");

  for (int i = 0; i < N; i++) {
    int16_t v = src[i];
    if (v > hi) v = hi;
    if (v < lo) v = lo;
    if (src[i] == inf) v = inf;
    if (src[i] == 0)   v = 0;
    ref[i] = v;
  }
  int f = 0;
  for (int i = 0; i < N; i++)
    if (res[i] != ref[i]) { f++;
      printf("[%2d] src=%5d got %5d exp %5d FAIL\n", i, src[i], res[i], ref[i]); }
  printf("%s (%d mismatches)\n", f ? "FAILED" : "PASSED", f);
  return 0;
}
*/

#define N 32   // подставь свой lifti
#ifndef BARRIER_AT
#define BARRIER_AT -1
#endif

int16_t scratch[N];   // куда пишем барьерные store (значение не важно)

#define BARRIER(k) do { if ((k) <= BARRIER_AT) \
    asm volatile("vse16.v v12, (%0)" :: "r"(scratch) : "memory"); } while(0)

int main(void) {
  int8_t a[N], b[N];
  int16_t res[N], ref[N];
  size_t vl, n = N;
  int16_t hi=120, lo=-120, inf=127, ninf=-127;

  int8_t ia[N] = {0,59,-28,-106,0,0,0,-100,0,1,0,0,-46,0,-33,0,
                  0,0,0,0,29,0,0,0,0,0,0,9,0,0,0,40};
  int8_t ib[N] = {0,-52,-22,19,0,0,0,92,0,-68,0,0,-64,0,-12,0,
                  0,0,0,0,102,0,0,0,0,0,0,-46,0,0,0,-112};
  for (int i=0;i<N;i++){a[i]=ia[i];b[i]=ib[i];}

  asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(n));
  asm volatile("vle8.v v4, (%0)" :: "r"(a) : "memory");
  asm volatile("vle8.v v8, (%0)" :: "r"(b) : "memory");
  asm volatile("vwadd.vv v12, v4, v8");
  asm volatile("vwadd.vx v20, v4, x0");
  asm volatile("vwadd.vx v24, v8, x0");

  asm volatile("vsetvli %0, %1, e16, m4, ta, mu" : "=r"(vl) : "r"(n));
  asm volatile("vmv.v.i v28, 0");
  asm volatile("vmv.v.v v16, v12");

  asm volatile("vmsgt.vx v0, v12, %0" :: "r"(hi));
  asm volatile("vadd.vx v12, v28, %0, v0.t" :: "r"(inf));   BARRIER(0);

  asm volatile("vmslt.vx v0, v12, %0" :: "r"(lo));
  asm volatile("vadd.vx v12, v28, %0, v0.t" :: "r"(ninf));  BARRIER(1);

  asm volatile("vmseq.vx v0, v24, %0" :: "r"(inf));
  asm volatile("vadd.vv v12, v24, v28, v0.t");              BARRIER(2);

  asm volatile("vmseq.vx v0, v24, %0" :: "r"(ninf));
  asm volatile("vadd.vv v12, v24, v28, v0.t");              BARRIER(3);

  asm volatile("vmseq.vx v0, v20, %0" :: "r"(inf));
  asm volatile("vadd.vv v12, v20, v28, v0.t");              BARRIER(4);

  asm volatile("vmseq.vx v0, v20, %0" :: "r"(ninf));
  asm volatile("vadd.vv v12, v20, v28, v0.t");              BARRIER(5);

  asm volatile("vmseq.vi v0, v16, 0");
  asm volatile("vadd.vi v12, v28, 0, v0.t");                BARRIER(6);

  asm volatile("vse16.v v12, (%0)" :: "r"(res) : "memory");

  // эталон на C
  for (int i = 0; i < N; i++) {
    int16_t s = (int16_t)a[i] + (int16_t)b[i];
    int16_t v = s;
    if (s > hi) v = inf;
    if (v < lo) v = ninf;
    if (b[i] == inf)  v = b[i];
    if (b[i] == ninf) v = b[i];
    if (a[i] == inf)  v = a[i];
    if (a[i] == ninf) v = a[i];
    if (s == 0) v = 0;
    ref[i] = v;
  }

  int8_t out8[N];

  asm volatile("vfcvt.f.x.v v12, v12");
  asm volatile("vsetvli %0, %1, e8, m2, ta, mu" : "=r"(vl) : "r"(n));
  asm volatile("vfncvt.x.f.w v4, v12");
  asm volatile("vse8.v v4, (%0)" :: "r"(out8) : "memory");

  int f = 0;
  for (int i = 0; i < N; i++)
    if (out8[i] != (int8_t)ref[i]) { f++;
      printf("[%2d] a=%4d b=%4d got %5d exp %5d FAIL\n", i, a[i], b[i], out8[i], ref[i]); }
  printf("%s (%d mismatches)\n", f ? "FAILED" : "PASSED", f);
  return 0;
}

/*
int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    this_soft_bits = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t));
    this_var_to_check = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t));
    this_check_to_var = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t)); 
  }

  // Initialize the arrays
  if (cid == 0) {
    snrt_dma_start_1d(this_soft_bits,    this_soft_bits_dram,    num_cores * lifting_size * sizeof(int8_t));
    snrt_dma_start_1d(this_check_to_var, this_check_to_var_dram, num_cores * lifting_size * sizeof(int8_t));
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
  //One compute per core.
  snrt_cluster_hw_barrier();
  compute_var_to_check_msgs(this_var_to_check + cid * lifting_size,
                              this_soft_bits    + cid * lifting_size,
                              this_check_to_var + cid * lifting_size,
                              lifting_size); 
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

    printf("\n----- (%d) compute_var_to_check_msgs -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    compute_var_to_check_check(this_var_to_check, this_soft_bits, this_check_to_var,
                               0, lifting_size);
  }
  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
} */
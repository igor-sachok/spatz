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

/*
#include <stdint.h>
#include <stdio.h>
#include <snrt.h>

// ============================================================
// Systematic isolation of vredand.vs hang conditions.
// Mirrors the working fdotp_v32b structure as closely as possible,
// varying: LMUL, vd==vs2 self-reference, and whether v24 comes from
// a loop (vand.vv accumulation) vs a fresh single vmv.v.i.
// ============================================================

// ---- Test A: single-shot vredand, LMUL=8, vd != vs2 (baseline, like fdotp) ----
uint32_t testA_single_m8_distinct(uint32_t n) {
    uint32_t vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v24, -1");      // data to reduce
    asm volatile("vmv.v.i v0, -1");       // neutral accumulator
    asm volatile("vredand.vs v8, v24, v0");  // vd=v8, distinct from vs2=v24, vs1=v0
    asm volatile("vmv.x.s %0, v8" : "=r"(red));
    return red;
}

// ---- Test B: single-shot vredand, LMUL=8, vd == vs2 (self-reference) ----
uint32_t testB_single_m8_selfref(uint32_t n) {
    uint32_t vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v24, -1");
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v24, v24, v0");  // vd == vs2 == v24
    asm volatile("vmv.x.s %0, v24" : "=r"(red));
    return red;
}

// ---- Test C: like fdotp - vsetvli called TWICE (once before loop, once after) + loop with plain vand.vv, THEN single reduce, vd != vs2 ----
uint32_t testC_loop_then_reduce_distinct(int8_t *data, uint32_t n) {
    uint32_t remaining = n, vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.i v24, -1");
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v8, (%0)" :: "r"(data));
        asm volatile("vand.vv v24, v24, v8");
        data += vl;
        remaining -= vl;
    } while (remaining > 0);
    asm volatile("vsetvli zero, %0, e8, m8, ta, ma" :: "r"(n));
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v16, v24, v0");   // vd=v16, distinct
    asm volatile("vmv.x.s %0, v16" : "=r"(red));
    return red;
}

// ---- Test D: same as C but vd == vs2 (self-reference) — closest to your original bug ----
uint32_t testD_loop_then_reduce_selfref(int8_t *data, uint32_t n) {
    uint32_t remaining = n, vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.i v24, -1");
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v8, (%0)" :: "r"(data));
        asm volatile("vand.vv v24, v24, v8");
        data += vl;
        remaining -= vl;
    } while (remaining > 0);
    asm volatile("vsetvli zero, %0, e8, m8, ta, ma" :: "r"(n));
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v24, v24, v0");   // vd == vs2 == v24
    asm volatile("vmv.x.s %0, v24" : "=r"(red));
    return red;
}

// ---- Test E: single-shot vredand at LMUL=1, vd == vs2 ----
uint32_t testE_single_m1_selfref(uint32_t n) {
    uint32_t vl, red;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v3, -1");
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v3, v3, v0");
    asm volatile("vmv.x.s %0, v3" : "=r"(red));
    return red;
}

// ---- Test F: single-shot vredand at LMUL=4, vd == vs2 ----
uint32_t testF_single_m4_selfref(uint32_t n) {
    uint32_t vl, red;
    asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v4, -1");
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v4, v4, v0");
    asm volatile("vmv.x.s %0, v4" : "=r"(red));
    return red;
}

uint32_t testC2_double_vsetvli_no_loop(uint32_t n) {
    uint32_t vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v24, -1");
    asm volatile("vsetvli zero, %0, e8, m8, ta, ma" :: "r"(n));
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v16, v24, v0");
    asm volatile("vmv.x.s %0, v16" : "=r"(red));
    return red;
}
uint32_t testC3_loop_no_second_vsetvli(int8_t *data, uint32_t n) {
    uint32_t remaining = n, vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.i v24, -1");
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v8, (%0)" :: "r"(data));
        asm volatile("vand.vv v24, v24, v8");
        data += vl;
        remaining -= vl;
    } while (remaining > 0);
    // БЕЗ повторного vsetvli — используем vl/vtype от последней итерации
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v16, v24, v0");
    asm volatile("vmv.x.s %0, v16" : "=r"(red));
    return red;
}
// Test G: цикл БЕЗ памяти — только vand.vv в регистрах, потом reduce
uint32_t testG_loop_no_memory(uint32_t n) {
    uint32_t remaining = n, vl, red;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.i v24, -1");
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vmv.v.i v8, -1");        // НЕ из памяти, просто immediate
        asm volatile("vand.vv v24, v24, v8");
        remaining -= vl;
    } while (remaining > 0);
    asm volatile("vsetvli zero, %0, e8, m8, ta, ma" :: "r"(n));
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v16, v24, v0");
    asm volatile("vmv.x.s %0, v16" : "=r"(red));
    return red;
}

// Test H: цикл С памятью (vle8.v), БЕЗ vand.vv — просто грузим в v24 каждую итерацию
uint32_t testH_loop_load_no_and(int8_t *data, uint32_t n) {
    uint32_t remaining = n, vl, red;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v24, (%0)" :: "r"(data));
        data += vl;
        remaining -= vl;
    } while (remaining > 0);
    asm volatile("vsetvli zero, %0, e8, m8, ta, ma" :: "r"(n));
    asm volatile("vmv.v.i v0, -1");
    asm volatile("vredand.vs v16, v24, v0");
    asm volatile("vmv.x.s %0, v16" : "=r"(red));
    return red;
}

// Test I: reduction КАЖДУЮ итерацию цикла (vd != vs2), накопление через GPR scalar accumulator
uint32_t testI_reduce_every_iter_gpr_accum(int8_t *data, uint32_t n) {
    uint32_t remaining = n, vl, red;
    uint32_t accum = 0xFFFFFFFF;   // нейтральный элемент AND

    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v8, (%0)" :: "r"(data));

        asm volatile("vmv.v.i v0, -1");
        asm volatile("vredand.vs v16, v8, v0");   // reduce this chunk, vd=v16 (distinct from vs2=v8)

        uint32_t chunk;
        asm volatile("vmv.x.s %0, v16" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        remaining -= vl;
    } while (remaining > 0);

    red = accum;
    return red;
}

// Test J: vmsne.vi в НЕ-v0 регистр при LMUL=8, затем прямое чтение — БЕЗ reduction вообще
uint32_t testJ_vmsne_nonzero_dest_m8(int8_t *data, uint32_t n) {
    uint32_t vl, result;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle8.v v0, (%0)" :: "r"(data));
    asm volatile("vmsne.vi v16, v0, 0");      // маска -> v16 (НЕ v0!), при LMUL=8
    asm volatile("vmv.x.s %0, v16" : "=r"(result));
    return result;
}

// Test K: то же самое, но маска пишется В v0 (стандартная конвенция) — контроль
uint32_t testK_vmsne_v0_dest_m8(int8_t *data, uint32_t n) {
    uint32_t vl, result;
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle8.v v8, (%0)" :: "r"(data));   // данные грузим не в v0, чтобы освободить v0 под маску
    asm volatile("vmsne.vi v0, v8, 0");             // маска -> v0
    asm volatile("vmv.x.s %0, v0" : "=r"(result));
    return result;
}

uint32_t testI1_add_store_after(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v8, (%0)" :: "r"(data));

        asm volatile("vmv.v.i v0, -1");
        asm volatile("vredand.vs v16, v8, v0");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v16" : "=r"(chunk));
        accum &= chunk;

        // Добавили: store из v8 ПОСЛЕ reduction
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

// ---- Test I2: Test I1 + store ДО reduction (как в реальном коде) ----
uint32_t testI2_store_before_reduce(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v8, (%0)" :: "r"(data));

        // Добавили: store из v8 ДО reduction
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        asm volatile("vmv.v.i v0, -1");
        asm volatile("vredand.vs v16, v8, v0");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v16" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

// ---- Test I3: Test I2 + дополнительная арифметика на v8 ДО store (adds/srl, как hard bit) ----
uint32_t testI3_arith_then_store_then_reduce(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));   // теперь данные в v0!

        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        // reduce ИСХОДНЫЕ данные v0 (просто чтобы проверить reduce после доп. арифметики где-то рядом)
        asm volatile("vmv.v.i v16, -1");
        asm volatile("vredand.vs v24, v0, v16");   // vs2=v0(исходные данные), vs1=v16, vd=v24
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v24" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}
uint32_t testI5_broken_chain(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        // hard bit
        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        // "!= 0", БЕЗ self-reference на каждом шаге:
        // v16 = -v0            (пишем v16, читаем v0 -- ок)
        // v24 = v0 | v16       (пишем v24, читаем v0,v16 -- ок)
        // v16 = sign(v24)      (пишем v16, читаем v24 -- ок, НЕ то же самое что писали до этого)
        asm volatile("vrsub.vi v16, v0, 0");
        asm volatile("vor.vv   v24, v0, v16");
        asm volatile("vsra.vi  v16, v24, 7");

        // reduce v16, vd=v8 (свободен), vs1=v24 (свободен после vor)
        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

uint32_t testI4_full_pipeline(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        // hard bit
        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        // "!= 0" -> v16
        asm volatile("vrsub.vi v16, v0, 0");
        asm volatile("vor.vv   v16, v0, v16");
        asm volatile("vsra.vi  v16, v16, 7");

        // reduce v16 (это отличие от I3, где редуцировали v0)
        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");   // vd=v8 (свободен, данные уже сохранены), vs2=v16, vs1=v24
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}
// Test I6: I3 + vrsub.vi (проверяем ТОЛЬКО vrsub)
uint32_t testI6_add_vrsub(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        asm volatile("vrsub.vi v16, v0, 0");   // НОВОЕ: только vrsub, пишем в v16

        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");  // reduce v16 (результат vrsub)
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

// Test I7: I3 + vor.vv (проверяем ТОЛЬКО vor, на исходных данных)
uint32_t testI7_add_vor(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        asm volatile("vor.vv v16, v0, v8");    // НОВОЕ: только vor.vv, пишем в v16

        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

// Test I8: I3 + vsra.vi (проверяем ТОЛЬКО vsra, вместо vsrl)
uint32_t testI8_add_vsra(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        asm volatile("vsra.vi v16, v0, 7");    // НОВОЕ: только vsra, пишем в v16 (читаем v0, не self-ref)

        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

uint32_t testI9_reduce_alu_computed(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v16, v8, v24");   // reduce v8 (ALU-computed), НЕ v0!
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v16" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

uint32_t testI10_same_as_I9_but_v16(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        asm volatile("vadd.vi v16, v0, -1");   // было v8, теперь v16
        asm volatile("vsrl.vi v16, v16, 7");   // было v8, теперь v16
        asm volatile("vse8.v v16, (%0)" :: "r"(out));

        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");  // редуцируем v16
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

uint32_t testI11_add_delay_instr(int8_t *data, int8_t *out, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        asm volatile("vrsub.vi v16, v0, 0");   // запись v16
        asm volatile("vmv.v.i v24, -1");        // "буферная" независимая инструкция (уже есть)
        asm volatile("vadd.vi v24, v24, 0");    // ЕЩЁ одна независимая инструкция — доп. задержка

        asm volatile("vredand.vs v8, v16, v24");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

// Test I12: store как барьер между вычислением "!=0" и reduce
uint32_t testI12_store_barrier(int8_t *data, int8_t *out, int8_t *scratch, uint32_t n) {
    uint32_t remaining = n, vl;
    uint32_t accum = 0xFFFFFFFF;
    do {
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v0, (%0)" :: "r"(data));

        // hard bit
        asm volatile("vadd.vi v8, v0, -1");
        asm volatile("vsrl.vi v8, v8, 7");
        asm volatile("vse8.v v8, (%0)" :: "r"(out));

        // "!= 0" -> v16
        asm volatile("vrsub.vi v16, v0, 0");
        asm volatile("vor.vv   v16, v0, v16");
        asm volatile("vsra.vi  v16, v16, 7");

        // барьер: store v16 в scratch перед reduce
        asm volatile("vse8.v v16, (%0)" :: "r"(scratch));

        asm volatile("vmv.v.i v24, -1");
        asm volatile("vredand.vs v8, v16, v24");
        uint32_t chunk;
        asm volatile("vmv.x.s %0, v8" : "=r"(chunk));
        accum &= chunk;

        data += vl;
        out += vl;
        remaining -= vl;
    } while (remaining > 0);
    return accum;
}

int main() {
    const unsigned int cid = snrt_cluster_core_idx();
    int8_t *data;
    int8_t *out_buf;
    int8_t *scratch_buf;
      if (cid == 0) {
    data = (int8_t *)snrt_l1alloc(512 * 2 * sizeof(int8_t));
    out_buf = (int8_t *)snrt_l1alloc(512 * 2 * sizeof(int8_t));
    scratch_buf = (int8_t *)snrt_l1alloc(512 * 2 * sizeof(int8_t));
  }

    if (cid == 0) {
        for (int i = 0; i < 512; ++i) data[i] = 1;

        printf("Test A (single-shot, m8, vd!=vs2)...\n");
        uint32_t rA = testA_single_m8_distinct(256);
        printf("Test A result = %u\n", rA);

        printf("Test I (reduce every iter, GPR accumulator)...\n");
uint32_t rI = testI_reduce_every_iter_gpr_accum(data, 256);
printf("Test I result = %u\n", rI);
printf("Test I1 (store AFTER reduce)...\n");
uint32_t rI1 = testI1_add_store_after(data, out_buf, 256);
printf("Test I1 result = %u\n", rI1);

printf("Test I2 (store BEFORE reduce)...\n");
uint32_t rI2 = testI2_store_before_reduce(data, out_buf, 256);
printf("Test I2 result = %u\n", rI2);

printf("Test I3 (arith + store + reduce on v0)...\n");
uint32_t rI3 = testI3_arith_then_store_then_reduce(data, out_buf, 256);
printf("Test I3 result = %u\n", rI3);

printf("Test I9 (reduce ALU-computed v8, not raw-load v0)...\n");
uint32_t rI9 = testI9_reduce_alu_computed(data, out_buf, 256);
printf("Test I9 result = %u\n", rI9);


printf("Test I10 (reduce ALU-computed v16, not raw-load v0)...\n");
uint32_t rI10 = testI10_same_as_I9_but_v16(data, out_buf, 256);
printf("Test I10 result = %u\n", rI10);

printf("Test I12 (store barrier before reduce)...\n");
uint32_t rI12 = testI12_store_barrier(data, out_buf, scratch_buf, 256);
printf("Test I12 result = %u\n", rI12);


printf("Test I11 (reduce ALU-computed v16, not raw-load v0)...\n");
uint32_t rI11 = testI11_add_delay_instr(data, out_buf, 256);
printf("Test I11 result = %u\n", rI11);

printf("Test I6 (I3 + vrsub.vi only)...\n");
uint32_t rI6 = testI6_add_vrsub(data, out_buf, 256);
printf("Test I6 result = %u\n", rI6);

printf("Test I7 (I3 + vor.vv only)...\n");
uint32_t rI7 = testI7_add_vor(data, out_buf, 256);
printf("Test I7 result = %u\n", rI7);

printf("Test I8 (I3 + vsra.vi only)...\n");
uint32_t rI8 = testI8_add_vsra(data, out_buf, 256);
printf("Test I8 result = %u\n", rI8);

printf("Test I5 (broken self-ref chain in !=0 computation)...\n");
uint32_t rI5 = testI5_broken_chain(data, out_buf, 256);
printf("Test I5 result = %u\n", rI5);
printf("Test I4 (full pipeline: hardbit + !=0 arith + reduce v16)...\n");
uint32_t rI4 = testI4_full_pipeline(data, out_buf, 256);
printf("Test I4 result = %u\n", rI4);

        printf("Test B (single-shot, m8, vd==vs2)...\n");
        uint32_t rB = testB_single_m8_selfref(256);
        printf("Test B result = %u\n", rB);

        printf("Test E (single-shot, m1, vd==vs2)...\n");
        uint32_t rE = testE_single_m1_selfref(16);
        printf("Test E result = %u\n", rE);

        printf("Test F (single-shot, m4, vd==vs2)...\n");
        uint32_t rF = testF_single_m4_selfref(64);
        printf("Test F result = %u\n", rF);
        printf("Test G (loop, no memory, vand.vv only)...\n");
uint32_t rG = testG_loop_no_memory(256);
printf("Test G result = %u\n", rG);

printf("Test H (loop with vle8.v, no vand.vv)...\n");
uint32_t rH = testH_loop_load_no_and(data, 256);
printf("Test H result = %u\n", rH);

        printf("Test C2 ...\n");
        uint32_t rC2 = testC2_double_vsetvli_no_loop(256);
        printf("Test C result = %u\n", rC2);

        printf("Test C3 ...\n");
        uint32_t rC3 = testC3_loop_no_second_vsetvli(data, 256);
        printf("Test C result = %u\n", rC3);

        printf("Test C (loop + vand.vv + single reduce, vd!=vs2)...\n");
        uint32_t rC = testC_loop_then_reduce_distinct(data, 256);
        printf("Test C result = %u\n", rC);

        printf("Test D (loop + vand.vv + single reduce, vd==vs2)...\n");
        uint32_t rD = testD_loop_then_reduce_selfref(data, 256);
        printf("Test D result = %u\n", rD);

        printf("All tests completed.\n");
    }

    return 0;
}
*/
/*
int main() {
    const unsigned int cid = snrt_cluster_core_idx();

    if (cid == 0) {
        uint32_t vl;
        uint32_t result;

        printf("Starting reduction-after-vfu-op deadlock repro...\n");

        // Any non-reduction VFU instruction immediately followed by a
        // reduction instruction on the same/dependent register is enough
        // to trigger the race.
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(64));
        asm volatile("vmv.v.i v0, 1");           
        asm volatile("vrsub.vi v16, v0, 0");      
        asm volatile("vmv.v.i v24, -1");         
        asm volatile("vredand.vs v8, v16, v24"); 
        asm volatile("vmv.x.s %0, v8" : "=r"(result));

        printf("Result = %u (PASSED, did not hang)\n", result);
    }

    return 0;
}
*/


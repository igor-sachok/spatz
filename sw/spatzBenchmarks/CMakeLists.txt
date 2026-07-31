# Copyright 2020 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

# Defines
set(SNRT_NFPU_PER_CORE "0" CACHE STRING "Number of FPUs per Spatz")
set(ELEN "64" CACHE STRING "Element length (32 or 64)")

# Allow spatzBenchmarks to be built as a standalone library.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
    set(CMAKE_TOOLCHAIN_FILE toolchain-gcc CACHE STRING "Toolchain to use")

    project(Benchmarks LANGUAGES C ASM)
    include(SnitchUtilities)

    # Build the runtime.
    add_subdirectory(../snRuntime snRuntime)
endif()

include_directories(include)
include_directories(${SNRUNTIME_INCLUDE_DIRS})

add_compile_options(-O3 -g -ffunction-sections)

# Use unrolled spatzBenchmarks
if (UNROLL)
add_definitions(-DUNROLL)
endif()

# Macro to regenerate the golden values and compile a module
macro(add_spatz_test_oneParam name file param1)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_twoParam name file param1 param2)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_threeParam name file param1 param2 param3)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}.h" SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Use data type as another parameter
macro(add_spatz_test_oneParam_type name file param1 type)
  set(target_name ${name}_M${param1})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${type}.h" PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_twoParam_type name file param1 param2 type)
  set(target_name ${name}_M${param1}_N${param2})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${type}.h" PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

macro(add_spatz_test_threeParam_type name file param1 param2 param3 type)
  set(target_name ${name}_M${param1}_N${param2}_K${param3})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC DATAHEADER="data/data_${param1}_${param2}_${param3}_${type}.h" PREC=${type} SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Parametrized SpMV variant: (format, N, P_W). Selects data header
# `data/data_spmv_<fmt>_N<N>_PW<PW>.h`. Target name suffixed with the shape.
macro(add_spatz_test_spmv name file fmt N P_W)
  set(target_name ${name}_${fmt}_N${N}_PW${P_W})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_spmv_${fmt}_N${N}_PW${P_W}.h"
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Parametrized SpMM variant: (format, M, N, P_W). Selects data header
# `data/data_spmm_<fmt>_M<M>_N<N>_PW<PW>.h`. Target name suffixed with the shape.
macro(add_spatz_test_spmm name file fmt M N P_W)
  set(target_name ${name}_${fmt}_M${M}_N${N}_PW${P_W})
  add_snitch_test(${target_name} ${file})
  target_link_libraries(test-${SNITCH_TEST_PREFIX}${target_name} benchmark ${SNITCH_RUNTIME})
  target_compile_definitions(test-${SNITCH_TEST_PREFIX}${target_name} PUBLIC
    DATAHEADER="data/data_spmm_${fmt}_M${M}_N${N}_PW${P_W}.h"
    SNRT_NFPU_PER_CORE=${SNRT_NFPU_PER_CORE})
endmacro()

# Benchmark library
add_library(benchmark benchmark/benchmark.c)

# Kernels

if (ELEN EQUAL 64)
  add_library(dp-fmatmul dp-fmatmul/kernel/dp-fmatmul.c)

  add_library(dp-faxpy dp-faxpy/kernel/faxpy.c)

  add_library(dp-fdotp dp-fdotp/kernel/fdotp.c)

  add_library(dp-fconv2d dp-fconv2d/kernel/fconv2d.c)

  add_library(dp-fft dp-fft/kernel/fft.c)
  add_library(srs-prod srs-prod/kernel/prod.c)
endif()

add_library(sp-fmatmul sp-fmatmul/kernel/sp-fmatmul.c)
add_library(hp-fmatmul hp-fmatmul/kernel/hp-fmatmul.c)

add_library(widening-hp-fmatmul widening-hp-fmatmul/kernel/widening-fmatmul.c)
add_library(widening-bp-fmatmul widening-bp-fmatmul/kernel/widening-fmatmul.c)

add_library(sdotp-hp-fmatmul sdotp-hp-fmatmul/kernel/sdotp-fmatmul.c)
add_library(sdotp-bp-fmatmul sdotp-bp-fmatmul/kernel/sdotp-fmatmul.c)

add_library(sp-fft sp-fft/kernel/fft.c)
add_library(srs-fft srs-fft/kernel/srs-fft.c)
add_library(srs-hard-decision srs-hard-decision/kernel/hard_decision.c)
add_library(srs-compute-soft-bits srs-compute-soft-bits/kernel/compute_soft_bits.c)
add_library(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/kernel/compute_var_to_check_msgs.c)
add_library(srs-compute-check-to-var-msgs srs-compute-check-to-var-msgs/kernel/compute_check_to_var_msgs.c)
add_library(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/kernel/analyze_var_to_check_msgs.c)
add_library(srs-scale-llr srs-scale-llr/kernel/scale_llr.c)

# Tests
enable_testing()
set(SNITCH_TEST_PREFIX spatzBenchmarks-)

if (ELEN EQUAL 64)
  add_spatz_test_threeParam(dp-fmatmul dp-fmatmul/main.c 64  64  64 )

  add_spatz_test_twoParam_type(dp-gemv gemv/main.c 64  128 64)
  add_spatz_test_threeParam_type(dp-sa-gemv sa-gemv/main.c 256 128 16 64)

  add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 256)
  add_spatz_test_oneParam(dp-faxpy dp-faxpy/main.c 1024)

  add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 128)
  add_spatz_test_oneParam(dp-fdotp dp-fdotp/main.c 4096)

  add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 32 32 7)
  add_spatz_test_threeParam(dp-fconv2d dp-fconv2d/main.c 64 64 7)

  add_spatz_test_twoParam(dp-fft dp-fft/main.c 128 2)
  add_spatz_test_oneParam(srs-prod srs-prod/main.c 128)
  add_spatz_test_oneParam(srs-prod srs-prod/main.c 8)
  add_spatz_test_oneParam(dotp-srs dotp-srs/main.c 2048)
  add_spatz_test_oneParam(dotp-srs dotp-srs/main.c 4096)
  add_spatz_test_oneParam(dotp-srs dotp-srs/main.c 128)
  add_spatz_test_oneParam(dotp-srs dotp-srs/main.c 8)
  add_spatz_test_oneParam(precoding precoding/main.c 128)
  add_spatz_test_oneParam(precoding precoding/main.c 4096)
  #add_spatz_test_oneParam(layer-map-precoding layer-map-precoding/main.c 128)

endif()

add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(sp-fmatmul sp-fmatmul/main.c 64  128 64 )

add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(hp-fmatmul hp-fmatmul/main.c 128 128 128)

add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(widening-hp-fmatmul widening-hp-fmatmul/main.c 128 128 128)

add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 128 128)
add_spatz_test_threeParam(widening-bp-fmatmul widening-bp-fmatmul/main.c 128 256 128)

add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(sdotp-hp-fmatmul sdotp-hp-fmatmul/main.c 128 128 128)

add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 64  64  64 )
add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 64  128 64 )
add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 128 128 128)
add_spatz_test_threeParam(sdotp-bp-fmatmul sdotp-bp-fmatmul/main.c 128 256 128)

add_spatz_test_twoParam_type(sp-gemv gemv/main.c 128 128 32)
add_spatz_test_twoParam_type(hp-gemv gemv/main.c 256 128 16)
add_spatz_test_threeParam_type(hp-sa-gemv sa-gemv/main.c 128 4096 512 16)

add_spatz_test_twoParam(sp-fft sp-fft/main.c 256 2)
add_spatz_test_twoParam(sp-fft sp-fft/main.c 512 2)

add_spatz_test_twoParam(srs-fft srs-fft/main.c 32 2)
add_spatz_test_twoParam(srs-fft srs-fft/main.c 256 2)
add_spatz_test_twoParam(srs-fft srs-fft/main.c 512 2)

add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 8 2)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 200 2)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 384 2)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 512 2)
add_spatz_test_twoParam(srs-hard-decision srs-hard-decision/main.c 1024 2)

add_spatz_test_twoParam(srs-compute-soft-bits srs-compute-soft-bits/main.c 384 2)
add_spatz_test_twoParam(srs-compute-soft-bits srs-compute-soft-bits/main.c 1024 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/main.c 384 2)
add_spatz_test_twoParam(srs-compute-var-to-check-msgs srs-compute-var-to-check-msgs/main.c 1024 2)
add_spatz_test_twoParam(srs-compute-check-to-var-msgs srs-compute-check-to-var-msgs/main.c 384 2)
add_spatz_test_twoParam(srs-compute-check-to-var-msgs srs-compute-check-to-var-msgs/main.c 1024 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 100 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 384 2)
add_spatz_test_twoParam(srs-analyze-var-to-check-msgs srs-analyze-var-to-check-msgs/main.c 1024 2)
add_spatz_test_twoParam(srs-scale-llr srs-scale-llr/main.c 384 2)
add_spatz_test_twoParam(srs-scale-llr srs-scale-llr/main.c 1024 2)

# Ventaglio sparse benchmarks. The kernels carry BOTH a Ventaglio (vfx)
# implementation and a baseline RVV reference compiled in via the
# USE_BASELINE macro; only the vfx variants are registered as tests here.
# To run the baseline locally for comparison, add a target that compiles
# main.c with -DUSE_BASELINE. Data headers live under data/ and are
# regenerated by the benchmark data-generation flow.
#
# These tests require -DVENTAGLIO on the RTL side (see hjson `ventaglio: true`).
if(SPATZ_CLUSTER_VENTAGLIO)
  add_spatz_test_spmv(sp-SpMV sp-SpMV/main.c 1to4 32 128)
  add_spatz_test_spmv(sp-SpMV sp-SpMV/main.c 2to4 32 128)
  add_spatz_test_spmm(sp-SpMM sp-SpMM/main.c 1to4 4 8 64)
  add_spatz_test_spmm(sp-SpMM sp-SpMM/main.c 2to4 4 8 64)
endif()

/**
 * @file cases_fnn2.c
 * @brief Bench cases for whole models as fnn_tflite.py converts them, and
 * CMSIS-NN running the same layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fnn_fused.h"
#include "fnn_quant.h"
#include "fx_arch.h"
#include "r2.h"
#include "r3.h"
#include "r5.h"
#ifdef BENCH_HAVE_CMSIS_NN
#include "arm_nnfunctions.h"
#endif

#define XOFF 64U /* each sample starts one byte further in */
#define xin ((int8_t *)(void *)bench_scratch)
#define yout ((int8_t *)(void *)(bench_scratch + 1024U))
#define work ((int8_t *)(void *)(bench_scratch + 1056U))
_Static_assert(R5_IN_LEN + XOFF <= 1024U && R2_IN_LEN + XOFF <= 1024U &&
                   R3_IN_LEN + XOFF <= 1024U && R5_OUT_LEN <= 32U &&
                   R2_OUT_LEN <= 32U && R3_OUT_LEN <= 32U,
               "inputs and outputs");
_Static_assert(1056U + R5_ARENA_SIZE <= BENCH_SCRATCH_SIZE &&
                   1056U + R2_ARENA_SIZE <= BENCH_SCRATCH_SIZE &&
                   1056U + R3_ARENA_SIZE <= BENCH_SCRATCH_SIZE &&
                   1056U + 6272U + 1568U <= BENCH_SCRATCH_SIZE,
               "the arenas and the CMSIS-NN tensors");

static int32_t bm;
static int8_t br;

static uint32_t rnd(uint32_t *s) {
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

#ifdef BENCH_HAVE_CMSIS_NN
static void c_setup(void);
#endif

static void setup(void) {
  static bool done;
  if (!bench_scratch_claim(&done) && done) {
    return;
  }
  done = true;
  uint32_t s = 0x2545F491U;
  for (unsigned i = 0U; i < 1024U; i++) {
    xin[i] = (int8_t)(rnd(&s) >> 24);
  }
  (void)fnn_quant_from_scale(1.0 / 2048.0, &bm, &br);
#ifdef BENCH_HAVE_CMSIS_NN
  c_setup();
#endif
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % XOFF;
}

#define MODEL(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, (uint32_t)(call),          \
                  h = bench_hash(h, yout, (bytes)), (void)0)

static uint32_t r2(uint32_t off) {
  r2_run(xin + off, yout, (uint8_t *)work);
  return 0U;
}
static uint32_t r5(uint32_t off) {
  r5_run(xin + off, yout, (uint8_t *)work);
  return 0U;
}
static uint32_t r3(uint32_t off) {
  r3_run(xin + off, yout, (uint8_t *)work);
  return 0U;
}
MODEL(b_r2, r2(x), R2_OUT_LEN)
MODEL(b_r5, r5(x), R5_OUT_LEN)
MODEL(b_r3, r3(x), R3_OUT_LEN)

#define W61                                                                    \
  2, 40, 2, -63, -50, 13, 45, 7, -82, 15, -32, 91, 42, -51, -14, -53, 12, -32, \
      120, 79, -21, -86, -102, 108, -112, -127, -112, -80, -62, -124, 40, 67,  \
      101, 59, 31, -126, 18, 55, -117, 101, 1, 63, -34, 96, -107, 16, -89,     \
      -33, -93, -105, 18, -81, 125, -83, 0, 57, -106, 27, -87, -27, 1
#define W244 W61, W61, W61, W61
#define W976 W244, W244, W244, W244
static const int8_t cw[61U * 132U] BENCH_TABLE FX_ALIGN(16) = {
    W976, W976, W976, W976, W976, W976, W976, W976, W244};
_Static_assert(sizeof cw >= 7840U, "the largest layer's weights");

#define BH 12U
#define BX (BH * 5U * 64U)
static FNN_DwConv bdw = {
    cw, NULL, {BH, 5U, 64U},  {BH, 5U, 64U},    3U, 3U, 1U, 1U, 1U,
    1U, -3,   {&bm, &br, 1U}, {-128, -128, 127}};
static FNN_Conv bpw = {
    cw + 640U, NULL, {BH, 5U, 64U},  {BH, 5U, 64U}, 1U, 1U, 1U, 1U, 0U, 0U,
    1U,        -128, {&bm, &br, 1U}, {5, -128, 127}};
_Static_assert(1056U + BX + FNN_DWPW_SCRATCH(3U, 5U, 64U, 64U) <=
                       BENCH_SCRATCH_SIZE &&
                   1056U + 2U * BX <= BENCH_SCRATCH_SIZE,
               "the block, fused and not");

/* Each sample starts from a fresh copy, since the block overwrites its input. */
static void block_in(uint32_t off) {
  const uint32_t *s = (const uint32_t *)(const void *)cw + (off & 60U);
  uint32_t *d = (uint32_t *)(void *)work;
  for (uint32_t i = 0U; i < BX / 4U; i++) {
    d[i] = s[i];
  }
}

static uint32_t fused(uint32_t off) {
  block_in(off);
  fnn_dwpw_s8(&bdw, &bpw, work, work, work + BX);
  return 0U;
}
static uint32_t apart(uint32_t off) {
  block_in(off);
  fnn_dwconv_s8(&bdw, work, work + BX);
  fnn_conv_s8(&bpw, work + BX, work, NULL);
  return 0U;
}
#define BLOCK(name, call)                                                      \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, (uint32_t)(call),          \
                  h = bench_hash(h, work, BX), (void)0)
BLOCK(b_dwpw, fused(x))
BLOCK(b_dw_pw, apart(x))

#ifdef BENCH_HAVE_CMSIS_NN
#define CH 32U
static int32_t bias[CH], mult[CH], shift[CH];
static int8_t cbuf[512];

static void c_setup(void) {
  uint32_t s = 0x9E3779B9U;
  for (unsigned c = 0U; c < CH; c++) {
    int8_t r;
    bias[c] = (int32_t)rnd(&s) >> 19;
    /* scales near 2^-11 keep most sums inside int8 */
    (void)fnn_quant_from_scale((1.0 + c / 32.0) / 2048.0, &mult[c], &r);
    shift[c] = -1 - r;
  }
}

/* Traps on any error: a wrong argument must not pass for a fast time. */
static void ok(arm_cmsis_nn_status st) {
  if (st != ARM_CMSIS_NN_SUCCESS) {
    __builtin_trap();
  }
}

static void c_conv(const int8_t *in, int32_t h, int32_t w, int32_t c,
                   int32_t oc, int32_t kh, int32_t kw, int32_t pad,
                   int8_t *out) {
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_conv_params p = {3,          -128,   {1, 1},
                                  {pad, pad}, {1, 1}, {-128, 127}};
  const cmsis_nn_per_channel_quant_params q = {mult, shift};
  const cmsis_nn_dims id = {1, h, w, c}, fd = {oc, kh, kw, c},
                      bd = {1, 1, 1, oc},
                      od = {1, h + 2 * pad - kh + 1, w + 2 * pad - kw + 1, oc};
  if (arm_convolve_wrapper_s8_get_buffer_size(&p, &id, &fd, &od) >
      (int32_t)sizeof cbuf) {
    __builtin_trap();
  }
  ok(arm_convolve_wrapper_s8(&ctx, &p, &q, &id, in, &fd, cw, &bd, bias, &od,
                             out));
}

static void c_pool(const int8_t *in, int32_t h, int32_t w, int32_t c,
                   int32_t kh, int32_t kw, int8_t *out) {
  const cmsis_nn_context ctx = {NULL, 0};
  const cmsis_nn_pool_params p = {{kh, kw}, {0, 0}, {-128, 127}};
  const cmsis_nn_dims id = {1, h, w, c}, fd = {1, kh, kw, 1},
                      od = {1, h / kh, w / kw, c};
  ok(arm_max_pool_s8(&ctx, &p, &id, in, &fd, &od, out));
}

static void c_fc(const int8_t *in, int32_t n_in, int32_t n_out, int8_t *out) {
  const cmsis_nn_context ctx = {NULL, 0};
  const cmsis_nn_fc_params p = {128, 0, 5, {-128, 127}};
  const cmsis_nn_per_tensor_quant_params q = {mult[0], shift[0]};
  const cmsis_nn_dims id = {1, 1, 1, n_in}, fd = {n_in, 1, 1, n_out},
                      bd = {1, 1, 1, n_out}, od = {1, 1, 1, n_out};
  ok(arm_fully_connected_s8(&ctx, &p, &q, &id, in, &fd, cw, &bd, bias, &od,
                            out));
}

static uint32_t c_r2(uint32_t off) {
  int8_t *a = work, *b = work + 1984;
  c_conv(xin + off, 1, 128, 3, 16, 1, 5, 0, a);
  c_pool(a, 1, 124, 16, 1, 2, b);
  c_conv(b, 1, 62, 16, 32, 1, 5, 0, a);
  c_pool(a, 1, 58, 32, 1, 2, b);
  c_conv(b, 1, 29, 32, 32, 1, 3, 0, a);
  /* CMSIS-NN has no MEAN: a global average pool takes its place. */
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_pool_params p = {{1, 1}, {0, 0}, {-128, 127}};
  const cmsis_nn_dims id = {1, 1, 27, 32}, fd = {1, 1, 27, 1},
                      od = {1, 1, 1, 32};
  if (arm_avgpool_s8_get_buffer_size(1, 32) > (int32_t)sizeof cbuf) {
    __builtin_trap();
  }
  ok(arm_avgpool_s8(&ctx, &p, &id, a, &fd, &od, b));
  c_fc(b, 32, 8, yout);
  return 0U;
}

static uint32_t c_r5(uint32_t off) {
  int8_t *a = work, *b = work + 6272;
  c_conv(xin + off, 28, 28, 1, 8, 3, 3, 1, a);
  c_pool(a, 28, 28, 8, 2, 2, b);
  c_conv(b, 14, 14, 8, 16, 3, 3, 1, a);
  c_pool(a, 14, 14, 16, 2, 2, b);
  c_fc(b, 784, 10, a);
  arm_softmax_s8(a, 1, 10, 1 << 30, 23, -248, yout);
  return 0U;
}
MODEL(c_model_r2, c_r2(x), 8U)
MODEL(c_model_r5, c_r5(x), 10U)
#endif

static const BenchCase cases[] = {
    {"model R2, IMU gesture, converted", "flexlib", b_r2, true},
    {"model R5, 28 x 28 CNN, converted", "flexlib", b_r5, true},
    {"model R3, keyword spotting DS-CNN, converted", "flexlib", b_r3, true},
    {"fnn_dwpw_s8, 3 x 3 then 1 x 1, 12 x 5 x 64, in place", "flexlib", b_dwpw,
     true},
    {"fnn_dwconv_s8 then fnn_conv_s8, 12 x 5 x 64", "flexlib", b_dw_pw, true},
#ifdef BENCH_HAVE_CMSIS_NN
    {"CMSIS-NN layers, model R2, a global average pool for its MEAN",
     "CMSIS-NN", c_model_r2, false},
    {"CMSIS-NN layers, model R5", "CMSIS-NN", c_model_r5, false},
#endif
};

const BenchGroup bench_group_fnn2 = {cases, BENCH_COUNT(cases)};

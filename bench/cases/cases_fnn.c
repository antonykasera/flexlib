/**
 * @file cases_fnn.c
 * @brief Bench cases for flexnn, and the CMSIS-NN kernels it is measured
 * against
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fnn_quant.h"
#ifdef BENCH_HAVE_CMSIS_NN
#include "arm_nnfunctions.h"
#include "arm_nnsupportfunctions.h"
#endif

#ifdef BENCH_HAVE_CMSIS_NN
/* the layers take their weights from one flash pool, a 61-byte pattern over
 * and over; because 61 is prime, no two rows come out the same */
#define W61                                                                    \
  2, 40, 2, -63, -50, 13, 45, 7, -82, 15, -32, 91, 42, -51, -14, -53, 12, -32, \
      120, 79, -21, -86, -102, 108, -112, -127, -112, -80, -62, -124, 40, 67,  \
      101, 59, 31, -126, 18, 55, -117, 101, 1, 63, -34, 96, -107, 16, -89,     \
      -33, -93, -105, 18, -81, 125, -83, 0, 57, -106, 27, -87, -27, 1
#define W244 W61, W61, W61, W61
#define W976 W244, W244, W244, W244
#define W3904 W976, W976, W976, W976
#define W15616 W3904, W3904, W3904, W3904
#define WPOOL_LEN (61U * 270U)
static const int8_t wpool[WPOOL_LEN] BENCH_TABLE = {W15616, W244, W244,
                                                    W244,   W61,  W61};
#endif

#define CH 64U     /* channels */
#define XLEN 4096U /* the largest input, the max pool's */
#define XOFF 64U   /* each sample starts one byte further in */
#define ZX (-3)    /* input zero point */
#define ZY 5       /* output zero point */

_Static_assert(BENCH_SCRATCH_SIZE >= 8192U, "inputs and outputs");
#define xin ((int8_t *)(void *)bench_scratch)
#define yout ((int8_t *)(void *)(bench_scratch + XLEN + XOFF))
#define YMAX (BENCH_SCRATCH_SIZE - XLEN - XOFF)
static int32_t acc[256 + 4], bias[CH];
static int32_t mult[CH], shift[CH]; /* CMSIS-NN takes TFLite's shifts */
static int8_t rsh[CH];

static uint32_t rnd(uint32_t *s) {
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

#ifdef BENCH_HAVE_CMSIS_NN
static void c_check(void);
#endif

static void setup(void) {
  static bool done;
  if (!bench_scratch_claim(&done) && done) {
    return;
  }
  done = true;
  uint32_t s = 0x9E3779B9U;
  for (unsigned i = 0U; i < XLEN + XOFF; i++) {
    xin[i] = (int8_t)(rnd(&s) >> 24);
  }
  for (unsigned i = 0U; i < 256U + 4U; i++) {
    acc[i] = (int32_t)rnd(&s) >> 12; /* about what a 256-term sum spans */
  }
  for (unsigned c = 0U; c < CH; c++) {
    bias[c] = (int32_t)rnd(&s) >> 19;
    /* scales near 2^-11 bring a 256-term sum into int8 range */
    (void)fnn_quant_from_scale((1.0 + c / 64.0) / 2048.0, &mult[c], &rsh[c]);
    shift[c] = -1 - rsh[c];
  }
#ifdef BENCH_HAVE_CMSIS_NN
  c_check();
#endif
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % XOFF;
}

#define OUT(bytes) h = bench_hash(h, yout, (bytes))
#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, (uint32_t)(call),          \
                  OUT(bytes), (void)0)

static const FNN_Quant q64 = {mult, rsh, CH};
static const FNN_Out out8 = {ZY, -128, 127};

static uint32_t requant(uint32_t off) {
  fnn_requant_s8(acc + off % 4U, yout, 256U, &q64, &out8);
  return 0U;
}
BLOCK(b_requant, requant(x), 256U)

#ifdef BENCH_HAVE_CMSIS_NN
/* CMSIS-NN as it ships, with gemmlowp's rounding */

static int8_t cbuf[1024];

static const cmsis_nn_fc_params fc_p = {-ZX, 0, ZY, {-128, 127}};
static const cmsis_nn_conv_params conv_p = {-ZX,    ZY,     {1, 1},
                                            {0, 0}, {1, 1}, {-128, 127}};
static const cmsis_nn_conv_params conv3_p = {-ZX,    ZY,     {1, 1},
                                             {1, 1}, {1, 1}, {-128, 127}};
static const cmsis_nn_dw_conv_params dw_p = {-ZX,    ZY,     1,          {1, 1},
                                             {1, 1}, {1, 1}, {-128, 127}};
static const cmsis_nn_pool_params mpool_p = {{2, 2}, {0, 0}, {-128, 127}};
static const cmsis_nn_pool_params apool_p = {{1, 1}, {0, 0}, {-128, 127}};

static cmsis_nn_per_channel_quant_params pc(void) {
  return (cmsis_nn_per_channel_quant_params){mult, shift};
}

/* the output step every CMSIS-NN s8 kernel ends with */
static uint32_t c_requant_loop(uint32_t off) {
  const int32_t *a = acc + off % 4U;
  for (unsigned i = 0U; i < 256U; i += CH) {
    for (unsigned c = 0U; c < CH; c++) {
      int32_t v = arm_nn_requantize(a[i + c], mult[c], shift[c]) + ZY;
      yout[i + c] = (int8_t)CLAMP(v, 127, -128);
    }
  }
  return 0U;
}
BLOCK(c_requant, c_requant_loop(x), 256U)

static uint32_t c_fc(uint32_t off, int32_t in, int32_t out) {
  const cmsis_nn_context ctx = {NULL, 0};
  const cmsis_nn_per_tensor_quant_params q = {mult[0], shift[0]};
  const cmsis_nn_dims id = {1, 1, 1, in}, fd = {in, 1, 1, out},
                      bd = {1, 1, 1, out}, od = {1, 1, 1, out};
  return (uint32_t)arm_fully_connected_s8(&ctx, &fc_p, &q, &id, xin + off, &fd,
                                          wpool, &bd, bias, &od, yout);
}
BLOCK(c_fc_256, c_fc(x, 256, 64), 64U)
BLOCK(c_fc_64, c_fc(x, 64, 12), 12U)

static uint32_t c_pw(uint32_t off) {
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_per_channel_quant_params q = pc();
  const cmsis_nn_dims id = {1, 5, 5, 64}, fd = {64, 1, 1, 64},
                      bd = {1, 1, 1, 64}, od = {1, 5, 5, 64};
  return (uint32_t)arm_convolve_1x1_s8_fast(&ctx, &conv_p, &q, &id, xin + off,
                                            &fd, wpool + 300, &bd, bias, &od,
                                            yout);
}
BLOCK(c_pw_64, c_pw(x), 1600U)

static uint32_t c_conv3(uint32_t off) {
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_per_channel_quant_params q = pc();
  const cmsis_nn_dims id = {1, 12, 12, 8}, fd = {16, 3, 3, 8},
                      bd = {1, 1, 1, 16}, od = {1, 12, 12, 16};
  return (uint32_t)arm_convolve_s8(&ctx, &conv3_p, &q, &id, xin + off, &fd,
                                   wpool + 5000, &bd, bias, NULL, &od, yout);
}
BLOCK(c_conv3_8, c_conv3(x), 2304U)

static uint32_t c_conv1d(uint32_t off) {
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_per_channel_quant_params q = pc();
  const cmsis_nn_dims id = {1, 1, 68, 16}, fd = {32, 1, 5, 16},
                      bd = {1, 1, 1, 32}, od = {1, 1, 64, 32};
  return (uint32_t)arm_convolve_1_x_n_s8(&ctx, &conv_p, &q, &id, xin + off, &fd,
                                         wpool + 7000, &bd, bias, &od, yout);
}
BLOCK(c_conv1d_16, c_conv1d(x), 2048U)

static uint32_t c_dw(uint32_t off, bool opt) {
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_per_channel_quant_params q = pc();
  const cmsis_nn_dims id = {1, 5, 5, 64}, fd = {1, 3, 3, 64},
                      bd = {1, 1, 1, 64}, od = {1, 5, 5, 64};
  return opt ? (uint32_t)arm_depthwise_conv_s8_opt(
                   &ctx, &dw_p, &q, &id, xin + off, &fd, wpool + 10000, &bd,
                   bias, &od, yout)
             : (uint32_t)arm_depthwise_conv_s8(&ctx, &dw_p, &q, &id, xin + off,
                                               &fd, wpool + 10000, &bd, bias,
                                               &od, yout);
}
BLOCK(c_dw_opt, c_dw(x, true), 1600U)
BLOCK(c_dw_plain, c_dw(x, false), 1600U)

static uint32_t c_maxpool(uint32_t off) {
  const cmsis_nn_context ctx = {NULL, 0};
  const cmsis_nn_dims id = {1, 16, 16, 16}, fd = {1, 2, 2, 1},
                      od = {1, 8, 8, 16};
  return (uint32_t)arm_max_pool_s8(&ctx, &mpool_p, &id, xin + off, &fd, &od,
                                   yout);
}
BLOCK(c_maxpool_16, c_maxpool(x), 1024U)

static uint32_t c_avgpool(uint32_t off) {
  const cmsis_nn_context ctx = {cbuf, sizeof cbuf};
  const cmsis_nn_dims id = {1, 5, 5, 64}, fd = {1, 5, 5, 1}, od = {1, 1, 1, 64};
  return (uint32_t)arm_avgpool_s8(&ctx, &apool_p, &id, xin + off, &fd, &od,
                                  yout);
}
BLOCK(c_avgpool_64, c_avgpool(x), 64U)

/* TFLite's ADD parameters for two inputs and an output at the same scale */
static uint32_t c_add(uint32_t off) {
  return (uint32_t)arm_elementwise_add_s8(xin + off, xin + 2048U + off, -ZX,
                                          1 << 30, 0, -ZX, 1 << 30, 0, 20, yout,
                                          ZY, 1 << 30, -18, -128, 127, 1024);
}
BLOCK(c_add_1024, c_add(x), 1024U)

/* TFLite's softmax parameters for an input scale of 1/16 */
static uint32_t c_softmax(uint32_t off) {
  arm_softmax_s8(xin + off, 1, 12, 1 << 30, 23, -248, yout);
  return 0U;
}
BLOCK(c_softmax_12, c_softmax(x), 12U)

/* every call runs once at setup and traps on an error: a wrong argument
 * must not pass for a fast time */
static void c_check(void) {
  const cmsis_nn_dims cv_i = {1, 12, 12, 8}, cv_f = {16, 3, 3, 8};
  const cmsis_nn_dims c1_i = {1, 1, 68, 16}, c1_f = {32, 1, 5, 16},
                      c1_o = {1, 1, 64, 32};
  const cmsis_nn_dims dw_i = {1, 5, 5, 64}, dw_f = {1, 3, 3, 64};
  const int32_t n = (int32_t)sizeof cbuf;
  bool bad =
      arm_convolve_s8_get_buffer_size(&cv_i, &cv_f) > n ||
      arm_convolve_1_x_n_s8_get_buffer_size(&conv_p, &c1_i, &c1_f, &c1_o) > n ||
      arm_depthwise_conv_s8_opt_get_buffer_size(&dw_i, &dw_f) > n ||
      arm_avgpool_s8_get_buffer_size(1, 64) > n;
  bad = bad || (c_fc(0U, 256, 64) | c_fc(0U, 64, 12) | c_pw(0U) | c_conv3(0U) |
                c_conv1d(0U) | c_dw(0U, true) | c_dw(0U, false) |
                c_maxpool(0U) | c_avgpool(0U) | c_add(0U)) != 0U;
  if (bad) {
    __builtin_trap();
  }
}

#endif

static const BenchCase cases[] = {
    {"fnn_requant_s8, 256 outputs, 64 channels", "flexlib", b_requant, true},
#ifdef BENCH_HAVE_CMSIS_NN
    {"arm_nn_requantize + clamp, 256 outputs, 64 channels", "CMSIS-NN",
     c_requant, false},
    {"arm_fully_connected_s8, 256 -> 64", "CMSIS-NN", c_fc_256, false},
    {"arm_fully_connected_s8, 64 -> 12", "CMSIS-NN", c_fc_64, false},
    {"arm_convolve_1x1_s8_fast, 64 -> 64, 5 x 5", "CMSIS-NN", c_pw_64, false},
    {"arm_convolve_s8, 3 x 3, 8 -> 16, 12 x 12", "CMSIS-NN", c_conv3_8, false},
    {"arm_convolve_1_x_n_s8, 5 taps, 16 -> 32, 64", "CMSIS-NN", c_conv1d_16,
     false},
    {"arm_depthwise_conv_s8_opt, 3 x 3, 64 x 5 x 5", "CMSIS-NN", c_dw_opt,
     false},
    {"arm_depthwise_conv_s8, 3 x 3, 64 x 5 x 5", "CMSIS-NN", c_dw_plain, false},
    {"arm_max_pool_s8, 2 x 2, 16 x 16 x 16", "CMSIS-NN", c_maxpool_16, false},
    {"arm_avgpool_s8, 5 x 5 x 64 to 1 x 1", "CMSIS-NN", c_avgpool_64, false},
    {"arm_elementwise_add_s8, 1024", "CMSIS-NN", c_add_1024, false},
    {"arm_softmax_s8, 12", "CMSIS-NN", c_softmax_12, false},
#endif
};

const BenchGroup bench_group_fnn = {cases, BENCH_COUNT(cases)};

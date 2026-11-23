/**
 * @file cmsis.c
 * @brief flexnn in FNN_ROUND_GEMMLOWP against CMSIS-NN compiled for the host,
 * layer by layer, bit for bit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arm_nnfunctions.h"
#include "fnn_conv.h"
#include "fnn_dense.h"
#include "fnn_dwconv.h"
#include "fnn_elem.h"
#include "fnn_pool.h"
#include "fnn_quant.h"
#include "fnn_softmax.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define XMAX 8192U
#define WMAX 20000U
#define CMAX 64U

static int8_t x[XMAX], w[WMAX], y[XMAX], want[XMAX];
static int8_t wp[WMAX] __attribute__((aligned(4)));
static int8_t scratch[4096];
static int16_t cbuf[16384];
static int32_t b[CMAX], bf[CMAX], mult[CMAX], shift[CMAX];
static int8_t rsh[CMAX];
static uint32_t rs = 0x9E3779B9U;

static uint32_t rnd(void) {
  rs ^= rs << 13;
  rs ^= rs >> 17;
  rs ^= rs << 5;
  return rs;
}

static int32_t range(int32_t lo, int32_t hi) {
  return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1));
}

/* Draws a random layer whose sums have k terms. */
static void layer(uint32_t k, uint32_t oc, uint32_t nq, int32_t *zx,
                  int32_t *zy, int32_t *lo) {
  for (uint32_t i = 0U; i < k * oc; i++) {
    w[i] = (int8_t)range(-127, 127);
  }
  for (uint32_t c = 0U; c < oc; c++) {
    b[c] = range(-20000, 20000);
  }
  for (uint32_t c = 0U; c < nq; c++) {
    double s = (0.5 + (double)(rnd() % 1000U) / 1000.0) / (64.0 * sqrt(k));
    /* A power of two makes exact ties, where the rounding modes part. */
    s = rnd() % 4U == 0U ? ldexp(1.0, -8 - (int)(rnd() % 5U)) : s;
    (void)fnn_quant_from_scale(s, &mult[c], &rsh[c]);
    shift[c] = -1 - rsh[c];
  }
  for (uint32_t c = nq; c < oc; c++) {
    mult[c] = mult[0];
    shift[c] = shift[0];
    rsh[c] = rsh[0];
  }
  *zx = range(-128, 127);
  *zy = range(-128, 127);
  *lo = rnd() % 3U == 0U ? *zy : -128;
}

static unsigned long dense_cases(unsigned long *outputs) {
  unsigned long bad = 0U;
  for (unsigned rep = 0U; rep < 400U; rep++) {
    uint32_t n = 1U + rnd() % 300U, m = 1U + rnd() % CMAX;
    uint32_t nq = rep % 2U ? m : 1U;
    int32_t zx, zy, lo;
    layer(n, m, nq, &zx, &zy, &lo);
    for (uint32_t i = 0U; i < n; i++) {
      x[i] = (int8_t)rnd();
    }
    const cmsis_nn_context ctx = {cbuf, (int32_t)sizeof cbuf};
    const cmsis_nn_fc_params p = {-zx, 0, zy, {lo, 127}};
    const cmsis_nn_dims id = {1, 1, 1, (int32_t)n},
                        fd = {(int32_t)n, 1, 1, (int32_t)m},
                        bd = {1, 1, 1, (int32_t)m}, od = {1, 1, 1, (int32_t)m};
    if (nq == 1U) {
      const cmsis_nn_per_tensor_quant_params q = {mult[0], shift[0]};
      (void)arm_fully_connected_s8(&ctx, &p, &q, &id, x, &fd, w, &bd, b, &od,
                                   want);
    } else {
      const cmsis_nn_per_channel_quant_params q = {mult, shift};
      (void)arm_fully_connected_per_channel_s8(&ctx, &p, &q, &id, x, &fd, w,
                                               &bd, b, &od, want);
    }
    fnn_fold_bias_s8(w, n, m, b, zx, bf);
    fnn_dense_pack_s8(w, n, m, wp);
    for (uint32_t packed = 0U; packed < 2U; packed++) {
      const FNN_Dense l = {packed ? wp : w, bf,           n, m, packed,
                           {mult, rsh, nq}, {zy, lo, 127}};
      fnn_dense_s8(&l, x, y);
      for (uint32_t o = 0U; o < m; o++) {
        bad += y[o] != want[o];
      }
      *outputs += m;
    }
  }
  return bad;
}

static unsigned long conv_cases(unsigned long *outputs) {
  unsigned long bad = 0U;
  for (unsigned rep = 0U; rep < 600U; rep++) {
    uint32_t ih = 1U + rnd() % 9U, iw = 1U + rnd() % 12U;
    uint32_t ic = 1U + rnd() % 16U, oc = 1U + rnd() % 24U;
    uint32_t kh = 1U + rnd() % 5U, kw = 1U + rnd() % 5U;
    uint32_t s = 1U + rnd() % 2U, same = rnd() % 2U;
    if (rep % 4U == 0U) {
      ih = 1U; /* CMSIS-NN takes its 1 x n path */
      kh = 1U;
    } else if (rep % 4U == 1U) {
      kh = kw = 1U; /* and here its 1 x 1 paths */
    }
    uint32_t oh, ow, pt = 0U, pl = 0U;
    if (same) {
      oh = (ih + s - 1U) / s;
      ow = (iw + s - 1U) / s;
      uint32_t nh = (oh - 1U) * s + kh, nw = (ow - 1U) * s + kw;
      pt = nh > ih ? (nh - ih) / 2U : 0U;
      pl = nw > iw ? (nw - iw) / 2U : 0U;
    } else {
      if (ih < kh || iw < kw) {
        continue;
      }
      oh = (ih - kh) / s + 1U;
      ow = (iw - kw) / s + 1U;
    }
    uint32_t k = kh * kw * ic;
    if (ih * iw * ic > XMAX || oh * ow * oc > XMAX || k * oc > WMAX) {
      continue;
    }
    int32_t zx, zy, lo;
    layer(k, oc, oc, &zx, &zy, &lo);
    for (uint32_t i = 0U; i < ih * iw * ic; i++) {
      x[i] = (int8_t)rnd();
    }
    const cmsis_nn_conv_params p = {
        -zx,    zy,       {(int32_t)s, (int32_t)s}, {(int32_t)pl, (int32_t)pt},
        {1, 1}, {lo, 127}};
    const cmsis_nn_dims id = {1, (int32_t)ih, (int32_t)iw, (int32_t)ic},
                        fd = {(int32_t)oc, (int32_t)kh, (int32_t)kw,
                              (int32_t)ic},
                        bd = {1, 1, 1, (int32_t)oc},
                        od = {1, (int32_t)oh, (int32_t)ow, (int32_t)oc};
    int32_t need = arm_convolve_wrapper_s8_get_buffer_size(&p, &id, &fd, &od);
    if (need > (int32_t)sizeof cbuf) {
      continue;
    }
    const cmsis_nn_context ctx = {cbuf, (int32_t)sizeof cbuf};
    const cmsis_nn_per_channel_quant_params q = {mult, shift};
    if (arm_convolve_wrapper_s8(&ctx, &p, &q, &id, x, &fd, w, &bd, b, &od,
                                want) != ARM_CMSIS_NN_SUCCESS) {
      continue;
    }
    fnn_fold_bias_s8(w, k, oc, b, zx, bf);
    fnn_dense_pack_s8(w, k, oc, wp);
    for (uint32_t packed = 0U; packed < 2U; packed++) {
      const FNN_Conv l = {packed ? wp : w,
                          bf,
                          {ih, iw, ic},
                          {oh, ow, oc},
                          kh,
                          kw,
                          s,
                          s,
                          pt,
                          pl,
                          packed,
                          zx,
                          {mult, rsh, oc},
                          {zy, lo, 127}};
      if (fnn_conv_check(&l) != FNN_OK) {
        bad++;
        continue;
      }
      fnn_conv_s8(&l, x, y, scratch);
      for (uint32_t i = 0U; i < oh * ow * oc; i++) {
        bad += y[i] != want[i];
      }
      *outputs += oh * ow * oc;
    }
  }
  return bad;
}

static unsigned long dwconv_cases(unsigned long *outputs) {
  unsigned long bad = 0U;
  for (unsigned rep = 0U; rep < 600U; rep++) {
    uint32_t ih = 1U + rnd() % 9U, iw = 1U + rnd() % 12U;
    uint32_t ic = 1U + rnd() % 24U, m = rep % 4U == 0U ? 1U + rnd() % 3U : 1U;
    uint32_t kh = rep % 3U == 0U ? 3U : 1U + rnd() % 5U;
    uint32_t kw = rep % 3U == 0U ? 3U : 1U + rnd() % 5U;
    uint32_t s = 1U + rnd() % 2U, same = rnd() % 2U, oc = ic * m;
    uint32_t oh, ow, pt = 0U, pl = 0U;
    if (same) {
      oh = (ih + s - 1U) / s;
      ow = (iw + s - 1U) / s;
      uint32_t nh = (oh - 1U) * s + kh, nw = (ow - 1U) * s + kw;
      pt = nh > ih ? (nh - ih) / 2U : 0U;
      pl = nw > iw ? (nw - iw) / 2U : 0U;
    } else {
      if (ih < kh || iw < kw) {
        continue;
      }
      oh = (ih - kh) / s + 1U;
      ow = (iw - kw) / s + 1U;
    }
    if (ih * iw * ic > XMAX || oh * ow * oc > XMAX || oc > CMAX) {
      continue;
    }
    int32_t zx, zy, lo;
    layer(kh * kw, oc, oc, &zx, &zy, &lo);
    for (uint32_t i = 0U; i < ih * iw * ic; i++) {
      x[i] = (int8_t)rnd();
    }
    const cmsis_nn_dw_conv_params p = {-zx,
                                       zy,
                                       (int32_t)m,
                                       {(int32_t)s, (int32_t)s},
                                       {(int32_t)pl, (int32_t)pt},
                                       {1, 1},
                                       {lo, 127}};
    const cmsis_nn_dims id = {1, (int32_t)ih, (int32_t)iw, (int32_t)ic},
                        fd = {1, (int32_t)kh, (int32_t)kw, (int32_t)oc},
                        bd = {1, 1, 1, (int32_t)oc},
                        od = {1, (int32_t)oh, (int32_t)ow, (int32_t)oc};
    int32_t need =
        arm_depthwise_conv_wrapper_s8_get_buffer_size(&p, &id, &fd, &od);
    if (need > (int32_t)sizeof cbuf) {
      continue;
    }
    const cmsis_nn_context ctx = {cbuf, (int32_t)sizeof cbuf};
    const cmsis_nn_per_channel_quant_params q = {mult, shift};
    if (arm_depthwise_conv_wrapper_s8(&ctx, &p, &q, &id, x, &fd, w, &bd, b, &od,
                                      want) != ARM_CMSIS_NN_SUCCESS) {
      continue;
    }
    fnn_dwconv_fold_bias_s8(w, kh * kw, oc, b, zx, bf);
    const FNN_DwConv l = {
        w,  bf, {ih, iw, ic}, {oh, ow, oc},    kh,           kw, s, s,
        pt, pl, zx,           {mult, rsh, oc}, {zy, lo, 127}};
    if (fnn_dwconv_check(&l) != FNN_OK) {
      bad++;
      continue;
    }
    fnn_dwconv_s8(&l, x, y);
    for (uint32_t i = 0U; i < oh * ow * oc; i++) {
      bad += y[i] != want[i];
    }
    *outputs += oh * ow * oc;
  }
  return bad;
}

/* Compares max pools, or average pools when avg is set. */
static unsigned long pool_cases(int avg, unsigned long *outputs) {
  unsigned long bad = 0U;
  for (unsigned rep = 0U; rep < 600U; rep++) {
    uint32_t ih = 1U + rnd() % 12U, iw = 1U + rnd() % 12U;
    uint32_t c = 1U + rnd() % 24U, kh = 1U + rnd() % 5U, kw = 1U + rnd() % 5U;
    uint32_t s = 1U + rnd() % 3U, same = rnd() % 2U, oh, ow, pt = 0U, pl = 0U;
    if (same) {
      oh = (ih + s - 1U) / s;
      ow = (iw + s - 1U) / s;
      uint32_t nh = (oh - 1U) * s + kh, nw = (ow - 1U) * s + kw;
      pt = nh > ih ? (nh - ih) / 2U : 0U;
      pl = nw > iw ? (nw - iw) / 2U : 0U;
    } else {
      if (ih < kh || iw < kw) {
        continue;
      }
      oh = (ih - kh) / s + 1U;
      ow = (iw - kw) / s + 1U;
    }
    if (ih * iw * c > XMAX || oh * ow * c > XMAX) {
      continue;
    }
    for (uint32_t i = 0U; i < ih * iw * c; i++) {
      x[i] = (int8_t)rnd();
    }
    int32_t lo = rnd() % 3U == 0U ? range(-128, 0) : -128;
    int32_t hi = rnd() % 3U == 0U ? range(lo, 127) : 127;
    const cmsis_nn_pool_params p = {{(int32_t)s, (int32_t)s},
                                    {(int32_t)pl, (int32_t)pt},
                                    {lo, hi}};
    const cmsis_nn_dims id = {1, (int32_t)ih, (int32_t)iw, (int32_t)c},
                        fd = {1, (int32_t)kh, (int32_t)kw, 1},
                        od = {1, (int32_t)oh, (int32_t)ow, (int32_t)c};
    const cmsis_nn_context ctx = {cbuf, (int32_t)sizeof cbuf};
    arm_cmsis_nn_status st =
        avg ? arm_avgpool_s8(&ctx, &p, &id, x, &fd, &od, want)
            : arm_max_pool_s8(&ctx, &p, &id, x, &fd, &od, want);
    if (st != ARM_CMSIS_NN_SUCCESS) {
      continue;
    }
    const FNN_Pool l = {{ih, iw, c}, {oh, ow, c}, kh, kw, s, s, pt, pl, lo, hi};
    if (fnn_pool_check(&l) != FNN_OK) {
      bad++;
      continue;
    }
    if (avg) {
      fnn_avgpool_s8(&l, x, y);
    } else {
      fnn_maxpool_s8(&l, x, y);
    }
    for (uint32_t i = 0U; i < oh * ow * c; i++) {
      bad += y[i] != want[i];
    }
    *outputs += oh * ow * c;
  }
  return bad;
}

/* Compares adds, with the multipliers from fnn_add_init(). */
static unsigned long add_cases(unsigned long *outputs) {
  unsigned long bad = 0U;
  for (unsigned rep = 0U; rep < 1000U; rep++) {
    double s1 = 0.001 + (double)(rnd() % 100000U) / 100000.0;
    double s2 = rnd() % 4U == 0U ? s1 : 0.001 + (double)(rnd() % 100000U) / 100000.0;
    double sy = (s1 + s2) * (0.3 + (double)(rnd() % 3000U) / 1000.0);
    sy = rnd() % 4U == 0U ? 2.0 * s1 : sy;
    int32_t z1 = range(-128, 127), z2 = range(-128, 127), zy = range(-128, 127);
    int32_t lo = rnd() % 3U == 0U ? zy : -128;
    uint32_t n = 1U + rnd() % 4096U;
    for (uint32_t i = 0U; i < n; i++) {
      x[i] = (int8_t)rnd();
      w[i] = (int8_t)rnd();
    }
    FNN_Add a;
    if (fnn_add_init(&a, s1, z1, s2, z2, sy, (FNN_Out){zy, lo, 127}) !=
        FNN_OK) {
      bad++;
      continue;
    }
    if (arm_elementwise_add_s8(x, w, -z1, a.m[0], -a.sh[0], -z2, a.m[1],
                               -a.sh[1], 20, want, zy, a.m[2], -a.sh[2], lo,
                               127, (int32_t)n) != ARM_CMSIS_NN_SUCCESS) {
      continue;
    }
    fnn_add_s8(&a, x, w, y, n);
    for (uint32_t i = 0U; i < n; i++) {
      bad += y[i] != want[i];
    }
    *outputs += n;
  }
  return bad;
}

/* Compares softmax at an input scale of 1/16, with TFLite's parameters. */
static unsigned long softmax_cases(unsigned long *outputs) {
  static uint32_t tab[256];
  (void)fnn_softmax_s8_build(1.0, 1.0 / 16.0, tab);
  unsigned long differ = 0U;
  for (unsigned rep = 0U; rep < 2000U; rep++) {
    uint32_t n = 2U + rnd() % 30U;
    for (uint32_t i = 0U; i < n; i++) {
      x[i] = (int8_t)rnd();
    }
    arm_softmax_s8(x, 1, (int32_t)n, 1 << 30, 23, -248, want);
    fnn_softmax_s8(x, y, n, tab);
    for (uint32_t i = 0U; i < n; i++) {
      differ += y[i] != want[i];
    }
    *outputs += n;
  }
  return differ;
}

int main(void) {
  unsigned long nd = 0U, nc = 0U;
  unsigned long bd = dense_cases(&nd), bc = conv_cases(&nc);
  printf("fully connected: %lu of %lu outputs differ from CMSIS-NN\n", bd, nd);
  printf("convolution:     %lu of %lu outputs differ from CMSIS-NN\n", bc, nc);
  unsigned long nw = 0U, bw = dwconv_cases(&nw);
  printf("depthwise:       %lu of %lu outputs differ from CMSIS-NN\n", bw, nw);
  unsigned long ns = 0U, ds = softmax_cases(&ns);
  printf("softmax:         %lu of %lu outputs differ from CMSIS-NN\n", ds, ns);
  unsigned long nm = 0U, bm = pool_cases(0, &nm);
  printf("max pool:        %lu of %lu outputs differ from CMSIS-NN\n", bm, nm);
  unsigned long na = 0U, ba = pool_cases(1, &na);
  printf("average pool:    %lu of %lu outputs differ from CMSIS-NN\n", ba, na);
  unsigned long ne = 0U, be = add_cases(&ne);
  printf("add:             %lu of %lu outputs differ from CMSIS-NN\n", be, ne);
  return bd == 0U && bc == 0U && bw == 0U && ds == 0U && bm == 0U &&
                 ba == 0U && be == 0U
             ? 0
             : 1;
}

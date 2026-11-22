/**
 * @file cases_fnn.c
 * @brief Bench cases for flexnn, and the CMSIS-NN kernels it is measured
 * against
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fnn_conv.h"
#include "fnn_convert.h"
#include "fnn_dense.h"
#include "fnn_dwconv.h"
#include "fnn_elem.h"
#include "fnn_lut.h"
#include "fnn_pool.h"
#include "fnn_quant.h"
#include "fnn_shape.h"
#include "fnn_softmax.h"
#include <math.h>
#ifdef BENCH_HAVE_CMSIS_NN
#include "arm_nnfunctions.h"
#include "arm_nnsupportfunctions.h"
#endif

/* One flash pool of a 61-byte pattern; 61 is prime, and no two rows match. */
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
static const int8_t wpool[WPOOL_LEN] BENCH_TABLE FX_ALIGN(16) = {
    W15616, W244, W244, W244, W61, W61};

#define CH 64U     /* channels */
#define XLEN 4096U /* the largest input, the max pool's */
#define XOFF 64U   /* each sample starts one byte further in */
#define ZX (-3)    /* input zero point */
#define ZY 5       /* output zero point */

_Static_assert(BENCH_SCRATCH_SIZE >= 8192U, "inputs and outputs");
#define xin ((int8_t *)(void *)bench_scratch)
#define yout ((int8_t *)(void *)(bench_scratch + XLEN + XOFF))
#define YMAX (BENCH_SCRATCH_SIZE - XLEN - XOFF)
/* The softmax table, past the largest output (2304 bytes). */
#define etab ((uint32_t *)(void *)(bench_scratch + 7168U))
_Static_assert(XLEN + XOFF + 2304U <= 7168U, "outputs clear of the table");
/* Float inputs sit between the largest output and the table. */
#define fin ((float *)(void *)(bench_scratch + 6464U))
#define FN 128U
_Static_assert(XLEN + XOFF + 2304U <= 6464U && 6464U + 4U * (FN + 16U) <= 7168U,
               "float inputs clear of the outputs and the table");
/* The add layer, between the patch scratch and the table. */
#define add_eq (*(FNN_Add *)(void *)(bench_scratch + 7112U))
_Static_assert(7112U + sizeof(FNN_Add) <= 7168U, "add layer clear of the table");
static int32_t acc[256 + 4], bias[CH], bf256[CH], bf128[CH], bf64[12];
static int32_t mult[CH], shift[CH]; /* CMSIS-NN takes TFLite's shifts */
static int8_t rsh[CH];
static int8_t lut[256];
static FNN_Mean mean64 = {{5U, 5U, 64U}, ZX, 0, 0, {ZY, -128, 127}};

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
  for (unsigned i = 0U; i < 256U; i++) {
    lut[i] = (int8_t)rnd(&s);
  }
  /* About exp(-d / 16), in integers: the host and the board agree. */
  for (unsigned i = 0U; i < FN + 16U; i++) {
    fin[i] = (float)(int32_t)rnd(&s) * 0x1p-31f * 6.0f;
  }
  etab[0] = 1U << 31;
  for (unsigned d = 1U; d < 256U; d++) {
    etab[d] = etab[d - 1U] - etab[d - 1U] / 16U;
  }
  fnn_fold_bias_s8(wpool, 256U, 64U, bias, ZX, bf256);
  fnn_fold_bias_s8(wpool, 128U, 64U, bias, ZX, bf128);
  fnn_fold_bias_s8(wpool, 64U, 12U, bias, ZX, bf64);
  (void)fnn_mean_quant(1.0, 25U, &mean64.mult, &mean64.rsh);
  (void)fnn_add_init(&add_eq, 0.05, ZX, 0.05, ZX, 0.05,
                     (FNN_Out){ZY, -128, 127});
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

static const FNN_Dense fc256 = {wpool, bf256,           256U,           64U,
                                0U,    {mult, rsh, 1U}, {ZY, -128, 127}};
static const FNN_Dense fc256p = {wpool, bf256,           256U,           64U,
                                 1U,    {mult, rsh, 1U}, {ZY, -128, 127}};
static const FNN_Dense fc128 = {wpool, bf128,           128U,           64U,
                                0U,    {mult, rsh, 1U}, {ZY, -128, 127}};
static const FNN_Dense fc128p = {wpool, bf128,           128U,           64U,
                                 1U,    {mult, rsh, 1U}, {ZY, -128, 127}};
static const FNN_Dense fc64 = {wpool,           bf64,           64U, 12U, 0U,
                               {mult, rsh, 1U}, {ZY, -128, 127}};
static const FNN_Dense fc64p = {wpool,           bf64,           64U, 12U, 1U,
                                {mult, rsh, 1U}, {ZY, -128, 127}};

static uint32_t dense(uint32_t off, const FNN_Dense *l) {
  fnn_dense_s8(l, xin + off, yout);
  return 0U;
}
BLOCK(b_fc128, dense(x, &fc128), 64U)
BLOCK(b_fc128p, dense(x, &fc128p), 64U)
BLOCK(b_fc64, dense(x, &fc64), 12U)
BLOCK(b_fc64p, dense(x, &fc64p), 12U)

/* 16 KB of weights fill the flash cache: these run from a flushed one. */
#define BLOCK_COLD(name, call, bytes)                                          \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, (uint32_t)(call),          \
                  OUT(bytes), bench_cold_flush())
BLOCK_COLD(b_fc256_cold, dense(x, &fc256), 64U)
BLOCK_COLD(b_fc256p_cold, dense(x, &fc256p), 64U)

/* These are the CMSIS-NN rows' layers, with weights at the same offsets. */
#define CONV(name, ih, iw, ic, oh, ow, oc, k_h, k_w, p_t, p_l, woff, pk)       \
  static const FNN_Conv name = {                                              \
      wpool + (woff), bias, {ih, iw, ic}, {oh, ow, oc},    k_h,                \
      k_w,            1U,   1U,           p_t,             p_l,                \
      pk,             ZX,   {mult, rsh, oc}, {ZY, -128, 127}}
CONV(pw64, 5U, 5U, 64U, 5U, 5U, 64U, 1U, 1U, 0U, 0U, 300U, 0U);
CONV(pw64p, 5U, 5U, 64U, 5U, 5U, 64U, 1U, 1U, 0U, 0U, 300U, 1U);
CONV(cv3, 12U, 12U, 8U, 12U, 12U, 16U, 3U, 3U, 1U, 1U, 5000U, 0U);
CONV(cv3p, 12U, 12U, 8U, 12U, 12U, 16U, 3U, 3U, 1U, 1U, 5000U, 1U);
CONV(cv1d, 1U, 68U, 16U, 1U, 64U, 32U, 1U, 5U, 0U, 0U, 7000U, 0U);
CONV(cv1dp, 1U, 68U, 16U, 1U, 64U, 32U, 1U, 5U, 0U, 0U, 7000U, 1U);
/* The patch sits between the float inputs and the table. */
#define cscr ((int8_t *)(void *)(bench_scratch + 7040U))
_Static_assert(6464U + 4U * (FN + 16U) <= 7040U &&
                   7040U + FNN_CONV_SCRATCH(3U, 3U, 8U) <= 7168U,
               "patch clear of the float inputs and the table");

static uint32_t conv(uint32_t off, const FNN_Conv *l) {
  fnn_conv_s8(l, xin + off, yout, cscr);
  return 0U;
}
BLOCK(b_pw64, conv(x, &pw64), 1600U)
BLOCK(b_pw64p, conv(x, &pw64p), 1600U)
BLOCK(b_cv3, conv(x, &cv3), 2304U)
BLOCK(b_cv3p, conv(x, &cv3p), 2304U)
BLOCK(b_cv1d, conv(x, &cv1d), 2048U)
BLOCK(b_cv1dp, conv(x, &cv1dp), 2048U)

static const FNN_DwConv dw64 = {
    wpool + 10000U, bias, {5U, 5U, 64U},   {5U, 5U, 64U}, 3U, 3U, 1U,
    1U,             1U,   1U,              ZX,            {mult, rsh, 64U},
    {ZY, -128, 127}};
static uint32_t dwconv(uint32_t off) {
  fnn_dwconv_s8(&dw64, xin + off, yout);
  return 0U;
}
BLOCK(b_dw64, dwconv(x), 1600U)
static const FNN_DwConv dw64v = {
    wpool + 10000U, bias, {7U, 7U, 64U},   {5U, 5U, 64U}, 3U, 3U, 1U,
    1U,             0U,   0U,              ZX,            {mult, rsh, 64U},
    {ZY, -128, 127}};
static uint32_t dwconv_valid(uint32_t off) {
  fnn_dwconv_s8(&dw64v, xin + off, yout);
  return 0U;
}
BLOCK(b_dw64v, dwconv_valid(x), 1600U)

static const FNN_Pool mp16 = {{16U, 16U, 16U}, {8U, 8U, 16U}, 2U, 2U, 2U, 2U,
                              0U,              0U,            -128, 127};
static const FNN_Pool ap64 = {{5U, 5U, 64U}, {1U, 1U, 64U}, 5U, 5U, 1U, 1U,
                              0U,            0U,            -128, 127};
static uint32_t pool(uint32_t off, const FNN_Pool *p, int avg) {
  if (avg) {
    fnn_avgpool_s8(p, xin + off, yout);
  } else {
    fnn_maxpool_s8(p, xin + off, yout);
  }
  return 0U;
}
static uint32_t mean(uint32_t off) {
  fnn_mean_s8(&mean64, xin + off, yout);
  return 0U;
}
BLOCK(b_maxpool16, pool(x, &mp16, 0), 1024U)
BLOCK(b_avgpool64, pool(x, &ap64, 1), 64U)
BLOCK(b_mean64, mean(x), 64U)

static uint32_t add(uint32_t off) {
  fnn_add_s8(&add_eq, xin + off, xin + 2048U + off, yout, 1024U);
  return 0U;
}
BLOCK(b_add1024, add(x), 1024U)

static inline int32_t mbq(int32_t x, int32_t m, uint32_t e) {
  int32_t v = (int32_t)(fx_smlal(1LL << 30, x, m) >> 31);
  uint32_t mask = (1U << e) - 1U;
  uint32_t thr = (mask >> 1) + (uint32_t)(v < 0);
  return (v >> e) + (int32_t)(((uint32_t)v & mask) > thr);
}

BENCH_CALL static void k_add_tfl(const FNN_Add *a, const int8_t *x1,
                                 const int8_t *x2, int8_t *y, uint32_t n) {
  int32_t z1 = a->z1, z2 = a->z2, m1 = a->m[0], m2 = a->m[1], mo = a->m[2];
  uint32_t e1 = (uint32_t)a->sh[0], e2 = (uint32_t)a->sh[1];
  uint32_t eo = (uint32_t)a->sh[2];
  int32_t zp = a->o.zp, lo = a->o.lo, hi = a->o.hi;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v1 = mbq((x1[i] - z1) * (1 << 20), m1, e1);
    int32_t v2 = mbq((x2[i] - z2) * (1 << 20), m2, e2);
    y[i] = fnn_clamp_s8(mbq(v1 + v2, mo, eo), zp, lo, hi);
  }
}
/* TFLite's multipliers and shifts for these scales, as c_add passes them. */
static const FNN_Add add_tfl = {
    ZX, ZX, {1 << 30, 1 << 30, 1 << 30}, {0, 0, 18}, {ZY, -128, 127}};
static uint32_t add_tfl3(uint32_t off) {
  k_add_tfl(&add_tfl, xin + off, xin + 2048U + off, yout, 1024U);
  return 0U;
}
BLOCK(b_add1024_tfl, add_tfl3(x), 1024U)

static uint32_t concat(uint32_t off) {
  static const uint32_t len[2] = {16U, 16U};
  const int8_t *const in[2] = {xin + off, xin + 1024U + off};
  fnn_concat_s8(in, len, 2U, 64U, yout);
  return 0U;
}
BLOCK(b_concat, concat(x), 2048U)

/* The library's window, for the candidates below. */
static const int8_t *win(const FNN_Pool *p, const int8_t *x, uint32_t oy,
                         uint32_t ox, uint32_t *nky, uint32_t *nkx) {
  int32_t iy = (int32_t)(oy * p->sh) - (int32_t)p->pt;
  int32_t ix = (int32_t)(ox * p->sw) - (int32_t)p->pl;
  int32_t ky0 = -iy > 0 ? -iy : 0, kx0 = -ix > 0 ? -ix : 0;
  int32_t ky1 = (int32_t)p->in.h - iy, kx1 = (int32_t)p->in.w - ix;
  ky1 = ky1 < (int32_t)p->kh ? ky1 : (int32_t)p->kh;
  kx1 = kx1 < (int32_t)p->kw ? kx1 : (int32_t)p->kw;
  *nky = (uint32_t)(ky1 - ky0);
  *nkx = (uint32_t)(kx1 - kx0);
  return x + ((uint32_t)(iy + ky0) * p->in.w + (uint32_t)(ix + kx0)) * p->in.c;
}

BENCH_CALL static void k_maxpool4(const FNN_Pool *p, const int8_t *x,
                                  int8_t *y) {
  uint32_t c = p->in.c, row = p->in.w * c;
  for (uint32_t oy = 0U; oy < p->out.h; oy++) {
    for (uint32_t ox = 0U; ox < p->out.w; ox++, y += c) {
      uint32_t nky, nkx;
      const int8_t *w = win(p, x, oy, ox, &nky, &nkx);
      for (uint32_t j = 0U; j + 4U <= c; j += 4U) {
        const int8_t *r = w + j;
        int32_t m0 = -128, m1 = -128, m2 = -128, m3 = -128;
        for (uint32_t ky = 0U; ky < nky; ky++, r += row) {
          const int8_t *q = r;
          for (uint32_t kx = 0U; kx < nkx; kx++, q += c) {
            m0 = q[0] > m0 ? q[0] : m0;
            m1 = q[1] > m1 ? q[1] : m1;
            m2 = q[2] > m2 ? q[2] : m2;
            m3 = q[3] > m3 ? q[3] : m3;
          }
        }
        y[j] = (int8_t)(m0 < p->lo ? p->lo : (m0 > p->hi ? p->hi : m0));
        y[j + 1U] = (int8_t)(m1 < p->lo ? p->lo : (m1 > p->hi ? p->hi : m1));
        y[j + 2U] = (int8_t)(m2 < p->lo ? p->lo : (m2 > p->hi ? p->hi : m2));
        y[j + 3U] = (int8_t)(m3 < p->lo ? p->lo : (m3 > p->hi ? p->hi : m3));
      }
    }
  }
}

static int32_t avg_away(int32_t s, int32_t n) {
  return (s > 0 ? s + n / 2 : s - n / 2) / n;
}

BENCH_CALL static void k_avgpool_lanes(const FNN_Pool *p, const int8_t *x,
                                       int8_t *y) {
  uint32_t c = p->in.c, row = p->in.w * c;
  for (uint32_t oy = 0U; oy < p->out.h; oy++) {
    for (uint32_t ox = 0U; ox < p->out.w; ox++, y += c) {
      uint32_t nky, nkx;
      const int8_t *w = win(p, x, oy, ox, &nky, &nkx);
      uint32_t n = nky * nkx;
      for (uint32_t j = 0U; j + 4U <= c; j += 4U) {
        const int8_t *r = w + j;
        uint32_t e = 0U, o = 0U;
        for (uint32_t ky = 0U; ky < nky; ky++, r += row) {
          const int8_t *q = r;
          for (uint32_t kx = 0U; kx < nkx; kx++, q += c) {
            uint32_t v;
            memcpy(&v, q, 4U);
            v ^= 0x80808080U;
            e += v & 0x00FF00FFU;
            o += (v >> 8) & 0x00FF00FFU;
          }
        }
        uint32_t s[4] = {e & 0xFFFFU, o & 0xFFFFU, e >> 16, o >> 16};
        for (uint32_t i = 0U; i < 4U; i++) {
          int32_t v = avg_away((int32_t)(s[i] - 128U * n), (int32_t)n);
          y[j + i] =
              (int8_t)(v < p->lo ? p->lo : (v > p->hi ? p->hi : v));
        }
      }
    }
  }
}

static uint32_t pool_with(uint32_t off, const FNN_Pool *p,
                          void (*k)(const FNN_Pool *, const int8_t *,
                                    int8_t *)) {
  k(p, xin + off, yout);
  return 0U;
}
BLOCK(b_maxpool4, pool_with(x, &mp16, k_maxpool4), 1024U)
BLOCK(b_avgpool_lanes, pool_with(x, &ap64, k_avgpool_lanes), 64U)

BENCH_CALL static void k_px2(const int8_t *xa, const int8_t *xb,
                             const int8_t *w, uint32_t half, uint32_t n,
                             uint32_t *sa, uint32_t *sb) {
  const uint32_t *q = (const uint32_t *)(const void *)w + half;
  uint32_t a0 = sa[0], a1 = sa[1], a2 = sa[2], a3 = sa[3];
  uint32_t b0 = sb[0], b1 = sb[1], b2 = sb[2], b3 = sb[3];
  for (uint32_t i = 0U; i < n; i++) {
    int32_t u = xa[i], v = xb[i];
    uint32_t r = q[2U * i];
    int32_t w0 = (int8_t)r, w1 = (int8_t)(r >> 8), w2 = (int8_t)(r >> 16);
    int32_t w3 = (int32_t)r >> 24;
    a0 += (uint32_t)(u * w0);
    b0 += (uint32_t)(v * w0);
    a1 += (uint32_t)(u * w1);
    b1 += (uint32_t)(v * w1);
    a2 += (uint32_t)(u * w2);
    b2 += (uint32_t)(v * w2);
    a3 += (uint32_t)(u * w3);
    b3 += (uint32_t)(v * w3);
  }
  sa[0] = a0;
  sa[1] = a1;
  sa[2] = a2;
  sa[3] = a3;
  sb[0] = b0;
  sb[1] = b1;
  sb[2] = b2;
  sb[3] = b3;
}

static uint32_t pw_px2(uint32_t off) {
  const int8_t *x = xin + off, *w = wpool + 300U;
  int8_t *y = yout;
  for (uint32_t p = 0U; p < 25U; p += 2U) {
    const int8_t *xa = x + p * 64U, *xb = p + 1U < 25U ? xa + 64U : xa;
    for (uint32_t o = 0U; o < 64U; o += 8U) {
      uint32_t sa[8], sb[8];
      for (uint32_t j = 0U; j < 8U; j++) {
        sa[j] = sb[j] = (uint32_t)bias[o + j];
      }
      k_px2(xa, xb, w + o * 64U, 0U, 64U, sa, sb);
      k_px2(xa, xb, w + o * 64U, 1U, 64U, sa + 4, sb + 4);
      for (uint32_t j = 0U; j < 8U; j++) {
        uint32_t c = o + j;
        y[p * 64U + c] = fnn_clamp_s8(
            fnn_requant((int32_t)sa[j], mult[c], rsh[c]), ZY, -128, 127);
        if (p + 1U < 25U) {
          y[(p + 1U) * 64U + c] = fnn_clamp_s8(
              fnn_requant((int32_t)sb[j], mult[c], rsh[c]), ZY, -128, 127);
        }
      }
    }
  }
  return 0U;
}
BLOCK(b_pw_px2, pw_px2(x), 1600U)

/* candidates the packed kernel was chosen over, in a copy of its driver */
typedef void Rows(const int8_t *x, const int8_t *w, uint32_t n, uint32_t *sum);

BENCH_CALL static void k_grouped(const int8_t *x, const int8_t *w, uint32_t n,
                                 uint32_t *sum) {
  const int8_t *w0 = w, *w1 = w + n, *w2 = w1 + n, *w3 = w2 + n;
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t i = 0U; i < n; i++) {
    int32_t xi = x[i], v0 = w0[i], v1 = w1[i], v2 = w2[i], v3 = w3[i];
    __asm__("" : "+r"(xi), "+r"(v0), "+r"(v1), "+r"(v2), "+r"(v3));
    a0 += (uint32_t)(xi * v0);
    a1 += (uint32_t)(xi * v1);
    a2 += (uint32_t)(xi * v2);
    a3 += (uint32_t)(xi * v3);
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

BENCH_CALL static void k_word(const int8_t *x, const int8_t *w, uint32_t n,
                              uint32_t *sum) {
  const uint32_t *q = (const uint32_t *)(const void *)w;
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t i = 0U; i < n; i++) {
    int32_t xi = x[i];
    uint32_t v = q[i];
    a0 += (uint32_t)(xi * (int8_t)v);
    a1 += (uint32_t)(xi * (int8_t)(v >> 8));
    a2 += (uint32_t)(xi * (int8_t)(v >> 16));
    a3 += (uint32_t)(xi * ((int32_t)v >> 24));
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

#define LANE(v, k) ((int32_t)((v) << (24U - 8U * (k))) >> 24)
BENCH_CALL static void k_x4(const int8_t *x, const int8_t *w, uint32_t n,
                            uint32_t *sum) {
  const uint32_t *q = (const uint32_t *)(const void *)w;
  const uint32_t *xq = (const uint32_t *)(const void *)x;
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t i = 0U; i < n / 4U; i++) {
    uint32_t xs;
    memcpy(&xs, &xq[i], 4U);
    for (uint32_t k = 0U; k < 4U; k++) {
      int32_t xk = LANE(xs, k);
      uint32_t v = q[4U * i + k];
      a0 += (uint32_t)(xk * (int8_t)v);
      a1 += (uint32_t)(xk * (int8_t)(v >> 8));
      a2 += (uint32_t)(xk * (int8_t)(v >> 16));
      a3 += (uint32_t)(xk * ((int32_t)v >> 24));
    }
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

BENCH_CALL static void k_word8(const int8_t *x, const int8_t *w, uint32_t n,
                               uint32_t *sum) {
  const uint32_t *q = (const uint32_t *)(const void *)w;
  uint32_t a[8];
  for (uint32_t j = 0U; j < 8U; j++) {
    a[j] = sum[j];
  }
  for (uint32_t i = 0U; i < n; i++) {
    int32_t xi = x[i];
    uint32_t v = q[2U * i], u = q[2U * i + 1U];
    a[0] += (uint32_t)(xi * (int8_t)v);
    a[1] += (uint32_t)(xi * (int8_t)(v >> 8));
    a[2] += (uint32_t)(xi * (int8_t)(v >> 16));
    a[3] += (uint32_t)(xi * ((int32_t)v >> 24));
    a[4] += (uint32_t)(xi * (int8_t)u);
    a[5] += (uint32_t)(xi * (int8_t)(u >> 8));
    a[6] += (uint32_t)(xi * (int8_t)(u >> 16));
    a[7] += (uint32_t)(xi * ((int32_t)u >> 24));
  }
  for (uint32_t j = 0U; j < 8U; j++) {
    sum[j] = a[j];
  }
}

static uint32_t dense_with(uint32_t off, Rows *k, uint32_t rows) {
  const int8_t *x = xin + off;
  for (uint32_t o = 0U; o < 64U; o += rows) {
    uint32_t a[8];
    for (uint32_t j = 0U; j < rows; j++) {
      a[j] = (uint32_t)bf128[o + j];
    }
    k(x, wpool + o * 128U, 128U, a);
    for (uint32_t j = 0U; j < rows; j++) {
      int32_t v = fnn_requant((int32_t)a[j], mult[0], rsh[0]);
      yout[o + j] = fnn_clamp_s8(v, ZY, -128, 127);
    }
  }
  return 0U;
}
BENCH_CALL static void k_pairs(const int8_t *x, const int8_t *w, uint32_t n,
                               uint32_t *sum) {
  const int32_t *q = (const int32_t *)(const void *)w;
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t i = 0U; i + 1U < n; i += 2U) {
    int32_t x0 = x[i], x1 = x[i + 1U];
    uint32_t p0 = (uint32_t)(x0 * q[2U * i]) + (uint32_t)(x1 * q[2U * i + 2U]);
    uint32_t p1 =
        (uint32_t)(x0 * q[2U * i + 1U]) + (uint32_t)(x1 * q[2U * i + 3U]);
    int32_t l0 = (int16_t)p0, l1 = (int16_t)p1;
    a0 += (uint32_t)l0;
    a1 += (uint32_t)((int32_t)(p0 - (uint32_t)l0) >> 16);
    a2 += (uint32_t)l1;
    a3 += (uint32_t)((int32_t)(p1 - (uint32_t)l1) >> 16);
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

/* At 128 inputs both layouts fit the flash cache. */
static uint32_t dense32(uint32_t off, Rows *k, uint32_t rows, uint32_t bytes) {
  const int8_t *x = xin + off;
  for (uint32_t o = 0U; o < 32U; o += rows) {
    uint32_t a[8];
    for (uint32_t j = 0U; j < rows; j++) {
      a[j] = (uint32_t)bf128[o + j];
    }
    k(x, wpool + o * 128U * bytes, 128U, a);
    for (uint32_t j = 0U; j < rows; j++) {
      int32_t v = fnn_requant((int32_t)a[j], mult[0], rsh[0]);
      yout[o + j] = fnn_clamp_s8(v, ZY, -128, 127);
    }
  }
  return 0U;
}
BLOCK(b_fc32_word8, dense32(x, k_word8, 8U, 1U), 32U)
BLOCK(b_fc32_pairs, dense32(x, k_pairs, 4U, 2U), 32U)

BLOCK(b_fc_grouped, dense_with(x, k_grouped, 4U), 64U)
BLOCK(b_fc_word, dense_with(x, k_word, 4U), 64U)
BLOCK(b_fc_x4, dense_with(x & ~3U, k_x4, 4U), 64U)
BLOCK(b_fc_word8, dense_with(x, k_word8, 8U), 64U)

static uint32_t lut_lib(uint32_t off) {
  fnn_lut_s8(xin + off, yout, 1024U, lut);
  return 0U;
}
BLOCK(b_lut, lut_lib(x), 1024U)

/* candidates the table kernel was chosen over */
typedef void Lut(const int8_t *x, int8_t *y, uint32_t n, const int8_t *t);

BENCH_CALL static void k_lut8(const int8_t *x, int8_t *y, uint32_t n,
                              const int8_t *lut8) {
  const int8_t *t = lut8 + 128;
  uint32_t i = 0U;
  for (; i + 8U <= n; i += 8U) {
    int32_t a = x[i], b = x[i + 1U], c = x[i + 2U], d = x[i + 3U];
    int32_t e = x[i + 4U], f = x[i + 5U], g = x[i + 6U], h = x[i + 7U];
    __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e), "+r"(f),
                 "+r"(g), "+r"(h));
    y[i] = t[a];
    y[i + 1U] = t[b];
    y[i + 2U] = t[c];
    y[i + 3U] = t[d];
    y[i + 4U] = t[e];
    y[i + 5U] = t[f];
    y[i + 6U] = t[g];
    y[i + 7U] = t[h];
  }
  for (; i < n; i++) {
    y[i] = t[x[i]];
  }
}

BENCH_CALL static void k_lut8g(const int8_t *x, int8_t *y, uint32_t n,
                               const int8_t *lut8) {
  const int8_t *t = lut8 + 128;
  uint32_t i = 0U;
  for (; i + 8U <= n; i += 8U) {
    int32_t a = x[i], b = x[i + 1U], c = x[i + 2U], d = x[i + 3U];
    int32_t e = x[i + 4U], f = x[i + 5U], g = x[i + 6U], h = x[i + 7U];
    __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e), "+r"(f),
                 "+r"(g), "+r"(h));
    a = t[a];
    b = t[b];
    c = t[c];
    d = t[d];
    e = t[e];
    f = t[f];
    g = t[g];
    h = t[h];
    __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e), "+r"(f),
                 "+r"(g), "+r"(h));
    y[i] = (int8_t)a;
    y[i + 1U] = (int8_t)b;
    y[i + 2U] = (int8_t)c;
    y[i + 3U] = (int8_t)d;
    y[i + 4U] = (int8_t)e;
    y[i + 5U] = (int8_t)f;
    y[i + 6U] = (int8_t)g;
    y[i + 7U] = (int8_t)h;
  }
  for (; i < n; i++) {
    y[i] = t[x[i]];
  }
}

BENCH_CALL static void k_lutw(const int8_t *x, int8_t *y, uint32_t n,
                              const int8_t *lut8) {
  const uint8_t *t = (const uint8_t *)lut8;
  uint32_t i = 0U;
  for (; i + 4U <= n; i += 4U) {
    uint32_t v;
    memcpy(&v, x + i, 4U);
    v ^= 0x80808080U;
    uint32_t o = (uint32_t)t[v & 255U] | (uint32_t)t[(v >> 8) & 255U] << 8 |
                 (uint32_t)t[(v >> 16) & 255U] << 16 |
                 (uint32_t)t[v >> 24] << 24;
    memcpy(y + i, &o, 4U);
  }
  for (; i < n; i++) {
    y[i] = lut8[x[i] + 128];
  }
}

#define HOLD8(a, b, c, d, e, f, g, h)                                          \
  __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e), "+r"(f), "+r"(g),  \
          "+r"(h))
#define STORE(p, v)                                                            \
  do {                                                                         \
    *(p) = (int8_t)(v);                                                        \
    __asm__("" ::: "memory");                                                  \
  } while (0)
BENCH_CALL static void k_lut8s(const int8_t *x, int8_t *y, uint32_t n,
                               const int8_t *lut8) {
  const int8_t *t = lut8 + 128;
  for (uint32_t i = 0U; i + 8U <= n; i += 8U) {
    int32_t a = x[i], b = x[i + 1U], c = x[i + 2U], d = x[i + 3U];
    int32_t e = x[i + 4U], f = x[i + 5U], g = x[i + 6U], h = x[i + 7U];
    HOLD8(a, b, c, d, e, f, g, h);
    a = t[a];
    b = t[b];
    c = t[c];
    d = t[d];
    e = t[e];
    f = t[f];
    g = t[g];
    h = t[h];
    HOLD8(a, b, c, d, e, f, g, h);
    STORE(y + i, a);
    STORE(y + i + 1U, b);
    STORE(y + i + 2U, c);
    STORE(y + i + 3U, d);
    STORE(y + i + 4U, e);
    STORE(y + i + 5U, f);
    STORE(y + i + 6U, g);
    STORE(y + i + 7U, h);
  }
}

#define STOREU(p, v)                                                           \
  do {                                                                         \
    *(p) = (uint8_t)(v);                                                       \
    __asm__("" ::: "memory");                                                  \
  } while (0)
FX_INLINE void lutu8(const uint8_t *x, uint8_t *y, const uint8_t *t) {
  uint32_t a = x[0], b = x[1], c = x[2], d = x[3];
  uint32_t e = x[4], f = x[5], g = x[6], h = x[7];
  HOLD8(a, b, c, d, e, f, g, h);
  a = t[a];
  b = t[b];
  c = t[c];
  d = t[d];
  e = t[e];
  f = t[f];
  g = t[g];
  h = t[h];
  HOLD8(a, b, c, d, e, f, g, h);
  STOREU(y, a);
  STOREU(y + 1, b);
  STOREU(y + 2, c);
  STOREU(y + 3, d);
  STOREU(y + 4, e);
  STOREU(y + 5, f);
  STOREU(y + 6, g);
  STOREU(y + 7, h);
}
BENCH_CALL static void k_lutu8(const int8_t *xs, int8_t *ys, uint32_t n,
                               const int8_t *lut8) {
  const uint8_t *x = (const uint8_t *)xs, *t = (const uint8_t *)lut8;
  uint8_t *y = (uint8_t *)ys;
  const uint8_t *end = x + (n & ~15U);
  while (x != end) {
    lutu8(x, y, t);
    lutu8(x + 8, y + 8, t);
    x += 16;
    y += 16;
    FX_OPAQUE(y);
  }
}

static uint32_t lut_with(uint32_t off, Lut *k) {
  k(xin + off, yout, 1024U, lut);
  return 0U;
}
BLOCK(b_lut8, lut_with(x, k_lut8), 1024U)
BLOCK(b_lut8g, lut_with(x, k_lut8g), 1024U)
BLOCK(b_lutw, lut_with(x, k_lutw), 1024U)
BLOCK(b_lut8s, lut_with(x, k_lut8s), 1024U)
BLOCK(b_lutu8, lut_with(x, k_lutu8), 1024U)

static uint32_t softmax(uint32_t off, uint32_t n) {
  fnn_softmax_s8(xin + off, yout, n, etab);
  return 0U;
}
BLOCK(b_softmax12, softmax(x, 12U), 12U)
BLOCK(b_softmax256, softmax(x, 256U), 256U)

BENCH_CALL static void k_softmax_div(const int8_t *x, int8_t *y, uint32_t n,
                                     const uint32_t *t) {
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    s += t[mx - x[i]];
  }
  for (uint32_t i = 0U; i < n; i++) {
    uint64_t q = (512U * (uint64_t)t[mx - x[i]] + s) / (2U * s);
    y[i] = q > 255U ? 127 : (int8_t)((int32_t)q - 128);
  }
}
BENCH_CALL static void k_softmax_fix(const int8_t *x, int8_t *y, uint32_t n,
                                     const uint32_t *t) {
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    s += t[mx - x[i]];
  }
  uint32_t hi = (uint32_t)(s >> 32);
  uint32_t sh = hi != 0U ? 41U - fx_clz(hi) : 9U;
  uint64_t h = s >> 1;
  uint32_t b = (uint32_t)(s >> sh) + 1U, ht = (uint32_t)(h >> sh);
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t e = t[mx - x[i]];
    uint32_t q = ((e >> (sh - 8U)) + ht) / b;
    uint64_t r = ((uint64_t)e << 8) + h - (uint64_t)q * s;
    q += r >= s ? 1U : 0U;
    y[i] = q > 255U ? 127 : (int8_t)((int32_t)q - 128);
  }
}
static uint32_t softmax_fix(uint32_t off, uint32_t n) {
  k_softmax_fix(xin + off, yout, n, etab);
  return 0U;
}
BLOCK(b_softmax12_fix, softmax_fix(x, 12U), 12U)

static uint32_t softmax_div(uint32_t off, uint32_t n) {
  k_softmax_div(xin + off, yout, n, etab);
  return 0U;
}
BLOCK(b_softmax12_div, softmax_div(x, 12U), 12U)

BENCH_CALL static uint32_t k_argmax2(const int8_t *x, uint32_t n) {
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  uint32_t k = 0U;
  while (x[k] != mx) {
    k++;
  }
  return k;
}
BENCH_UNARY_DEF(b_argmax12_2, uint32_t, in_off, uint32_t,
                k_argmax2(xin + x, 12U), (void)0, (void)0)
BENCH_UNARY_DEF(b_argmax256_2, uint32_t, in_off, uint32_t,
                k_argmax2(xin + x, 256U), (void)0, (void)0)

BENCH_UNARY_DEF(b_argmax12, uint32_t, in_off, uint32_t,
                fnn_argmax_s8(xin + x, 12U), (void)0, (void)0)
BENCH_UNARY_DEF(b_argmax256, uint32_t, in_off, uint32_t,
                fnn_argmax_s8(xin + x, 256U), (void)0, (void)0)

#define SCALE 0.05f
static int32_t qm15, qm31, qmf, dm15;
static int8_t qr15, qr31, qrf, dr15;

static void conv_setup(void) {
  static bool done;
  if (!done) {
    done = true;
    (void)fnn_quant_from_scale(0x1p-15 / (double)SCALE, &qm15, &qr15);
    (void)fnn_quant_from_scale(0x1p-31 / (double)SCALE, &qm31, &qr31);
    (void)fnn_quant_from_scale(1.0 / (double)SCALE, &qmf, &qrf);
    (void)fnn_quant_from_scale(0x1p15 * (double)SCALE, &dm15, &dr15);
  }
}

static uint32_t quant(uint32_t off, unsigned kind) {
  conv_setup();
  switch (kind) {
  case 0:
    fnn_quantize_q15_s8((const int16_t *)(void *)(xin + (off & ~1U)), yout,
                        256U, qm15, qr15, ZY);
    break;
  case 1:
    fnn_quantize_q31_s8((const int32_t *)(void *)(xin + (off & ~3U)), yout,
                        256U, qm31, qr31, ZY);
    break;
  case 2:
    fnn_quantize_f32_s8(fin + off % 16U, yout, FN, qmf, qrf, ZY);
    break;
  case 3:
    fnn_dequantize_s8_q15(xin + off, (int16_t *)(void *)yout, 256U, dm15,
                          dr15, ZY);
    break;
  default:
    fnn_dequantize_s8_f32(xin + off, (float *)(void *)yout, FN, SCALE, ZY);
    break;
  }
  return 0U;
}
BLOCK(b_quant_q15, quant(x, 0U), 256U)
BLOCK(b_quant_q31, quant(x, 1U), 256U)
BLOCK(b_quant_f32, quant(x, 2U), FN)
BLOCK(b_dequant_q15, quant(x, 3U), 512U)
BLOCK(b_dequant_f32, quant(x, 4U), 4U * FN)

BENCH_CALL static void k_quant_tfl(const float *x, int8_t *y, uint32_t n,
                                   float s, int32_t zp) {
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = (int32_t)roundf(x[i] / s) + zp;
    y[i] = (int8_t)(v < -128 ? -128 : (v > 127 ? 127 : v));
  }
}
BENCH_CALL static void k_dequant_mul(const int8_t *x, float *y, uint32_t n,
                                     float s, int32_t zp) {
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = s * (float)(x[i] - zp);
  }
}
static uint32_t quant_tfl(uint32_t off) {
  k_quant_tfl(fin + off % 16U, yout, FN, SCALE, ZY);
  return 0U;
}
static uint32_t dequant_mul(uint32_t off) {
  k_dequant_mul(xin + off, (float *)(void *)yout, FN, SCALE, ZY);
  return 0U;
}
BLOCK(b_quant_tfl, quant_tfl(x), FN)
BLOCK(b_dequant_mul, dequant_mul(x), 4U * FN)

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
BLOCK(c_fc_128, c_fc(x, 128, 64), 64U)
BLOCK_COLD(c_fc_256_cold, c_fc(x, 256, 64), 64U)
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

static uint32_t c_concat(uint32_t off) {
  arm_concatenation_s8_x(xin + off, 16, 8, 8, 1, yout, 32, 0);
  arm_concatenation_s8_x(xin + 1024U + off, 16, 8, 8, 1, yout, 32, 16);
  return 0U;
}
BLOCK(c_concat_x, c_concat(x), 2048U)

/* TFLite's softmax parameters for an input scale of 1/16 */
static uint32_t c_softmax(uint32_t off) {
  arm_softmax_s8(xin + off, 1, 12, 1 << 30, 23, -248, yout);
  return 0U;
}
BLOCK(c_softmax_12, c_softmax(x), 12U)

/* Traps on any error: a wrong argument must not pass for a fast time. */
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
    {"fnn_dense_s8, 128 -> 64", "flexlib", b_fc128, true},
    {"fnn_dense_s8, 128 -> 64, packed", "flexlib", b_fc128p, true},
    {"fnn_dense_s8, 64 -> 12", "flexlib", b_fc64, true},
    {"fnn_dense_s8, 64 -> 12, packed", "flexlib", b_fc64p, true},
    {"dense 128 -> 64, rows, loads before multiplies", "candidate",
     b_fc_grouped, true},
    {"dense 128 -> 64, four rows' weights per word", "candidate", b_fc_word,
     true},
    {"dense 128 -> 64, four rows per word, inputs four to a word", "candidate",
     b_fc_x4, true},
    {"dense 128 -> 64, eight rows per double word, one input a pass",
     "candidate", b_fc_word8, true},
    {"dense 128 -> 32, eight rows per double word", "candidate", b_fc32_word8,
     true},
    {"dense 128 -> 32, two rows per multiply (16-bit halves)", "candidate",
     b_fc32_pairs, true},
    {"fnn_conv_s8, 1 x 1, 64 -> 64, 5 x 5", "flexlib", b_pw64, true},
    {"fnn_conv_s8, 1 x 1, 64 -> 64, 5 x 5, packed", "flexlib", b_pw64p, true},
    {"fnn_conv_s8, 3 x 3, 8 -> 16, 12 x 12", "flexlib", b_cv3, true},
    {"fnn_conv_s8, 3 x 3, 8 -> 16, 12 x 12, packed", "flexlib", b_cv3p, true},
    {"fnn_conv_s8, 5 taps, 16 -> 32, 64", "flexlib", b_cv1d, true},
    {"fnn_conv_s8, 5 taps, 16 -> 32, 64, packed", "flexlib", b_cv1dp, true},
    {"conv 1 x 1, 64 -> 64, 5 x 5, two pixels x four rows", "candidate",
     b_pw_px2, true},
    {"fnn_dwconv_s8, 3 x 3, 64 x 5 x 5", "flexlib", b_dw64, true},
    {"fnn_dwconv_s8, 3 x 3, 64 x 7 x 7 to 5 x 5, VALID", "flexlib", b_dw64v,
     true},
    {"fnn_maxpool_s8, 2 x 2, 16 x 16 x 16", "flexlib", b_maxpool16, true},
    {"fnn_avgpool_s8, 5 x 5 x 64 to 1 x 1", "flexlib", b_avgpool64, true},
    {"fnn_mean_s8, 5 x 5 x 64", "flexlib", b_mean64, true},
    {"max pool 2 x 2, 16 x 16 x 16, four channels in registers", "candidate",
     b_maxpool4, true},
    {"average pool 5 x 5 x 64, four channels in 16-bit lanes of two words",
     "candidate", b_avgpool_lanes, true},
    {"fnn_add_s8, 1024", "flexlib", b_add1024, true},
    {"add 1024, TFLite's three roundings (FNN_ROUND_GEMMLOWP)", "candidate",
     b_add1024_tfl, true},
    {"fnn_concat_s8, 2 x 8 x 8 x 16 to 8 x 8 x 32", "flexlib", b_concat, true},
    {"fnn_lut_s8, 1024", "flexlib", b_lut, true},
    {"table 1024, eight a pass, each looked up and stored in turn",
     "candidate", b_lut8, true},
    {"table 1024, eight a pass, stores merged into words", "candidate",
     b_lut8g, true},
    {"table 1024, four to a word", "candidate", b_lutw, true},
    {"table 1024, eight a pass, lookups before byte stores", "candidate",
     b_lut8s, true},
    {"table 1024 by bit pattern, sixteen a pass in eights", "candidate",
     b_lutu8, true},
    {"fnn_softmax_s8, 12", "flexlib", b_softmax12, true},
    {"fnn_softmax_s8, 256", "flexlib", b_softmax256, true},
    {"softmax 12, a 64-bit division per output", "candidate", b_softmax12_div,
     true},
    {"softmax 12, a 64-bit correction per output", "candidate",
     b_softmax12_fix, true},
    {"fnn_argmax_s8, 12", "flexlib", b_argmax12, true},
    {"fnn_argmax_s8, 256", "flexlib", b_argmax256, true},
    {"argmax 12, the maximum, then its index", "candidate", b_argmax12_2, true},
    {"argmax 256, the maximum, then its index", "candidate", b_argmax256_2,
     true},
    {"fnn_quantize_q15_s8, 256", "flexlib", b_quant_q15, true},
    {"fnn_quantize_q31_s8, 256", "flexlib", b_quant_q31, true},
    {"fnn_quantize_f32_s8, 128", "flexlib", b_quant_f32, true},
    {"quantise 128 floats, roundf(x / s) in soft float", "candidate",
     b_quant_tfl, true},
    {"fnn_dequantize_s8_q15, 256", "flexlib", b_dequant_q15, true},
    {"fnn_dequantize_s8_f32, 128", "flexlib", b_dequant_f32, true},
    {"dequantise 128 to float, the soft-float multiply", "candidate",
     b_dequant_mul, true},
#ifdef BENCH_HAVE_CMSIS_NN
    {"arm_nn_requantize + clamp, 256 outputs, 64 channels", "CMSIS-NN",
     c_requant, false},
    {"arm_fully_connected_s8, 128 -> 64", "CMSIS-NN", c_fc_128, false},
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
    {"arm_concatenation_s8_x, 2 x 8 x 8 x 16 to 8 x 8 x 32", "CMSIS-NN",
     c_concat_x, false},
    {"arm_softmax_s8, 12", "CMSIS-NN", c_softmax_12, false},
#endif
};

const BenchGroup bench_group_fnn = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fnn_dense_s8, 256 -> 64", "flexlib", b_fc256_cold, true},
    {"fnn_dense_s8, 256 -> 64, packed", "flexlib", b_fc256p_cold, true},
#ifdef BENCH_HAVE_CMSIS_NN
    {"arm_fully_connected_s8, 256 -> 64", "CMSIS-NN", c_fc_256_cold, false},
#endif
};

const BenchGroup bench_group_fnn_cold = {cold, BENCH_COUNT(cold)};

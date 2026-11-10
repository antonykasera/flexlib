/**
 * @file fm_quat.c
 * @brief Float quaternions on fm_internal.h's exact sums of products
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_quat.h"
#include "fm_float.h"
#include "fm_internal.h"
#include <string.h>

#define F32_QNAN 0x7FC00000U
#define ONE_M (1 << 30) /* 1.0 unpacked on the A side, exponent 127 */

FX_INLINE uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

FX_INLINE float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

FX_INLINE void put4(FM_Quat *r, uint32_t w, uint32_t x, uint32_t y,
                    uint32_t z) {
  const uint32_t v[4] = {w, x, y, z};
  memcpy(r, v, sizeof v);
}

/* one-product sum out of line (normalize, integrate) */
__attribute__((noinline)) static uint32_t sum1(int32_t ma, int32_t ea,
                                               int32_t mb, int32_t eb) {
  return fm_sum_f32(&ma, &ea, &mb, &eb, 1U, 1U);
}

/* the four components unpacked; nonzero if any is inf or NaN */
FX_INLINE uint32_t unp4(const FM_Quat *q, uint32_t sh, int32_t m[4],
                        int32_t e[4]) {
  uint32_t b[4];
  uint32_t bad = 0U;
  memcpy(b, q, sizeof b);
#pragma GCC unroll 4
  for (uint32_t i = 0U; i < 4U; i++) {
    m[i] = fm_unp_f32(b[i], sh, &e[i], &bad);
  }
  return bad;
}

/* x y 2^d, x and y finite */
FX_INLINE uint32_t fmul(uint32_t x, uint32_t y, int32_t d) {
  int32_t ex;
  int32_t ey;
  uint32_t bad = 0U;
  int32_t mx = fm_unp_f32(x, 7U, &ex, &bad);
  int32_t my = fm_unp_f32(y, 6U, &ey, &bad);
  return sum1(mx, ex, my, ey + d);
}

/* one component of a b: a[k] b[perm k], term k negated if bit k of neg */
FX_INLINE uint32_t hamilton(const int32_t *ma, const int32_t *ea,
                            const int32_t *mb, const int32_t *eb, uint32_t perm,
                            uint32_t neg) {
  int32_t pm[4];
  int32_t pe[4];
#pragma GCC unroll 4
  for (uint32_t k = 0U; k < 4U; k++) {
    uint32_t j = (perm >> (2U * k)) & 3U;
    pm[k] = ((neg >> k) & 1U) != 0U ? -mb[j] : mb[j];
    pe[k] = eb[j];
  }
  return fm_sum_f32(ma, ea, pm, pe, 1U, 4U);
}

void fm_quat_mul(FM_Quat *r, const FM_Quat *a, const FM_Quat *b) {
  int32_t ma[4];
  int32_t ea[4];
  int32_t mb[4];
  int32_t eb[4];
  if ((unp4(a, 7U, ma, ea) | unp4(b, 6U, mb, eb)) != 0U) {
    put4(r, F32_QNAN, F32_QNAN, F32_QNAN, F32_QNAN);
    return;
  }
  /* w = aw bw - ax bx - ay by - az bz, x = aw bx + ax bw + ay bz - ... */
  put4(r, hamilton(ma, ea, mb, eb, 0xE4U, 0xEU),
       hamilton(ma, ea, mb, eb, 0xB1U, 0x8U),
       hamilton(ma, ea, mb, eb, 0x4EU, 0x2U),
       hamilton(ma, ea, mb, eb, 0x1BU, 0x4U));
}

/* |q|^2 = N exactly, scaled by 4^-k into [0.5, 2); 1/|q| by rsqrt + Newton */
void fm_quat_normalize(FM_Quat *r, const FM_Quat *q) {
  int32_t m[4];
  int32_t e[4];
  if (unp4(q, 7U, m, e) != 0U) {
    put4(r, F32_QNAN, F32_QNAN, F32_QNAN, F32_QNAN);
    return;
  }
  int32_t mh[4]; /* the B side: exact, the A side's mantissas are even */
#pragma GCC unroll 4
  for (uint32_t i = 0U; i < 4U; i++) {
    mh[i] = m[i] >> 1;
  }
  int32_t emax;
  uint64_t s = (uint64_t)fm_sum_f32_s(m, e, mh, e, 1U, 4U, &emax);
  if (s == 0U) {
    put4(r, 0x3F800000U, 0U, 0U, 0U);
    return;
  }
  uint32_t lz = fu_clz64(s);
  uint32_t n = (uint32_t)((s << lz) >> 32);
  int32_t ex = emax - 123 - (int32_t)lz; /* N's biased exponent */
  int32_t k = (ex - 126) >> 1;
  uint32_t es = (uint32_t)(ex - 2 * k); /* 126 or 127 */
  uint32_t y = f2u(fm_rsqrtf(u2f(((es - 1U) << 23) + (n >> 8))));  /* seed */
  uint32_t y31 = (((y << 8) | 0x80000000U) >> (127U - (y >> 23))); /* Q31 */
  uint32_t x31 = n >> (127U - es);
  uint64_t p = fx_umull(x31, (uint32_t)(fx_umull(y31, y31) >> 32)); /* Q61 */
  uint32_t d = (uint32_t)(((3ULL << 61) - p) >> 31);        /* 3 - N y^2, Q30 */
  int32_t my = (int32_t)(uint32_t)(fx_umull(y31, d) >> 33); /* Q29 */
  int32_t ey = 127 - k;
  put4(r, sum1(m[0], e[0], my, ey), sum1(m[1], e[1], my, ey),
       sum1(m[2], e[2], my, ey), sum1(m[3], e[3], my, ey));
}

/* v + 2 w t + 2 u x t, t = u x v, u = q's vector part */
void fm_quat_rotate(float r[3], const FM_Quat *q, const float v[3]) {
  int32_t mq[4];
  int32_t eq[4];
  int32_t mv[3];
  int32_t ev[3];
  uint32_t vb[3];
  memcpy(vb, v, sizeof vb);
  uint32_t bad = unp4(q, 7U, mq, eq);
#pragma GCC unroll 3
  for (uint32_t i = 0U; i < 3U; i++) {
    mv[i] = fm_unp_f32(vb[i], 6U, &ev[i], &bad);
  }
  uint32_t out[3];
  if (bad != 0U) {
    out[0] = out[1] = out[2] = F32_QNAN;
    memcpy(r, out, sizeof out);
    return;
  }
  int32_t mt[3];
  int32_t et[3];
#pragma GCC unroll 3
  for (uint32_t i = 0U; i < 3U; i++) {
    uint32_t j = i == 2U ? 0U : i + 1U;
    uint32_t k = i == 0U ? 2U : i - 1U;
    const int32_t am[2] = {mq[1U + j], mq[1U + k]};
    const int32_t ae[2] = {eq[1U + j], eq[1U + k]};
    const int32_t bm[2] = {mv[k], -mv[j]};
    const int32_t be[2] = {ev[k], ev[j]};
    mt[i] = fm_unp_f32(fm_sum_f32(am, ae, bm, be, 1U, 2U), 6U, &et[i], &bad);
    et[i]++; /* 2 t */
  }
#pragma GCC unroll 3
  for (uint32_t i = 0U; i < 3U; i++) {
    uint32_t j = i == 2U ? 0U : i + 1U;
    uint32_t k = i == 0U ? 2U : i - 1U;
    const int32_t am[4] = {ONE_M, mq[0], mq[1U + j], mq[1U + k]};
    const int32_t ae[4] = {127, eq[0], eq[1U + j], eq[1U + k]};
    const int32_t bm[4] = {mv[i], mt[i], mt[k], -mt[j]};
    const int32_t be[4] = {ev[i], et[i], et[k], et[j]};
    out[i] = fm_sum_f32(am, ae, bm, be, 1U, 4U);
  }
  memcpy(r, out, sizeof out);
}

/* dq = (cos th, sin th w / |w|), th = |w| dt / 2 */
void fm_quat_integrate(FM_Quat *r, const FM_Quat *q, const float w[3],
                       float dt) {
  uint32_t wb[3];
  int32_t mw[3];
  int32_t mh[3];
  int32_t ew[3];
  uint32_t bad = 0U;
  memcpy(wb, w, sizeof wb);
#pragma GCC unroll 3
  for (uint32_t i = 0U; i < 3U; i++) {
    mw[i] = fm_unp_f32(wb[i], 7U, &ew[i], &bad);
    mh[i] = mw[i] >> 1;
  }
  uint32_t n2 = fm_sum_f32(mw, ew, mh, ew, 1U, 3U);
  bad |= (f2u(dt) & 0x7F800000U) == 0x7F800000U ? 1U : 0U;
  if (bad != 0U || n2 >= 0x7F800000U) {
    put4(r, F32_QNAN, F32_QNAN, F32_QNAN, F32_QNAN);
    return;
  }
  if (n2 == 0U) { /* a shortcut: dq would be (1, 0, 0, 0) */
    fm_quat_normalize(r, q);
    return;
  }
  uint32_t y = f2u(fm_rsqrtf(u2f(n2)));
  uint32_t th = fmul(fmul(n2, y, 0), f2u(dt), -1);
  float s; /* an overflowed th gives NaN, which fm_quat_mul passes on */
  float c;
  fm_sincosf(u2f(th), &s, &c);
  uint32_t k = fmul(f2u(s), y, 0);
  FM_Quat dq;
  put4(&dq, f2u(c), fmul(k, wb[0], 0), fmul(k, wb[1], 0), fmul(k, wb[2], 0));
  fm_quat_mul(r, q, &dq);
  fm_quat_normalize(r, r);
}

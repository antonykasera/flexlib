/**
 * @file fd_lms.c
 * @brief LMS adaptive filters: plain, normalised and sign-error, q15 / q31
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_lms.h"
#include "fd_fir_core.h"
#include "fd_wide.h"

/* the rounding of q = floor(2 |num| / den) to nearest, ties up in sign */
FX_INLINE int32_t qround(uint64_t q, bool neg, bool exact, uint32_t lim) {
  uint64_t m = neg ? (q >> 1) + ((q & 1U) != 0U && !exact ? 1U : 0U)
                   : (q >> 1) + (q & 1U);
  m = m > lim ? lim : m;
  return neg ? -(int32_t)m : (int32_t)m;
}

/* round(num / den), clamped to +-32767: den > 0, num < 2^46. With den cut
 * to its top 16 bits, 2 num / den is one 32-bit divide, then corrected
 * against the exact product. */
static int32_t qdiv15(uint64_t num, bool neg, uint64_t den) {
  const uint64_t n2 = num << 1;
  if (n2 >= den << 16) {
    return neg ? -32767 : 32767;
  }
  uint32_t h = (uint32_t)(den >> 32);
  uint32_t b = h != 0U ? 64U - fx_clz(h) : 32U - fx_clz((uint32_t)den);
  uint32_t s = b > 16U ? b - 16U : 0U;
  uint32_t q = (uint32_t)(n2 >> s) / (uint32_t)(den >> s);
  uint64_t p = den * q;
  while (p > n2) {
    q--;
    p -= den;
  }
  return qround(q, neg, p == n2, 32767U);
}

FX_INLINE uint32_t bits128(FD_U128 v) {
  if (v.hi != 0U) {
    uint32_t h = (uint32_t)(v.hi >> 32);
    return h != 0U ? 128U - fx_clz(h) : 96U - fx_clz((uint32_t)v.hi);
  }
  uint32_t h = (uint32_t)(v.lo >> 32);
  return h != 0U ? 64U - fx_clz(h) : 32U - fx_clz((uint32_t)v.lo);
}

/* round(num / den) to nearest (ties up in sign), clamped to +-lim: den > 0.
 * The quotient of 2 num / den is estimated from den's top 32 bits (Knuth),
 * then corrected against the exact product. */
static int32_t qdiv(FD_U128 num, bool neg, FD_U128 den, uint32_t lim) {
  const FD_U128 n2 = fd_u128_shl(num, 1U);
  if (fd_u128_ge(n2, fd_u128_shl(den, 32U))) {
    return neg ? -(int32_t)lim : (int32_t)lim;
  }
  uint32_t b = bits128(den), s = b > 32U ? b - 32U : 0U;
  FD_U128 nn = fd_u128_shr(n2, s);
  uint32_t dn = (uint32_t)fd_u128_shr(den, s).lo, rem;
  uint32_t nh = (uint32_t)(nn.lo >> 32);
  uint64_t q = nh >= dn ? 0xFFFFFFFFU : fd_divlu(nh, (uint32_t)nn.lo, dn, &rem);
  FD_U128 p = fd_u128_mul32(den, (uint32_t)q);
  while (!fd_u128_ge(n2, p)) {
    q--;
    p = fd_u128_sub(p, den);
  }
  FD_U128 r = fd_u128_sub(n2, p);
  return qround(q, neg, r.hi == 0U && r.lo == 0U, lim);
}

void fd_lms_init_q15(FD_LmsQ15 *f, int32_t *w, uint32_t ntaps, fm_q15 *state,
                     uint32_t state_len, fm_q15 mu, FD_LmsMode mode) {
  FX_ASSERT(ntaps >= 1U && state_len > ntaps && mu >= 0);
  f->w = w;
  f->state = state;
  f->ntaps = ntaps;
  f->len = state_len;
  f->pos = ntaps;
  f->mode = (uint32_t)mode;
  f->mu = mu;
  f->step = 0;
  f->res = 0;
  f->energy = 0U;
  for (uint32_t k = 0U; k < ntaps; k++) {
    w[k] = 1 << 14; /* zero, plus the rounding offset */
    state[k] = 0;
  }
}

/* the owed update w[k] += step p[-1 - k], fused with the output sum of
 * w[k] >> 15 times p[-k]; p points one past the newest sample */
FX_INLINE int64_t pass15(int32_t *w, const fm_q15 *p, uint32_t t,
                         int32_t step) {
  int64_t acc = 0;
  int32_t cur = *--p;
  FX_UNROLL(4)
  for (uint32_t k = 0U; k < t; k++) {
    int32_t old = *--p;
    int32_t u = FX_SSAT(w[k] + step * old, 31);
    w[k] = u;
    acc += (u >> 15) * cur;
    cur = old;
  }
  return acc;
}

void fd_lms_q15(FD_LmsQ15 *f, const fm_q15 *x, const fm_q15 *d, fm_q15 *y,
                fm_q15 *e, uint32_t n) {
  const uint32_t t = f->ntaps;
  for (uint32_t i = 0U; i < n; i++) {
    if (f->pos == f->len) {
      for (uint32_t k = 0U; k < t; k++) {
        f->state[k] = f->state[f->len - t + k];
      }
      f->pos = t;
    }
    fm_q15 *p = f->state + f->pos;
    int32_t xn = x[i], xo = p[-(int32_t)t];
    *p++ = (fm_q15)xn;
    f->pos++;
    int64_t acc = pass15(f->w, p, t, f->step);
    fm_q15 yo = out15w(acc);
    int32_t eo = fm_sat_q15(d[i] - yo);
    if (y != NULL) {
      y[i] = yo;
    }
    if (e != NULL) {
      e[i] = (fm_q15)eo;
    }
    int32_t me = f->mu * eo;
    if (f->mode == (uint32_t)FD_LMS_NORM) {
      f->energy += (uint32_t)(xn * xn);
      f->energy -= (uint32_t)(xo * xo);
      f->step =
          qdiv15((uint64_t)(me < 0 ? -me : me) << 15, me < 0, f->energy + 1U);
    } else if (f->mode == (uint32_t)FD_LMS_SIGN) {
      f->step = eo > 0 ? f->mu : (eo < 0 ? -f->mu : 0);
    } else {
      int32_t v = me + f->res; /* the last rounding carried on */
      f->step = (v + 0x4000) >> 15;
      f->res = v - f->step * 32768;
    }
  }
}

void fd_lms_coefs_q15(FD_LmsQ15 *f, fm_q15 *h) {
  const fm_q15 *p = f->state + f->pos;
  for (uint32_t k = 0U; k < f->ntaps; k++) {
    int32_t u = FX_SSAT(f->w[k] + f->step * p[-1 - (int32_t)k], 31);
    f->w[k] = u;
    h[k] = (fm_q15)(u >> 15);
  }
  f->step = 0;
}

/* ---- q31 ---- */

void fd_lms_init_q31(FD_LmsQ31 *f, int32_t *w, uint32_t ntaps, fm_q31 *state,
                     uint32_t state_len, fm_q31 mu, FD_LmsMode mode) {
  FX_ASSERT(ntaps >= 1U && state_len > ntaps && mu >= 0);
  f->w = w;
  f->state = state;
  f->ntaps = ntaps;
  f->len = state_len;
  f->pos = ntaps;
  f->mode = (uint32_t)mode;
  f->mu = mu;
  f->step = 0;
  f->energy[0] = f->energy[1] = 0U;
  for (uint32_t k = 0U; k < ntaps; k++) {
    w[k] = 0;
    state[k] = 0;
  }
}

/* w + round(step x / 2^32) saturated to Q30's range: w 2^32 + 2^31 in the
 * accumulator, one multiply-accumulate, its high word */
FX_INLINE int32_t upd31(int32_t w, int32_t step, int32_t x) {
  int64_t a = fx_smlal((int64_t)w * 4294967296LL + 0x80000000LL, step, x);
  return FX_SSAT((int32_t)(a >> 32), 31);
}

/* round(s / 2^30), saturated: it fits when bits 61..95 are all equal */
FX_INLINE fm_q31 out30w(Sum96 s) {
  sum96_add(&s, 0x20000000);
  uint32_t top = (uint32_t)(s.lo >> 61);
  if ((s.hi == 0 && top == 0U) || (s.hi == -1 && top == 7U)) {
    return (fm_q31)(uint32_t)(s.lo >> 30);
  }
  return s.hi < 0 ? INT32_MIN : INT32_MAX;
}

/* the q15 pass in q31: the output sum in 96 bits, two products at a time */
FX_INLINE Sum96 pass31(int32_t *w, const fm_q31 *p, uint32_t t, int32_t step) {
  Sum96 acc = {0U, 0};
  int32_t cur = *--p;
  uint32_t k = 0U;
  for (; k + 2U <= t; k += 2U) {
    int32_t o0 = *--p, o1 = *--p;
    int32_t u0 = upd31(w[k], step, o0), u1 = upd31(w[k + 1U], step, o1);
    w[k] = u0;
    w[k + 1U] = u1;
    sum96_add(&acc, fx_smlal(fx_smull(u0, cur), u1, o0));
    cur = o1;
  }
  if (k < t) {
    int32_t u = upd31(w[k], step, *--p);
    w[k] = u;
    sum96_add(&acc, fx_smull(u, cur));
  }
  return acc;
}

void fd_lms_q31(FD_LmsQ31 *f, const fm_q31 *x, const fm_q31 *d, fm_q31 *y,
                fm_q31 *e, uint32_t n) {
  const uint32_t t = f->ntaps;
  for (uint32_t i = 0U; i < n; i++) {
    if (f->pos == f->len) {
      for (uint32_t k = 0U; k < t; k++) {
        f->state[k] = f->state[f->len - t + k];
      }
      f->pos = t;
    }
    fm_q31 *p = f->state + f->pos;
    int32_t xn = x[i], xo = p[-(int32_t)t];
    *p++ = xn;
    f->pos++;
    fm_q31 yo = out30w(pass31(f->w, p, t, f->step));
    int32_t eo = fm_sub_q31(d[i], yo);
    if (y != NULL) {
      y[i] = yo;
    }
    if (e != NULL) {
      e[i] = eo;
    }
    if (f->mode == (uint32_t)FD_LMS_NORM) {
      FD_U128 en = fd_u128(f->energy[0], f->energy[1]);
      en = fd_u128_add(en, fd_u128(0U, (uint64_t)fx_smull(xn, xn)));
      en = fd_u128_sub(en, fd_u128(0U, (uint64_t)fx_smull(xo, xo)));
      f->energy[0] = en.hi;
      f->energy[1] = en.lo;
      int64_t me = fx_smull(f->mu, eo);
      FD_U128 num = fd_u128_shl(
          fd_u128(0U, me < 0 ? 0U - (uint64_t)me : (uint64_t)me), 31U);
      f->step = qdiv(num, me < 0, fd_u128_add(en, fd_u128(0U, 1U)),
                     (uint32_t)INT32_MAX);
    } else if (f->mode == (uint32_t)FD_LMS_SIGN) {
      f->step = eo > 0 ? f->mu : (eo < 0 ? -f->mu : 0);
    } else {
      f->step = (int32_t)((fx_smull(f->mu, eo) + 0x40000000) >> 31);
    }
  }
}

void fd_lms_coefs_q31(FD_LmsQ31 *f, fm_q31 *h) {
  const fm_q31 *p = f->state + f->pos;
  for (uint32_t k = 0U; k < f->ntaps; k++) {
    int32_t u = upd31(f->w[k], f->step, p[-1 - (int32_t)k]);
    f->w[k] = u;
    h[k] = u * 2;
  }
  f->step = 0;
}

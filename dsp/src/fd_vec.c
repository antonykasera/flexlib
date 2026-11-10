/**
 * @file fd_vec.c
 * @brief Element-wise block arithmetic and conversions
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_vec.h"
#include "fm_exp.h"

/* four loads before any store: in-place calls still pipeline the loads */
#define FD_MAP1(T, x, y, n, f)                                                 \
  do {                                                                         \
    for (uint32_t k_ = (n) >> 2; k_ != 0U; k_--) {                             \
      T x0 = (x)[0], x1 = (x)[1], x2 = (x)[2], x3 = (x)[3];                    \
      (y)[0] = f(x0);                                                          \
      (y)[1] = f(x1);                                                          \
      (y)[2] = f(x2);                                                          \
      (y)[3] = f(x3);                                                          \
      (x) += 4;                                                                \
      (y) += 4;                                                                \
    }                                                                          \
    for (uint32_t k_ = (n) & 3U; k_ != 0U; k_--) {                             \
      T v_ = *(x)++;                                                           \
      *(y)++ = f(v_);                                                          \
    }                                                                          \
  } while (0)

#define FD_MAP2(T, a, b, y, n, f)                                              \
  do {                                                                         \
    for (uint32_t k_ = (n) >> 2; k_ != 0U; k_--) {                             \
      T a0 = (a)[0], a1 = (a)[1], a2 = (a)[2], a3 = (a)[3];                    \
      T b0 = (b)[0], b1 = (b)[1], b2 = (b)[2], b3 = (b)[3];                    \
      (y)[0] = f(a0, b0);                                                      \
      (y)[1] = f(a1, b1);                                                      \
      (y)[2] = f(a2, b2);                                                      \
      (y)[3] = f(a3, b3);                                                      \
      (a) += 4;                                                                \
      (b) += 4;                                                                \
      (y) += 4;                                                                \
    }                                                                          \
    for (uint32_t k_ = (n) & 3U; k_ != 0U; k_--) {                             \
      T a_ = *(a)++, b_ = *(b)++;                                              \
      *(y)++ = f(a_, b_);                                                      \
    }                                                                          \
  } while (0)

/* round(a b / 2^31): SMLAL from the 2^30 preload; only -1 x -1 reaches 2^30 */
FX_INLINE fm_q31 mulr31(fm_q31 a, fm_q31 b) {
  int64_t p = fx_smlal(0x40000000, a, b);
  int32_t hi = (int32_t)(p >> 32);
  int32_t r = (int32_t)(((uint32_t)hi << 1) | ((uint32_t)p >> 31));
  return hi == 0x40000000 ? INT32_MAX : r;
}

/* ---- add, sub, mul ---- */

void fd_add_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n) {
  FD_MAP2(fm_q15, a, b, y, n, fm_add_q15);
}

void fd_add_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n) {
  FD_MAP2(fm_q31, a, b, y, n, fm_add_q31);
}

void fd_sub_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n) {
  FD_MAP2(fm_q15, a, b, y, n, fm_sub_q15);
}

void fd_sub_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n) {
  FD_MAP2(fm_q31, a, b, y, n, fm_sub_q31);
}

void fd_mul_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n) {
  FD_MAP2(fm_q15, a, b, y, n, fm_mul_q15);
}

void fd_mul_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n) {
  FD_MAP2(fm_q31, a, b, y, n, mulr31);
}

/* ---- scale, shift ---- */

/* p >> r, 1 <= r <= 31, saturated: it fits when -lim <= hi < lim */
FX_INLINE fm_q31 scale_lo(int64_t p, uint32_t r, uint32_t l, int32_t lim) {
  int32_t hi = (int32_t)(p >> 32);
  if (hi >= lim) {
    return INT32_MAX;
  }
  if (hi < -lim) {
    return INT32_MIN;
  }
  return (fm_q31)(((uint32_t)hi << l) | ((uint32_t)p >> r));
}

void fd_scale_q15(const fm_q15 *x, fm_q15 k, int32_t shift, fm_q15 *y,
                  uint32_t n) {
  FX_ASSERT(shift >= -15 && shift <= 15);
  const uint32_t r = (uint32_t)(15 - shift);
  const int32_t half = r != 0U ? (int32_t)(1UL << (r - 1U)) : 0;
#define SCALE15(v) fm_sat_q15(((v) * k + half) >> r)
  FD_MAP1(fm_q15, x, y, n, SCALE15);
#undef SCALE15
}

void fd_scale_q31(const fm_q31 *x, fm_q31 k, int32_t shift, fm_q31 *y,
                  uint32_t n) {
  FX_ASSERT(shift >= -31 && shift <= 31);
  const uint32_t r = (uint32_t)(31 - shift);
  if (r == 0U) {
#define SCALE31_0(v) fm_sat_q31(fx_smull((v), k))
    FD_MAP1(fm_q31, x, y, n, SCALE31_0);
#undef SCALE31_0
  } else if (r < 32U) {
    const int64_t half = (int64_t)(1ULL << (r - 1U));
    const int32_t lim = (int32_t)(1UL << (r - 1U));
    const uint32_t l = 32U - r;
#define SCALE31_LO(v) scale_lo(fx_smlal(half, (v), k), r, l, lim)
    FD_MAP1(fm_q31, x, y, n, SCALE31_LO);
#undef SCALE31_LO
  } else {
    const int64_t half = (int64_t)(1ULL << (r - 1U));
    const uint32_t h = r - 32U;
#define SCALE31_HI(v) (fm_q31)((int32_t)(fx_smlal(half, (v), k) >> 32) >> h)
    FD_MAP1(fm_q31, x, y, n, SCALE31_HI);
#undef SCALE31_HI
  }
}

void fd_shift_q15(const fm_q15 *x, int32_t shift, fm_q15 *y, uint32_t n) {
  FX_ASSERT(shift >= -15 && shift <= 15);
  if (shift >= 0) {
    const uint32_t s = (uint32_t)shift;
#define SHL15(v) fm_sat_q15((int32_t)((uint32_t)(v) << s))
    FD_MAP1(fm_q15, x, y, n, SHL15);
#undef SHL15
  } else {
    const uint32_t s = (uint32_t)-shift;
    const int32_t half = (int32_t)(1UL << (s - 1U));
#define SHR15(v) (fm_q15)(((v) + half) >> s)
    FD_MAP1(fm_q15, x, y, n, SHR15);
#undef SHR15
  }
}

void fd_shift_q31(const fm_q31 *x, int32_t shift, fm_q31 *y, uint32_t n) {
  FX_ASSERT(shift >= -31 && shift <= 31);
  if (shift >= 0) {
    const uint32_t s = (uint32_t)shift;
    const int32_t hi = INT32_MAX >> s;
    const int32_t lo = INT32_MIN >> s;
#define SHL31(v)                                                               \
  ((v) > hi ? INT32_MAX                                                        \
            : ((v) < lo ? INT32_MIN : (int32_t)((uint32_t)(v) << s)))
    FD_MAP1(fm_q31, x, y, n, SHL31);
#undef SHL31
  } else {
    const uint32_t s = (uint32_t)-shift;
#define SHR31(v) (((v) >> s) + (((v) >> (s - 1U)) & 1))
    FD_MAP1(fm_q31, x, y, n, SHR31);
#undef SHR31
  }
}

/* ---- offset, negate, abs, clip ---- */

void fd_offset_q15(const fm_q15 *x, fm_q15 c, fm_q15 *y, uint32_t n) {
#define OFF15(v) fm_add_q15((v), c)
  FD_MAP1(fm_q15, x, y, n, OFF15);
#undef OFF15
}

void fd_offset_q31(const fm_q31 *x, fm_q31 c, fm_q31 *y, uint32_t n) {
#define OFF31(v) fm_add_q31((v), c)
  FD_MAP1(fm_q31, x, y, n, OFF31);
#undef OFF31
}

#define NEG15(v) fm_sat_q15(-(int32_t)(v))
#define NEG31(v) fm_sub_q31(0, (v))
#define ABS15(v) fm_sat_q15((v) < 0 ? -(int32_t)(v) : (int32_t)(v))
#define ABS31(v) ((v) < 0 ? fm_sub_q31(0, (v)) : (v))

void fd_negate_q15(const fm_q15 *x, fm_q15 *y, uint32_t n) {
  FD_MAP1(fm_q15, x, y, n, NEG15);
}

void fd_negate_q31(const fm_q31 *x, fm_q31 *y, uint32_t n) {
  FD_MAP1(fm_q31, x, y, n, NEG31);
}

void fd_abs_q15(const fm_q15 *x, fm_q15 *y, uint32_t n) {
  FD_MAP1(fm_q15, x, y, n, ABS15);
}

void fd_abs_q31(const fm_q31 *x, fm_q31 *y, uint32_t n) {
  FD_MAP1(fm_q31, x, y, n, ABS31);
}

#define CLIP(v) ((v) < lo ? lo : ((v) > hi ? hi : (v)))

void fd_clip_q15(const fm_q15 *x, fm_q15 lo, fm_q15 hi, fm_q15 *y, uint32_t n) {
  FX_ASSERT(lo <= hi);
  FD_MAP1(fm_q15, x, y, n, CLIP);
}

void fd_clip_q31(const fm_q31 *x, fm_q31 lo, fm_q31 hi, fm_q31 *y, uint32_t n) {
  FX_ASSERT(lo <= hi);
  FD_MAP1(fm_q31, x, y, n, CLIP);
}

/* ---- fill, copy (FX_OPAQUE: GCC would call newlib's byte-wise memcpy) ---- */

#define FD_FILL(y, n, c)                                                       \
  do {                                                                         \
    uint32_t i_ = 0U;                                                          \
    for (; i_ + 4U <= (n); i_ += 4U) {                                         \
      (y)[i_] = (c);                                                           \
      (y)[i_ + 1U] = (c);                                                      \
      (y)[i_ + 2U] = (c);                                                      \
      (y)[i_ + 3U] = (c);                                                      \
    }                                                                          \
    for (; i_ < (n); i_++) {                                                   \
      (y)[i_] = (c);                                                           \
    }                                                                          \
  } while (0)

void fd_fill_q15(fm_q15 c, fm_q15 *y, uint32_t n) {
  FX_OPAQUE(y);
  FD_FILL(y, n, c);
}

void fd_fill_q31(fm_q31 c, fm_q31 *y, uint32_t n) {
  FX_OPAQUE(y);
  FD_FILL(y, n, c);
}

#define SAME(v) (v)

void fd_copy_q15(const fm_q15 *x, fm_q15 *y, uint32_t n) {
  FX_OPAQUE(y);
  FD_MAP1(fm_q15, x, y, n, SAME);
}

void fd_copy_q31(const fm_q31 *x, fm_q31 *y, uint32_t n) {
  FX_OPAQUE(y);
  FD_MAP1(fm_q31, x, y, n, SAME);
}

/* ---- conversions ---- */

#define Q15_TO_Q31(v) (fm_q31)((uint32_t)(v) << 16)
/* v + 2^15 is a multiple of 2^16 only for even v: (v >> 1) rounds the same */
#define Q31_TO_Q15(v) fm_sat_q15((half + ((v) >> 1)) >> 15)
#define Q7_TO_Q15(v) (fm_q15)((uint32_t)(v) << 8)
#define Q15_TO_Q7(v) (int8_t)FX_SSAT(((v) + 0x80) >> 8, 8)

void fd_q15_to_q31(const fm_q15 *x, fm_q31 *y, uint32_t n) {
  FD_MAP1(fm_q15, x, y, n, Q15_TO_Q31);
}

void fd_q31_to_q15(const fm_q31 *x, fm_q15 *y, uint32_t n) {
  int32_t half = 0x4000;
  FX_OPAQUE(half); /* in a register: ADD with an ASR operand, one instruction */
  FD_MAP1(fm_q31, x, y, n, Q31_TO_Q15);
}

void fd_q7_to_q15(const int8_t *x, fm_q15 *y, uint32_t n) {
  FD_MAP1(int8_t, x, y, n, Q7_TO_Q15);
}

void fd_q15_to_q7(const fm_q15 *x, int8_t *y, uint32_t n) {
  FD_MAP1(fm_q15, x, y, n, Q15_TO_Q7);
}

void fd_q15_to_f32(const fm_q15 *x, float *y, uint32_t n) {
  FD_MAP1(fm_q15, x, y, n, fm_q15_to_f32);
}

void fd_f32_to_q15(const float *x, fm_q15 *y, uint32_t n) {
  FD_MAP1(float, x, y, n, fm_f32_to_q15);
}

void fd_q31_to_f32(const fm_q31 *x, float *y, uint32_t n) {
  FD_MAP1(fm_q31, x, y, n, fm_q31_to_f32);
}

void fd_f32_to_q31(const float *x, fm_q31 *y, uint32_t n) {
  FD_MAP1(float, x, y, n, fm_f32_to_q31);
}

/* ---- dB: fm_db_q16 reads the word as Q16.16; these move it to full scale */

#define DB_Q15_OFFSET 394566   /* 20 log10(2) 2^16 */
#define DB_Q31_OFFSET -5918491 /* -15 x 20 log10(2) 2^16 */

void fd_db_q15(const fm_q15 *x, fm_q16 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fm_q16 d = fm_db_q16(x[i]);
    y[i] = d == INT32_MIN ? d : d + DB_Q15_OFFSET;
  }
}

void fd_db_q31(const fm_q31 *x, fm_q16 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fm_q16 d = fm_db_q16(x[i]);
    y[i] = d == INT32_MIN ? d : d + DB_Q31_OFFSET;
  }
}

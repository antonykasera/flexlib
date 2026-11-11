/**
 * @file cases_fd_fft.c
 * @brief Bench cases for fd_fft.h, the transforms' kernels and candidates
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fd_fft.h"
#include "fd_fft_core.h"
#include "fd_tone.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

_Static_assert(BENCH_SCRATCH_SIZE >= 8192U, "a 1024-point q31 block");

/* the work buffer: rebuilt before every sample, since the calls are in place */
static fd_cq15 *const c15 = (fd_cq15 *)(void *)bench_scratch;
static fd_cq31 *const c31 = (fd_cq31 *)(void *)bench_scratch;
static fm_q15 *const r15 = (fm_q15 *)(void *)bench_scratch;
static fm_q31 *const r31 = (fm_q31 *)(void *)bench_scratch;
static fm_q16 db[128];
static int scratch_id;

static inline uint32_t in_u(uint32_t u) { return u; }

/* full-scale white noise, different for every sample */
static void noise(uint32_t words, uint32_t seed) {
  (void)bench_scratch_claim(&scratch_id);
  uint32_t s = 0x9E3779B9U * (seed + 1U);
  uint32_t *w = (uint32_t *)(void *)bench_scratch;
  for (uint32_t i = 0U; i < words; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    w[i] = s;
  }
}

static void tone(uint32_t n, uint32_t seed) {
  (void)bench_scratch_claim(&scratch_id);
  FD_Nco o;
  fd_nco_init(&o, seed * 0x01234567U, 0x10000000U + seed * 977U, 0);
  fd_nco_cplx_q15(&o, c15, n);
}

#define HASH(bytes) h = bench_hash(h, bench_scratch, (bytes))
#define CASE(name, call, bytes, before)                                        \
  BENCH_UNARY_DEF(name, uint32_t, in_u, int32_t, (call), HASH(bytes), before)

/* ---- candidates (bench copies where they change a kernel) ---- */

#define QP FD_FFT_QP
#define TAB FD_FFT_TAB

/* B: (cos, -sin) of a / 256 turns from a full-wave q15 table (-sin keeps 1
 * exact), in RAM: an upper bound on what a flash table buys */
static int16_t tw256[2U * 192U];

static void tw256_init(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  for (uint32_t a = 0U; a < 192U; a++) {
    uint32_t t = a * (4U * QP / 256U), r = t % QP;
    int32_t s, c;
    switch (t / QP) {
    case 0:
      s = fd_fft_tw15(TAB[r]);
      c = fd_fft_tw15(TAB[QP - r]);
      break;
    case 1:
      s = fd_fft_tw15(TAB[QP - r]);
      c = -fd_fft_tw15(TAB[r]);
      break;
    default:
      s = -fd_fft_tw15(TAB[r]);
      c = -fd_fft_tw15(TAB[QP - r]);
      break;
    }
    tw256[2U * a] = (int16_t)(a == 0U ? 32767 : c); /* a = 0: unused */
    tw256[2U * a + 1U] = (int16_t)-s;
  }
}

/* the library's r4 body, twiddles from tw256 */
static inline void r4_tab(int16_t *x, uint32_t n, uint32_t q, int32_t sh,
                          uint32_t ps) {
  const uint32_t k = (uint32_t)(15 + sh) - ps;
  const int32_t rnd = (1 << (k - 1U)) - 1;
  const int32_t one = 1 << (15U - ps);
  const uint32_t qs = 2U * q, ng = n / (4U * q), step = 64U / q;
  for (uint32_t j = 1U; j < q; j++) {
    const int16_t *t1 = tw256 + 2U * j * step, *t2 = tw256 + 4U * j * step,
                  *t3 = tw256 + 6U * j * step;
    int32_t c1 = t1[0], n1 = t1[1], c2 = t2[0], n2 = t2[1], c3 = t3[0],
            n3 = t3[1];
    int16_t *p = x + 2U * j;
    for (uint32_t g = ng; g != 0U; g--) {
      int32_t x1r = p[qs], x1i = p[qs + 1U];
      int32_t x2r = p[2U * qs], x2i = p[2U * qs + 1U];
      int32_t x3r = p[3U * qs], x3i = p[3U * qs + 1U];
      int32_t p1r = (x1r * c2 - x1i * n2) >> ps;
      int32_t p1i = (x1i * c2 + x1r * n2) >> ps;
      int32_t p2r = (x2r * c1 - x2i * n1) >> ps;
      int32_t p2i = (x2i * c1 + x2r * n1) >> ps;
      int32_t p3r = (x3r * c3 - x3i * n3) >> ps;
      int32_t p3i = (x3i * c3 + x3r * n3) >> ps;
      int32_t x0r = p[0] * one + rnd, x0i = p[1] * one + rnd;
      int32_t ar = x0r + p1r, ai = x0i + p1i, br = x0r - p1r, bi = x0i - p1i;
      int32_t cr = p2r + p3r, ci = p2i + p3i, dr = p2r - p3r, di = p2i - p3i;
      p[0] = (int16_t)((ar + cr) >> k);
      p[1] = (int16_t)((ai + ci) >> k);
      p[qs] = (int16_t)((br + di) >> k);
      p[qs + 1U] = (int16_t)((bi - dr) >> k);
      p[2U * qs] = (int16_t)((ar - cr) >> k);
      p[2U * qs + 1U] = (int16_t)((ai - ci) >> k);
      p[3U * qs] = (int16_t)((br - di) >> k);
      p[3U * qs + 1U] = (int16_t)((bi + dr) >> k);
      p += 4U * qs;
    }
  }
}

BENCH_CALL static void r4_tab_n(int16_t *x, uint32_t n, uint32_t q,
                                int32_t sh) {
  r4_tab(x, n, q, sh, 0U);
}

BENCH_CALL static void r4_tab_w(int16_t *x, uint32_t n, uint32_t q,
                                int32_t sh) {
  r4_tab(x, n, q, sh, 2U);
}

/* the library driver, its twiddle stages swapped for the table ones */
BENCH_CALL static int32_t cfft_b(fd_cq15 *xc, uint32_t n) {
  int16_t *x = (int16_t *)(void *)xc;
  uint32_t b = fd_fft15_scan(x, n);
  fd_fft15_rev(x, n);
  int32_t e = 0;
  for (uint32_t q = 1U; 4U * q <= n; q *= 4U) {
    int32_t sh = (int32_t)b - (q == 1U ? 13 : 12);
    if (sh > 0) {
      fd_fft15_t4r(x, n, q, sh);
    } else {
      fd_fft15_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (b <= 13U) {
        r4_tab_n(x, n, q, sh);
      } else {
        r4_tab_w(x, n, q, sh);
      }
    }
    e += sh;
    b = fd_fft15_scan(x, n);
  }
  return e;
}

/* S2: a scan before every other stage; in between, b from the bound (every
 * stage's output is below 2^15) */
BENCH_CALL static int32_t cfft_s2(fd_cq15 *xc, uint32_t n) {
  int16_t *x = (int16_t *)(void *)xc;
  uint32_t b = fd_fft15_scan(x, n);
  fd_fft15_rev(x, n);
  int32_t e = 0;
  uint32_t i = 0U;
  for (uint32_t q = 1U; 4U * q <= n; q *= 4U, i++) {
    int32_t sh = (int32_t)b - (q == 1U ? 13 : 12);
    if (sh > 0) {
      fd_fft15_t4r(x, n, q, sh);
    } else {
      fd_fft15_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (b <= 13U) {
        fd_fft15_r4(x, n, q, sh);
      } else {
        fd_fft15_r4w(x, n, q, sh);
      }
    }
    e += sh;
    b = (i & 1U) != 0U ? fd_fft15_scan(x, n) : 15U;
  }
  return e;
}

/* S3: the exact maximum, and the smallest shift its bound allows */
static uint32_t absmax15(const int16_t *x, uint32_t n) {
  uint32_t m = 0U;
  for (uint32_t i = 0U; i < 2U * n; i++) {
    int32_t v = x[i];
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    m = a > m ? a : m;
  }
  return m;
}

/* smallest sh with g / 2^16 x m / 2^sh <= 32767 */
static int32_t shift_for(uint32_t m, uint32_t g) {
  const uint64_t lim = (uint64_t)32767U << 16;
  const uint64_t v = (uint64_t)m * g;
  int32_t sh = -12;
  while (sh<0 ? (v << (uint32_t)-sh) > lim : v>(lim << (uint32_t)sh)) {
    sh++;
  }
  return sh;
}

BENCH_CALL static int32_t cfft_s3(fd_cq15 *xc, uint32_t n) {
  int16_t *x = (int16_t *)(void *)xc;
  uint32_t m = absmax15(x, n);
  fd_fft15_rev(x, n);
  int32_t e = 0;
  for (uint32_t q = 1U; 4U * q <= n; q *= 4U) {
    /* growth 4 and 5.2431 in Q16 */
    int32_t sh = shift_for(m, q == 1U ? 262144U : 343611U);
    if (sh > 0) {
      fd_fft15_t4r(x, n, q, sh);
    } else {
      fd_fft15_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (m < 8192U) {
        fd_fft15_r4(x, n, q, sh);
      } else {
        fd_fft15_r4w(x, n, q, sh);
      }
    }
    e += sh;
    m = absmax15(x, n);
  }
  return e;
}

/* V2: the swaps from a table of pairs (CMSIS's way), n = 256 */
static uint8_t pairs256[240];
static uint32_t npairs256;

static void pairs_init(void) {
  if (npairs256 != 0U) {
    return;
  }
  for (uint32_t i = 0U; i < 256U; i++) {
    uint32_t r = fx_rbit(i) >> 24;
    if (i < r) {
      pairs256[npairs256++] = (uint8_t)i;
      pairs256[npairs256++] = (uint8_t)r;
    }
  }
}

BENCH_CALL static void rev_pairs(fd_cq15 *x) {
  uint32_t *w = (uint32_t *)(void *)x;
  for (uint32_t i = 0U; i < npairs256; i += 2U) {
    uint32_t a = pairs256[i], b = pairs256[i + 1U];
    uint32_t t = w[a];
    w[a] = w[b];
    w[b] = t;
  }
}

/* R2: radix-2 stages, twiddle-major, block floating point (growth 1 +
 * sqrt 2: sh = b - 13) */
BENCH_CALL static void r2_stage(int16_t *x, uint32_t n, uint32_t h,
                                int32_t sh) {
  const uint32_t k = (uint32_t)(15 + sh);
  const int32_t rnd = (1 << (k - 1U)) - 1;
  const uint32_t step = 2U * QP / h;
  for (uint32_t j = 0U; j < h; j++) {
    uint32_t a = j * step;
    int32_t s, c;
    if (a <= QP) {
      s = fd_fft_tw15(TAB[a]);
      c = fd_fft_tw15(TAB[QP - a]);
    } else {
      s = fd_fft_tw15(TAB[2U * QP - a]);
      c = -fd_fft_tw15(TAB[a - QP]);
    }
    int16_t *p = x + 2U * j;
    for (uint32_t g = n / (2U * h); g != 0U; g--) {
      int32_t yr = p[2U * h], yi = p[2U * h + 1U];
      int32_t pr = yr * c + yi * s, pi = yi * c - yr * s;
      int32_t x0r = p[0] * 32768 + rnd, x0i = p[1] * 32768 + rnd;
      p[0] = (int16_t)((x0r + pr) >> k);
      p[1] = (int16_t)((x0i + pi) >> k);
      p[2U * h] = (int16_t)((x0r - pr) >> k);
      p[2U * h + 1U] = (int16_t)((x0i - pi) >> k);
      p += 4U * h;
    }
  }
}

BENCH_CALL static int32_t cfft_r2(fd_cq15 *xc, uint32_t n) {
  int16_t *x = (int16_t *)(void *)xc;
  uint32_t b = fd_fft15_scan(x, n);
  fd_fft15_rev(x, n);
  int32_t sh = (int32_t)b - 14;
  if (sh > 0) {
    fd_fft15_t2r(x, n, sh);
  } else {
    fd_fft15_t2l(x, n, sh);
  }
  int32_t e = sh;
  for (uint32_t h = 2U; h < n; h *= 2U) {
    b = fd_fft15_scan(x, n);
    sh = (int32_t)b - 13;
    r2_stage(x, n, h, sh);
    e += sh;
  }
  return e;
}

/* R: each q31 product rounded to its high word, 32-bit sums (4 roundings per
 * output, not 1); timed at sh = 3, where no twiddle reaches 2^31 */
static inline int32_t rhi(int32_t a, int32_t b, int32_t c, int32_t d) {
  return (int32_t)(fx_smlal(fx_smlal(0x80000000LL, a, b), c, d) >> 32);
}

/* cos, sin at a / (4 QP) turns over 2^ts, magnitudes rounded */
static inline void tw31r(uint32_t a, uint32_t ts, int32_t *c, int32_t *s) {
  const uint32_t h = 1U << (ts - 1U);
  uint32_t vs, vc;
  int32_t ss = 1, sc = 1;
  if (a <= QP) {
    vs = TAB[a];
    vc = TAB[QP - a];
  } else if (a <= 2U * QP) {
    vs = TAB[2U * QP - a];
    vc = TAB[a - QP];
    sc = -1;
  } else {
    vs = TAB[a - 2U * QP];
    vc = TAB[3U * QP - a];
    ss = sc = -1;
  }
  *s = ss * (int32_t)((vs + h) >> ts);
  *c = sc * (int32_t)((vc + h) >> ts);
}

BENCH_CALL static void r4_round(int32_t *x, uint32_t n, uint32_t q,
                                int32_t sh) {
  const uint32_t ts = (uint32_t)sh - 1U, s0 = (uint32_t)sh;
  const int32_t half = (1 << (s0 - 1U)) - 1; /* x0 rounded, ties down */
  const uint32_t qs = 2U * q, ng = n / (4U * q), step = QP / q;
  for (uint32_t j = 1U; j < q; j++) {
    int32_t c1, s1, c2, s2, c3, s3;
    tw31r(j * step, ts, &c1, &s1);
    tw31r(2U * j * step, ts, &c2, &s2);
    tw31r(3U * j * step, ts, &c3, &s3);
    int32_t *p = x + 2U * j;
    for (uint32_t g = ng; g != 0U; g--) {
      int32_t x1r = p[qs], x1i = p[qs + 1U];
      int32_t x2r = p[2U * qs], x2i = p[2U * qs + 1U];
      int32_t x3r = p[3U * qs], x3i = p[3U * qs + 1U];
      int32_t p1r = rhi(x1r, c2, x1i, s2), p1i = rhi(x1i, c2, x1r, -s2);
      int32_t p2r = rhi(x2r, c1, x2i, s1), p2i = rhi(x2i, c1, x2r, -s1);
      int32_t p3r = rhi(x3r, c3, x3i, s3), p3i = rhi(x3i, c3, x3r, -s3);
      int32_t x0r = (p[0] + half) >> s0, x0i = (p[1] + half) >> s0;
      int32_t ar = x0r + p1r, ai = x0i + p1i, br = x0r - p1r, bi = x0i - p1i;
      int32_t cr = p2r + p3r, ci = p2i + p3i, dr = p2r - p3r, di = p2i - p3i;
      p[0] = ar + cr;
      p[1] = ai + ci;
      p[qs] = br + di;
      p[qs + 1U] = bi - dr;
      p[2U * qs] = ar - cr;
      p[2U * qs + 1U] = ai - ci;
      p[3U * qs] = br - di;
      p[3U * qs + 1U] = bi + dr;
      p += 4U * qs;
    }
  }
}

/* ---- cases ---- */

#define N15(n) noise((n), i)
#define N31(n) noise(2U * (n), i)

CASE(b_cfft15_64, fd_cfft_q15(c15, 64U), 256U, N15(64U))
CASE(b_cfft15_256, fd_cfft_q15(c15, 256U), 1024U, N15(256U))
CASE(b_cfft15_1024, fd_cfft_q15(c15, 1024U), 4096U, N15(1024U))
CASE(b_cfft15_tone, fd_cfft_q15(c15, 256U), 1024U, tone(256U, i))
CASE(b_icfft15_256, fd_icfft_q15(c15, 256U), 1024U, N15(256U))
CASE(b_cfft31_256, fd_cfft_q31(c31, 256U), 2048U, N31(256U))
CASE(b_cfft31_1024, fd_cfft_q31(c31, 1024U), 8192U, N31(1024U))
CASE(b_rfft15_512, fd_rfft_q15(r15, 512U), 1024U, N15(256U))
CASE(b_irfft15_512, fd_irfft_q15(r15, 512U), 1024U, N15(256U))
CASE(b_rfft31_512, fd_rfft_q31(r31, 512U), 2048U, N31(256U))
CASE(b_db15_128, (fd_fft_db_q15(c15, -3, db, 128U), db[5]), 0U, N15(128U))

/* the kernels alone, n = 256 */
CASE(b_scan15, (int32_t)fd_fft15_scan(&c15[0].re, 256U), 0U, N15(256U))
CASE(b_rev15, (fd_fft15_rev(&c15[0].re, 256U), 0), 1024U, N15(256U))
CASE(b_t4r15, (fd_fft15_t4r(&c15[0].re, 256U, 1U, 2), 0), 1024U,
     noise(256U, i ^ 0x55U))
CASE(b_r4w15, (fd_fft15_r4w(&c15[0].re, 256U, 16U, 3), 0), 1024U,
     noise(256U, i ^ 0x77U))
CASE(b_scan31, (int32_t)fd_fft31_scan(&c31[0].re, 256U), 0U, N31(256U))
CASE(b_r4_31, (fd_fft31_r4(&c31[0].re, 256U, 16U, 3), 0), 2048U,
     noise(512U, i ^ 0x77U))

/* candidates, n = 256 */
CASE(b_cand_b, cfft_b(c15, 256U), 1024U, (tw256_init(), N15(256U)))
CASE(b_cand_s2, cfft_s2(c15, 256U), 1024U, N15(256U))
CASE(b_cand_s3, cfft_s3(c15, 256U), 1024U, N15(256U))
CASE(b_cand_r2, cfft_r2(c15, 256U), 1024U, N15(256U))
CASE(b_cand_v2, (rev_pairs(c15), 0), 1024U, (pairs_init(), N15(256U)))
CASE(b_cand_r31, (r4_round(&c31[0].re, 256U, 16U, 3), 0), 2048U,
     noise(512U, i ^ 0x77U))

#ifdef BENCH_HAVE_CMSIS_DSP
static arm_rfft_instance_q15 rfft512;
static void cmsis_rfft_init(void) {
  static bool done;
  if (!done) {
    done = true;
    (void)arm_rfft_init_512_q15(&rfft512, 0U, 1U);
  }
}
CASE(c_cfft15_256, (arm_cfft_q15(&arm_cfft_sR_q15_len256, r15, 0U, 1U), 0), 0U,
     N15(256U))
CASE(c_cfft15_1024, (arm_cfft_q15(&arm_cfft_sR_q15_len1024, r15, 0U, 1U), 0),
     0U, N15(1024U))
CASE(c_cfft31_256, (arm_cfft_q31(&arm_cfft_sR_q31_len256, r31, 0U, 1U), 0), 0U,
     N31(256U))
CASE(c_cfft31_1024, (arm_cfft_q31(&arm_cfft_sR_q31_len1024, r31, 0U, 1U), 0),
     0U, N31(1024U))
CASE(c_rfft15_512, (arm_rfft_q15(&rfft512, r15, r15 + 1024U), 0), 0U,
     (cmsis_rfft_init(), N15(256U)))
#endif

static const BenchCase cases[] = {
    {"fd_cfft_q15, 64", "flexlib", b_cfft15_64, true},
    {"fd_cfft_q15, 256", "flexlib", b_cfft15_256, true},
    {"fd_cfft_q15, 1024", "flexlib", b_cfft15_1024, true},
    {"fd_cfft_q15, 256, tone", "flexlib", b_cfft15_tone, true},
    {"fd_icfft_q15, 256", "flexlib", b_icfft15_256, true},
    {"fd_cfft_q31, 256", "flexlib", b_cfft31_256, true},
    {"fd_cfft_q31, 1024", "flexlib", b_cfft31_1024, true},
    {"fd_rfft_q15, 512", "flexlib", b_rfft15_512, true},
    {"fd_irfft_q15, 512", "flexlib", b_irfft15_512, true},
    {"fd_rfft_q31, 512", "flexlib", b_rfft31_512, true},
    {"fd_fft_db_q15, 128 bins", "flexlib", b_db15_128, true},
    {"fft q15 scan, 256", "kernel", b_scan15, true},
    {"fft q15 bit reversal, 256", "kernel", b_rev15, true},
    {"fft q15 stage q = 1, 256", "kernel", b_t4r15, true},
    {"fft q15 twiddle stage q = 16 (b 15), 256", "kernel", b_r4w15, true},
    {"fft q31 scan, 256", "kernel", b_scan31, true},
    {"fft q31 twiddle stage q = 16 (sh 3), 256", "kernel", b_r4_31, true},
    {"cfft q15 256, full-wave twiddle table in RAM", "candidate", b_cand_b,
     true},
    {"cfft q15 256, scan every other stage", "candidate", b_cand_s2, true},
    {"cfft q15 256, exact maximum", "candidate", b_cand_s3, true},
    {"cfft q15 256, radix-2", "candidate", b_cand_r2, true},
    {"bit reversal 256 from a pair table", "candidate", b_cand_v2, true},
    {"q31 twiddle stage q = 16 (sh 3), products rounded", "candidate",
     b_cand_r31, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_cfft_q15, 256", "CMSIS-DSP", c_cfft15_256, false},
    {"arm_cfft_q15, 1024", "CMSIS-DSP", c_cfft15_1024, false},
    {"arm_cfft_q31, 256", "CMSIS-DSP", c_cfft31_256, false},
    {"arm_cfft_q31, 1024", "CMSIS-DSP", c_cfft31_1024, false},
    {"arm_rfft_q15, 512", "CMSIS-DSP", c_rfft15_512, false},
#endif
};

const BenchGroup bench_group_fd_fft = {cases, BENCH_COUNT(cases)};

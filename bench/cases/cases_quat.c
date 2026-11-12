/**
 * @file cases_quat.c
 * @brief Bench cases for fm_quat.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fm_internal.h"
#include "fm_quat.h"
#include <math.h>
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

/* ---- data: unit quaternions, vectors and rates from fixed pools ---- */

#define NQ 64U
static FM_Quat qpool[NQ];
static float vpool[NQ + 2U];
static FM_Quat qout;
static float vout[3];

static void pools_init(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x2545F491U;
  float f[4];
  for (unsigned i = 0U; i < NQ + 2U; i++) {
    for (unsigned k = 0U; k < 4U; k++) {
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      f[k] = (float)(int32_t)s * 4.65661287e-10f; /* +-1 */
    }
    if (i < NQ) {
      FM_Quat q = {f[0], f[1], f[2], f[3]};
      fm_quat_normalize(&qpool[i], &q);
    }
    vpool[i] = f[0] * 8.0f; /* rad/s for the rates */
  }
}

static inline uint32_t in_q(uint32_t u) {
  pools_init();
  return u % NQ;
}

#define QA (&qpool[x])
#define QB (&qpool[NQ - 1U - x])
#define VEC (&vpool[x])
#define DT 0.001f
#define QOUT h = bench_hash(h, &qout, sizeof qout)
#define VOUT h = bench_hash(h, vout, sizeof vout)

/* ---- candidate: first-order integration, q (x) (1, w dt / 2), normalised ----
 */

static inline uint32_t mulf(uint32_t x, uint32_t y, int32_t d) {
  int32_t ex;
  int32_t ey;
  uint32_t bad = 0U;
  int32_t mx = fm_unp_f32(x, 7U, &ex, &bad);
  int32_t my = fm_unp_f32(y, 6U, &ey, &bad);
  ey += d;
  return fm_sum_f32(&mx, &ex, &my, &ey, 1U, 1U);
}

BENCH_CALL static void integrate_first(FM_Quat *r, const FM_Quat *q,
                                       const float *w, float dt) {
  uint32_t b[3];
  uint32_t t;
  memcpy(b, w, sizeof b);
  memcpy(&t, &dt, sizeof t);
  uint32_t d[4] = {0x3F800000U, mulf(b[0], t, -1), mulf(b[1], t, -1),
                   mulf(b[2], t, -1)};
  FM_Quat dq;
  memcpy(&dq, d, sizeof d);
  fm_quat_mul(r, q, &dq);
  fm_quat_normalize(r, r);
}

/* ---- plain float (libgcc soft-float, libm) ---- */

BENCH_CALL static void mul_plain(FM_Quat *r, const FM_Quat *a,
                                 const FM_Quat *b) {
  FM_Quat t;
  t.w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
  t.x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
  t.y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
  t.z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
  *r = t;
}

BENCH_CALL static void normalize_plain(FM_Quat *r, const FM_Quat *q) {
  float s = 1.0f / sqrtf(q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
  r->w = q->w * s;
  r->x = q->x * s;
  r->y = q->y * s;
  r->z = q->z * s;
}

BENCH_CALL static void rotate_plain(float *r, const FM_Quat *q,
                                    const float *v) {
  float tx = 2.0f * (q->y * v[2] - q->z * v[1]);
  float ty = 2.0f * (q->z * v[0] - q->x * v[2]);
  float tz = 2.0f * (q->x * v[1] - q->y * v[0]);
  r[0] = v[0] + q->w * tx + (q->y * tz - q->z * ty);
  r[1] = v[1] + q->w * ty + (q->z * tx - q->x * tz);
  r[2] = v[2] + q->w * tz + (q->x * ty - q->y * tx);
}

BENCH_CALL static void integrate_plain(FM_Quat *r, const FM_Quat *q,
                                       const float *w, float dt) {
  float n = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
  float th = n * dt * 0.5f;
  float k = sinf(th) / n;
  FM_Quat dq = {cosf(th), k * w[0], k * w[1], k * w[2]};
  mul_plain(r, q, &dq);
  normalize_plain(r, r);
}

#ifdef BENCH_HAVE_CMSIS_DSP
BENCH_CALL static void mul_cmsis(FM_Quat *r, const FM_Quat *a,
                                 const FM_Quat *b) {
  arm_quaternion_product_single_f32(&a->w, &b->w, &r->w);
}

BENCH_CALL static void normalize_cmsis(FM_Quat *r, const FM_Quat *q) {
  arm_quaternion_normalize_f32(&q->w, &r->w, 1U);
}
#endif

/* ---- cases ---- */

BENCH_UNARY_POST(k_mul, uint32_t, in_q, uint32_t,
                 (fm_quat_mul(&qout, QA, QB), 0U), QOUT)
BENCH_UNARY_POST(k_mul_plain, uint32_t, in_q, uint32_t,
                 (mul_plain(&qout, QA, QB), 0U), QOUT)
BENCH_UNARY_POST(k_norm, uint32_t, in_q, uint32_t,
                 (fm_quat_normalize(&qout, QA), 0U), QOUT)
BENCH_UNARY_POST(k_norm_plain, uint32_t, in_q, uint32_t,
                 (normalize_plain(&qout, QA), 0U), QOUT)
BENCH_UNARY_POST(k_rot, uint32_t, in_q, uint32_t,
                 (fm_quat_rotate(vout, QA, VEC), 0U), VOUT)
BENCH_UNARY_POST(k_rot_plain, uint32_t, in_q, uint32_t,
                 (rotate_plain(vout, QA, VEC), 0U), VOUT)
BENCH_UNARY_POST(k_int, uint32_t, in_q, uint32_t,
                 (fm_quat_integrate(&qout, QA, VEC, DT), 0U), QOUT)
BENCH_UNARY_POST(k_int_first, uint32_t, in_q, uint32_t,
                 (integrate_first(&qout, QA, VEC, DT), 0U), QOUT)
BENCH_UNARY_POST(k_int_plain, uint32_t, in_q, uint32_t,
                 (integrate_plain(&qout, QA, VEC, DT), 0U), QOUT)
BENCH_UNARY_DEF(k_int_cold, uint32_t, in_q, uint32_t,
                (fm_quat_integrate(&qout, QA, VEC, DT), 0U), QOUT,
                bench_cold_flush())
#ifdef BENCH_HAVE_CMSIS_DSP
BENCH_UNARY_POST(k_mul_cmsis, uint32_t, in_q, uint32_t,
                 (mul_cmsis(&qout, QA, QB), 0U), QOUT)
BENCH_UNARY_POST(k_norm_cmsis, uint32_t, in_q, uint32_t,
                 (normalize_cmsis(&qout, QA), 0U), QOUT)
#endif

static const BenchCase cases[] = {
    {"fm_quat_mul", "flexmath", k_mul, true},
    {"quat mul, float", "libgcc", k_mul_plain, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_quaternion_product_single_f32", "CMSIS-DSP", k_mul_cmsis, false},
#endif
    {"fm_quat_normalize", "flexmath", k_norm, true},
    {"quat normalize, 1 / sqrtf and 4 multiplies", "libm", k_norm_plain, false},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_quaternion_normalize_f32, one", "CMSIS-DSP", k_norm_cmsis, false},
#endif
    {"fm_quat_rotate", "flexmath", k_rot, true},
    {"quat rotate, float", "libgcc", k_rot_plain, true},
    {"fm_quat_integrate", "flexmath", k_int, true},
    {"quat integrate, first order + normalise", "candidate", k_int_first, true},
    {"quat integrate, float with sqrtf / sinf / cosf", "libm", k_int_plain,
     false},
};

const BenchGroup bench_group_quat = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_quat_integrate", "flexmath", k_int_cold, true},
};

const BenchGroup bench_group_quat_cold = {cold, BENCH_COUNT(cold)};

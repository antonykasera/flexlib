/**
 * @file cases_pid.c
 * @brief Bench cases for fm_pid.h, with a saturating Q16.16 candidate, the
 *        same controller in plain float (libgcc), and CMSIS-DSP's PIDs
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_pid.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

/* Each sample starts from a state made from its own input (set untimed, in
 * the case's `before`), so the warm-up run and the host's give the same
 * hashes. Gains kp 1.5, ki 0.05, kd 0.4; output within +-100. */
static FM_PidQ16 pq;
static FM_PidF32 pf;

static void prep_q16(unsigned i) {
  fm_pid_init_q16(&pq, 98304, 3277, 26214, -100 * FM_Q16_ONE,
                  100 * FM_Q16_ONE);
  pq.i = (int64_t)(int32_t)bench_in[i] << 6; /* +-2^37: up to +-2 */
  pq.prev = (fm_q16)bench_in[(i + 2U) % BENCH_N] >> 9;
}

static void prep_f32(unsigned i) {
  fm_pid_init_f32(&pf, 1.5f, 0.05f, 0.4f, -100.0f, 100.0f);
  pf.i = (float)(int32_t)bench_in[i] * 0x1p-30f;
  pf.prev = (float)(int32_t)bench_in[(i + 2U) % BENCH_N] * 0x1p-25f;
}

/* +-64 */
static inline fm_q16 in_q16(uint32_t u) { return (fm_q16)u >> 9; }
static inline float in_f32(uint32_t u) {
  return (float)(int32_t)u * 0x1p-25f;
}

/* ---- candidate: e and d as saturated int32 differences ---- */

static inline int64_t sat32(int64_t v) {
  return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : v);
}

static inline int64_t clamp64(int64_t v, int64_t lo, int64_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

BENCH_CALL static fm_q16 pid_q16_sat(FM_PidQ16 *p, fm_q16 sp, fm_q16 pv) {
  int64_t lo = (int64_t)p->out_min * 65536;
  int64_t hi = (int64_t)p->out_max * 65536;
  int32_t e = (int32_t)sat32((int64_t)sp - pv);
  int32_t d = (int32_t)sat32((int64_t)p->prev - pv);
  int64_t i = clamp64(p->i + (int64_t)p->ki * e, lo, hi);
  p->i = i;
  p->prev = pv;
  int64_t o = i + (int64_t)p->kp * e + (int64_t)p->kd * d;
  return (fm_q16)((clamp64(o, lo, hi) + 32768) >> 16);
}

/* ---- plain float, the same controller ---- */

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

BENCH_CALL static float pid_f32_plain(FM_PidF32 *p, float sp, float pv) {
  float e = sp - pv;
  p->i = clampf(p->i + p->ki * e, p->out_min, p->out_max);
  float o = p->kp * e + p->i + p->kd * (p->prev - pv);
  p->prev = pv;
  return clampf(o, p->out_min, p->out_max);
}

#ifdef BENCH_HAVE_CMSIS_DSP
static arm_pid_instance_q31 cq;
static arm_pid_instance_f32 cf;

static void prep_cmsis(unsigned i) {
  (void)i;
  cq.Kp = 0x40000000; /* 0.5, 0.05, 0.2: q31 gains are below 1 */
  cq.Ki = 0x06666666;
  cq.Kd = 0x1999999A;
  arm_pid_init_q31(&cq, 1);
  cf.Kp = 1.5f;
  cf.Ki = 0.05f;
  cf.Kd = 0.4f;
  arm_pid_init_f32(&cf, 1);
}

BENCH_CALL static q31_t pid_cmsis_q31(q31_t x) { return arm_pid_q31(&cq, x); }
BENCH_CALL static float pid_cmsis_f32(float x) { return arm_pid_f32(&cf, x); }
#endif

/* ---- cases ---- */

BENCH_BINARY_DEF(p_q16, fm_q16, in_q16, fm_q16, in_q16, fm_q16,
                 fm_pid_q16(&pq, a, b), prep_q16(i))
BENCH_BINARY_DEF(p_q16_sat, fm_q16, in_q16, fm_q16, in_q16, fm_q16,
                 pid_q16_sat(&pq, a, b), prep_q16(i))
BENCH_BINARY_DEF(p_f32, float, in_f32, float, in_f32, float,
                 fm_pid_f32(&pf, a, b), prep_f32(i))
BENCH_BINARY_DEF(p_f32_plain, float, in_f32, float, in_f32, float,
                 pid_f32_plain(&pf, a, b), prep_f32(i))
#ifdef BENCH_HAVE_CMSIS_DSP
BENCH_UNARY_DEF(p_cmsis_q31, q31_t, (q31_t), q31_t, pid_cmsis_q31(x), (void)0,
                prep_cmsis(i))
BENCH_UNARY_DEF(p_cmsis_f32, float, in_f32, float, pid_cmsis_f32(x), (void)0,
                prep_cmsis(i))
#endif

static const BenchCase cases[] = {
    {"fm_pid_q16", "flexmath", p_q16, true},
    {"pid q16, e and d saturated to int32", "candidate", p_q16_sat, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_pid_q31 (no clamps, no anti-windup)", "CMSIS-DSP", p_cmsis_q31,
     false},
#endif
    {"fm_pid_f32", "flexmath", p_f32, true},
    {"pid f32, the same in plain float", "libgcc", p_f32_plain, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_pid_f32 (no clamps, no anti-windup)", "CMSIS-DSP", p_cmsis_f32,
     false},
#endif
};

const BenchGroup bench_group_pid = {cases, BENCH_COUNT(cases)};

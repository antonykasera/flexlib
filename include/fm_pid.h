/**
 * @file fm_pid.h
 * @brief PID controllers in Q16.16 and float: anti-windup, output clamping,
 *        derivative on measurement
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Each step: e = sp - pv, I = clamp(I + ki e), out = clamp(kp e + I + kd (prev
 * - pv)), prev = pv, with clamp to [out_min, out_max]. The gains are per step:
 * ki = Ki dt, kd = Kd / dt. Clamping the integrator to the output range is the
 * anti-windup; the derivative of the measurement alone means a setpoint step
 * gives no kick. Reset with the first measurement so the first step has none
 * either.
 */

#ifndef FM_PID_H
#define FM_PID_H

#include "fm_fixed.h"
#include <stdint.h>

typedef struct {
  fm_q16 kp, ki, kd;
  fm_q16 out_min, out_max;
  int64_t i; /* the integrator, Q32 (exact) */
  fm_q16 prev;
} FM_PidQ16;

typedef struct {
  float kp, ki, kd;
  float out_min, out_max;
  float i;
  float prev;
} FM_PidF32;

/* out_min <= out_max; the state starts at zero */
void fm_pid_init_q16(FM_PidQ16 *p, fm_q16 kp, fm_q16 ki, fm_q16 kd,
                     fm_q16 out_min, fm_q16 out_max);
void fm_pid_reset_q16(FM_PidQ16 *p, fm_q16 pv);

/* every product and sum exact, the output rounded once (nearest, ties up), for
 * every input */
fm_q16 fm_pid_q16(FM_PidQ16 *p, fm_q16 sp, fm_q16 pv);

void fm_pid_init_f32(FM_PidF32 *p, float kp, float ki, float kd, float out_min,
                     float out_max);
void fm_pid_reset_f32(FM_PidF32 *p, float pv);

/* the integrator update and the output are each one exact sum of products,
 * rounded once; a NaN or inf input gives NaN and leaves the state as it was */
float fm_pid_f32(FM_PidF32 *p, float sp, float pv);

#endif /* FM_PID_H */

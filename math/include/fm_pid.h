/**
 * @file fm_pid.h
 * @brief PID controllers in Q16.16 and float with anti-windup
 *
 * Gains are per step: ki = Ki dt, kd = Kd / dt. The derivative acts on the
 * measurement, the output is clamped to [out_min, out_max], and while it is
 * clamped the integrator stops at the limit.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_PID_H
#define FM_PID_H

#include "fm_fixed.h"
#include <stdint.h>

/** Q16.16 controller; opaque. */
typedef struct {
  /** @privatesection */
  fm_q16 kp, ki, kd;
  fm_q16 out_min, out_max;
  int64_t i; /* integrator, Q32 */
  fm_q16 prev;
} FM_PidQ16;

/** float controller; opaque. */
typedef struct {
  /** @privatesection */
  float kp, ki, kd;
  float out_min, out_max;
  float i;
  float prev;
} FM_PidF32;

/** Set up with zero state; out_min <= out_max. */
void fm_pid_init_q16(FM_PidQ16 *p, fm_q16 kp, fm_q16 ki, fm_q16 kd,
                     fm_q16 out_min, fm_q16 out_max);
/** Clear the integrator and take pv as the last measurement. */
void fm_pid_reset_q16(FM_PidQ16 *p, fm_q16 pv);

/** One step towards setpoint sp from measurement pv: exact sums, the output
 * rounded once (ties up). */
fm_q16 fm_pid_q16(FM_PidQ16 *p, fm_q16 sp, fm_q16 pv);

/** Set up with zero state; out_min <= out_max. */
void fm_pid_init_f32(FM_PidF32 *p, float kp, float ki, float kd, float out_min,
                     float out_max);
/** Clear the integrator and take pv as the last measurement. */
void fm_pid_reset_f32(FM_PidF32 *p, float pv);

/** One step towards setpoint sp from measurement pv: integrator and output
 * each within 0.5 ULP + 2^-32 sum of their |terms|. A NaN or inf input gives
 * NaN and leaves the state unchanged. */
float fm_pid_f32(FM_PidF32 *p, float sp, float pv);

#endif /* FM_PID_H */

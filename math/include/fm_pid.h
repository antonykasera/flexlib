/**
 * @file fm_pid.h
 * @brief PID controllers in Q16.16 and float with anti-windup
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_PID_H
#define FM_PID_H

#include "fm_fixed.h"
#include <stdint.h>

/* gains per step: ki = Ki dt, kd = Kd / dt; derivative on measurement */
typedef struct {
  fm_q16 kp, ki, kd;
  fm_q16 out_min, out_max;
  int64_t i; /* integrator, Q32 */
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

/* exact sums, output rounded once (ties up) */
fm_q16 fm_pid_q16(FM_PidQ16 *p, fm_q16 sp, fm_q16 pv);

void fm_pid_init_f32(FM_PidF32 *p, float kp, float ki, float kd, float out_min,
                     float out_max);
void fm_pid_reset_f32(FM_PidF32 *p, float pv);

/* NaN or inf input gives NaN and leaves the state unchanged */
float fm_pid_f32(FM_PidF32 *p, float sp, float pv);

#endif /* FM_PID_H */

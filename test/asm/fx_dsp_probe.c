/**
 * @file fx_dsp_probe.c
 * @brief Out-of-line copies of the fx_arch.h packed-pair wrappers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fx_arch.h"

int32_t probe_pkhbt(int32_t a, int32_t b);
int32_t probe_pkhtb(int32_t a, int32_t b);
int32_t probe_smlad(int32_t acc, int32_t a, int32_t b);
int64_t probe_smlald(int64_t acc, int32_t a, int32_t b);
int32_t probe_smuad(int32_t a, int32_t b);
int32_t probe_smusdx(int32_t a, int32_t b);
int32_t probe_qadd16(int32_t a, int32_t b);
int32_t probe_qsub16(int32_t a, int32_t b);
int32_t probe_shadd16(int32_t a, int32_t b);
int32_t probe_sxtb16(int32_t x);
int32_t probe_ssat16x8(int32_t x);
int32_t probe_smmulr(int32_t a, int32_t b);
int32_t probe_smulwb(int32_t a, int32_t b);
int32_t probe_smlawb(int32_t acc, int32_t a, int32_t b);

int32_t probe_pkhbt(int32_t a, int32_t b) { return fx_pkhbt(a, b); }
int32_t probe_pkhtb(int32_t a, int32_t b) { return fx_pkhtb(a, b); }
int32_t probe_smlad(int32_t acc, int32_t a, int32_t b) {
  return fx_smlad(acc, a, b);
}
int64_t probe_smlald(int64_t acc, int32_t a, int32_t b) {
  return fx_smlald(acc, a, b);
}
int32_t probe_smuad(int32_t a, int32_t b) { return fx_smuad(a, b); }
int32_t probe_smusdx(int32_t a, int32_t b) { return fx_smusdx(a, b); }
int32_t probe_qadd16(int32_t a, int32_t b) { return fx_qadd16(a, b); }
int32_t probe_qsub16(int32_t a, int32_t b) { return fx_qsub16(a, b); }
int32_t probe_shadd16(int32_t a, int32_t b) { return fx_shadd16(a, b); }
int32_t probe_sxtb16(int32_t x) { return fx_sxtb16(x); }
int32_t probe_ssat16x8(int32_t x) { return FX_SSAT16(x, 8); }
int32_t probe_smmulr(int32_t a, int32_t b) { return fx_smmulr(a, b); }
int32_t probe_smulwb(int32_t a, int32_t b) { return fx_smulwb(a, b); }
int32_t probe_smlawb(int32_t acc, int32_t a, int32_t b) {
  return fx_smlawb(acc, a, b);
}

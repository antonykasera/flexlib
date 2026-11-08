/**
 * @file cases_parse.c
 * @brief Bench cases for fu_parse.h, with the candidates and newlib's
 *        strtoul / strtol / strtod
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_bits.h"
#include "fu_fmt.h"
#include "fu_parse.h"
#include <math.h>
#include <stdlib.h>

/* ---- candidates ---- */

/* the scalar decimal loop fu_parse_u32 had before the SWAR one */
BENCH_CALL static FU_ParseStatus u32_scalar(const char *s, size_t len,
                                            uint32_t *out, size_t *used) {
  size_t i = 0U;
  uint32_t x = 0U;
  bool ovf = false;
  for (; i < len; i++) {
    uint32_t d = (uint32_t)(uint8_t)s[i] - '0';
    if (d > 9U) {
      break;
    }
    if (x >= 429496729U && (x > 429496729U || d > 5U)) {
      ovf = true;
    }
    x = x * 10U + d;
  }
  if (i == 0U || (used == NULL && i != len)) {
    return FU_PARSE_SYNTAX;
  }
  if (used != NULL) {
    *used = i;
  }
  *out = ovf ? UINT32_MAX : x;
  return ovf ? FU_PARSE_RANGE : FU_PARSE_OK;
}

/* hex by classifying each character (fu_parse_hex32 uses a table) */
FX_INLINE uint32_t hex_digit(char c) {
  uint32_t d = (uint32_t)(uint8_t)c - '0';
  if (d > 9U) {
    d = ((uint32_t)(uint8_t)c | 0x20U) - 'a';
    d = d < 6U ? d + 10U : 16U;
  }
  return d;
}

BENCH_CALL static FU_ParseStatus hex_branch(const char *s, size_t len,
                                            uint32_t *out, size_t *used) {
  size_t i = 0U;
  if (len >= 3U && s[0] == '0' && (s[1] | 0x20) == 'x' &&
      hex_digit(s[2]) < 16U) {
    i = 2U;
  }
  size_t first = i;
  uint32_t v = 0U;
  bool ovf = false;
  for (; i < len; i++) {
    uint32_t d = hex_digit(s[i]);
    if (d > 15U) {
      break;
    }
    ovf = ovf || (v >> 28) != 0U;
    v = (v << 4) | d;
  }
  if (i == first || (used == NULL && i != len)) {
    return FU_PARSE_SYNTAX;
  }
  if (used != NULL) {
    *used = i;
  }
  *out = ovf ? UINT32_MAX : v;
  return ovf ? FU_PARSE_RANGE : FU_PARSE_OK;
}

/* ---- inputs: text made by fu_fmt, one entry per bench input ---- */

/* in bench_scratch; "-32768.0000" is the longest text */
static char (*const text)[12] = (char (*)[12])bench_scratch;
static uint8_t *const text_len = bench_scratch + BENCH_N * 12U;
_Static_assert(BENCH_N * 13U <= BENCH_SCRATCH_SIZE, "parse data");
static unsigned slot;

/* stores the text for the next input and returns its index */
static uint32_t put_text(const char *s, size_t n) {
  static const char id = 0;
  (void)bench_scratch_claim(&id);
  unsigned k = slot++ % BENCH_N;
  memcpy(text[k], s, n);
  text[k][n] = '\0';
  text_len[k] = (uint8_t)n;
  return k;
}

static inline uint32_t in_u32_log(uint32_t u) {
  char s[24];
  return put_text(s, fu_fmt_u32(s, u >> (u & 31U)));
}
static inline uint32_t in_u32_10(uint32_t u) {
  char s[24];
  return put_text(s, fu_fmt_u32(s, 1000000000U + u % 3294967296U));
}
static inline uint32_t in_i32_log(uint32_t u) {
  char s[24];
  return put_text(s, fu_fmt_i32(s, (int32_t)u >> (u & 31U)));
}
static inline uint32_t in_hex_8(uint32_t u) {
  char s[24];
  return put_text(s, fu_fmt_hex32(s, u, 8U, (u & 1U) != 0U));
}
static inline uint32_t in_q16_4(uint32_t u) {
  char s[24];
  return put_text(s, fu_fmt_q16(s, (int32_t)u >> (u & 15U), 4U));
}

static uint32_t val;
#define VAL_HASH h = bench_hash(h, &val, sizeof val)

/* ---- cases ---- */

BENCH_UNARY_POST(p_u32_log, uint32_t, in_u32_log, uint32_t,
                 fu_parse_u32(text[x], text_len[x], &val, NULL), VAL_HASH)
BENCH_UNARY_POST(p_scalar_log, uint32_t, in_u32_log, uint32_t,
                 u32_scalar(text[x], text_len[x], &val, NULL), VAL_HASH)
BENCH_UNARY_POST(p_strtoul_log, uint32_t, in_u32_log, uint32_t,
                 (val = (uint32_t)strtoul(text[x], NULL, 10), 0U), VAL_HASH)
BENCH_UNARY_POST(p_u32_10, uint32_t, in_u32_10, uint32_t,
                 fu_parse_u32(text[x], text_len[x], &val, NULL), VAL_HASH)
BENCH_UNARY_POST(p_scalar_10, uint32_t, in_u32_10, uint32_t,
                 u32_scalar(text[x], text_len[x], &val, NULL), VAL_HASH)
BENCH_UNARY_POST(p_strtoul_10, uint32_t, in_u32_10, uint32_t,
                 (val = (uint32_t)strtoul(text[x], NULL, 10), 0U), VAL_HASH)
BENCH_UNARY_POST(p_i32_log, uint32_t, in_i32_log, uint32_t,
                 fu_parse_i32(text[x], text_len[x], (int32_t *)&val, NULL),
                 VAL_HASH)
BENCH_UNARY_POST(p_strtol_log, uint32_t, in_i32_log, uint32_t,
                 (val = (uint32_t)strtol(text[x], NULL, 10), 0U), VAL_HASH)
BENCH_UNARY_POST(p_hex_8, uint32_t, in_hex_8, uint32_t,
                 fu_parse_hex32(text[x], text_len[x], &val, NULL), VAL_HASH)
BENCH_UNARY_POST(p_hexbr_8, uint32_t, in_hex_8, uint32_t,
                 hex_branch(text[x], text_len[x], &val, NULL), VAL_HASH)
BENCH_UNARY_POST(p_strtoul16_8, uint32_t, in_hex_8, uint32_t,
                 (val = (uint32_t)strtoul(text[x], NULL, 16), 0U), VAL_HASH)
BENCH_UNARY_POST(p_q16_4, uint32_t, in_q16_4, uint32_t,
                 fu_parse_q16(text[x], text_len[x], (int32_t *)&val, NULL),
                 VAL_HASH)
/* what code without fu_parse does: strtod, scale, round */
BENCH_UNARY_POST(p_strtod_4, uint32_t, in_q16_4, uint32_t,
                 (val = (uint32_t)(int32_t)lrint(strtod(text[x], NULL) *
                                                 65536.0),
                  0U),
                 VAL_HASH)

/* the newlib rows return 0 where fu_parse returns FU_PARSE_OK (0), so a
 * flexlib row and its baseline share a hash */
static const BenchCase cases[] = {
    {"fu_parse_u32, log", "flexlib", p_u32_log, true},
    {"u32 one digit per step, log", "candidate", p_scalar_log, true},
    {"strtoul, log", "newlib", p_strtoul_log, true},
    {"fu_parse_u32, 10 digits", "flexlib", p_u32_10, true},
    {"u32 one digit per step, 10 digits", "candidate", p_scalar_10, true},
    {"strtoul, 10 digits", "newlib", p_strtoul_10, true},
    {"fu_parse_i32, log", "flexlib", p_i32_log, true},
    {"strtol, log", "newlib", p_strtol_log, true},
    {"fu_parse_hex32, 8 digits", "flexlib", p_hex_8, true},
    {"hex two-branch classify, 8 digits", "candidate", p_hexbr_8, true},
    {"strtoul base 16, 8 digits", "newlib", p_strtoul16_8, true},
    {"fu_parse_q16, 4 decimals", "flexlib", p_q16_4, true},
    {"strtod * 65536 + lrint, 4 decimals", "newlib", p_strtod_4, true},
};

const BenchGroup bench_group_parse = {cases, BENCH_COUNT(cases)};

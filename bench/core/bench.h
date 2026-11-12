/**
 * @file bench.h
 * @brief Benchmark cases
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BENCH_H
#define BENCH_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define BENCH_N 256U

#ifdef BENCH_HOST
#define BENCH_MEASURED_TEXT
#define BENCH_TABLE
#else
#define BENCH_MEASURED_TEXT __attribute__((section(".bench_measured.text")))
#define BENCH_TABLE __attribute__((section(".bench_measured.rodata")))
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define BENCH_CALL __attribute__((noinline, noipa)) BENCH_MEASURED_TEXT
#else
#define BENCH_CALL __attribute__((noinline)) BENCH_MEASURED_TEXT
#endif
#define BENCH_COUNT(a) ((unsigned)(sizeof(a) / sizeof((a)[0])))

/* every case derives its arguments from these */
extern volatile uint32_t bench_in[BENCH_N];

extern uint8_t bench_arg_a[BENCH_N * 8U];
extern uint8_t bench_arg_b[BENCH_N * 8U];

/* Bytes the case groups share, since only one runs at a time; the FFT image
 * (and the builds of every group) needs a 1024-point q31 block */
#if defined(BENCH_SET_utils) || defined(BENCH_SET_math) ||                     \
    defined(BENCH_SET_struct) || defined(BENCH_SET_dsp) ||                     \
    defined(BENCH_SET_dsp2) || defined(BENCH_SET_dsp4) ||                      \
    defined(BENCH_SET_dsp5)
#define BENCH_SCRATCH_SIZE 3584U
#else
#define BENCH_SCRATCH_SIZE 8192U
#endif
extern uint8_t bench_scratch[BENCH_SCRATCH_SIZE];
bool bench_scratch_claim(const void *id);

typedef uint32_t (*BenchFn)(uint32_t cyc[BENCH_N]); /* returns the hash */

typedef struct {
  const char *name; /* unique: the golden hash is looked up by it */
  const char *impl; /* "flexlib", "libm", "libgcc", "CMSIS-DSP" */
  BenchFn fn;
  bool golden; /* false when the host build differs (glibc vs newlib) */
} BenchCase;

typedef struct {
  const BenchCase *cases;
  unsigned n;
} BenchGroup;

extern const BenchGroup *const bench_groups[];
extern const unsigned bench_groups_n;

/* cases also timed cold: the flash cache flushed before every sample */
extern const BenchGroup *const bench_cold_groups[];
extern const unsigned bench_cold_groups_n;

typedef struct {
  const char *name;
  uint32_t hash;
} BenchGolden;

extern const BenchGolden bench_golden[]; /* golden.c, from `make golden` */
extern const unsigned bench_golden_n;

typedef enum {
  BENCH_GOLDEN_OK,
  BENCH_GOLDEN_FAIL,
  BENCH_GOLDEN_MISSING, /* golden case not in golden.c: run `make golden` */
  BENCH_GOLDEN_SKIP,    /* not a golden case */
} BenchGoldenStatus;

#define BENCH_SLOW_LIST 4U

typedef struct {
  uint32_t min;
  uint32_t median; /* upper median */
  uint32_t max;
  uint32_t hash;
  uint32_t n_slow;                   /* samples above 2 * median + 4 */
  uint16_t slow_at[BENCH_SLOW_LIST]; /* input indices of the first ones */
} BenchResult;

void bench_inputs_init(void);

/* Runs the empty case: the cycles a pair of CYCCNT reads adds to a sample. */
uint32_t bench_overhead(uint32_t cyc[BENCH_N], bool cold);

/* Runs c, subtracts overhead from every sample (clamped at 0). */
BenchResult bench_run(const BenchCase *c, uint32_t overhead,
                      uint32_t cyc[BENCH_N]);

BenchGoldenStatus bench_golden_check(const BenchCase *c, uint32_t hash);

/* ---- timing primitives ---- */

#define BENCH_HASH_INIT 2166136261U /* FNV-1a */

static inline uint32_t bench_hash(uint32_t h, const void *p, unsigned n) {
  const uint8_t *b = (const uint8_t *)p;
  for (unsigned i = 0U; i < n; i++) {
    h = (h ^ b[i]) * 16777619U;
  }
  return h;
}

extern uintptr_t bench_cold_keep; /* the running case's code, kept warm */

#ifdef BENCH_HOST
static inline void bench_cold_flush(void) {}
static inline uint32_t bench_now(void) { return 0U; }
#define BENCH_PIN(x) ((void)0)
#define BENCH_USE(y) ((void)(y))
#else
/* reads the low 32 KB of flash, then bench_cold_keep's lines (from SRAM) */
void bench_cold_flush(void);

/* DWT->CYCCNT, by address so case files don't need CMSIS */
static inline uint32_t bench_now(void) {
  return *(volatile const uint32_t *)0xE0001004U;
}
/* Keep the call between the CYCCNT reads */
#define BENCH_PIN(x) __asm__ volatile("" : "+r"(x) : : "memory")
#define BENCH_USE(y) __asm__ volatile("" : : "r"(y) : "memory")
#endif

/* Defines `static uint32_t name(uint32_t *cyc)`, timing `expr` (of x) once per
 * input. */
#define BENCH_UNARY(name, Tx, conv, Tr, expr)                                  \
  BENCH_UNARY_DEF(name, Tx, conv, Tr, expr, (void)0, (void)0)

/* for a call that writes text to buf and returns its length */
#define BENCH_TEXT(name, Tx, conv, expr, buf)                                  \
  BENCH_UNARY_POST(name, Tx, conv, uint32_t, (uint32_t)(expr),                 \
                   h = bench_hash(h, (buf), (unsigned)res))

/* BENCH_UNARY, plus `post` (which sees h and res) after each hash */
#define BENCH_UNARY_POST(name, Tx, conv, Tr, expr, post)                       \
  BENCH_UNARY_DEF(name, Tx, conv, Tr, expr, post, (void)0)

/* Same, for `expr` of a and b */
#define BENCH_BINARY(name, Ta, conva, Tb, convb, Tr, expr)                     \
  BENCH_BINARY_DEF(name, Ta, conva, Tb, convb, Tr, expr, (void)0)

/* Both, plus name##_cold, which flushes the flash cache before each sample. */
#define BENCH_UNARY_COLD(name, Tx, conv, Tr, expr)                             \
  BENCH_UNARY(name, Tx, conv, Tr, expr)                                        \
  BENCH_UNARY_DEF(name##_cold, Tx, conv, Tr, expr, (void)0, bench_cold_flush())
#define BENCH_BINARY_COLD(name, Ta, conva, Tb, convb, Tr, expr)                \
  BENCH_BINARY(name, Ta, conva, Tb, convb, Tr, expr)                           \
  BENCH_BINARY_DEF(name##_cold, Ta, conva, Tb, convb, Tr, expr,                \
                   bench_cold_flush())

#define BENCH_UNARY_DEF(name, Tx, conv, Tr, expr, post, before)                \
  static uint32_t name(uint32_t *cyc) {                                        \
    _Static_assert(sizeof(Tx) <= 8U, "argument wider than its slot");          \
    for (unsigned i = 0U; i < BENCH_N; i++) {                                  \
      Tx v = conv(bench_in[i]);                                                \
      memcpy(&bench_arg_a[8U * i], &v, sizeof v);                              \
    }                                                                          \
    uint32_t h = BENCH_HASH_INIT;                                              \
    for (unsigned i = 0U; i < BENCH_N; i++) {                                  \
      before;                                                                  \
      Tx x;                                                                    \
      memcpy(&x, &bench_arg_a[8U * i], sizeof x);                              \
      BENCH_PIN(x);                                                            \
      uint32_t t0 = bench_now();                                               \
      BENCH_PIN(x);                                                            \
      Tr r = (expr);                                                           \
      BENCH_USE(r);                                                            \
      uint32_t t1 = bench_now();                                               \
      cyc[i] = t1 - t0;                                                        \
      Tr res = r; /* hashing &r would put a store in the window */             \
      BENCH_PIN(res);                                                          \
      h = bench_hash(h, &res, (unsigned)sizeof res);                           \
      post;                                                                    \
    }                                                                          \
    return h;                                                                  \
  }

#define BENCH_BINARY_DEF(name, Ta, conva, Tb, convb, Tr, expr, before)         \
  static uint32_t name(uint32_t *cyc) {                                        \
    _Static_assert(sizeof(Ta) <= 8U && sizeof(Tb) <= 8U,                       \
                   "argument wider than its slot");                            \
    for (unsigned i = 0U; i < BENCH_N; i++) {                                  \
      Ta va = conva(bench_in[i]);                                              \
      Tb vb = convb(bench_in[(i + 1U) % BENCH_N]);                             \
      memcpy(&bench_arg_a[8U * i], &va, sizeof va);                            \
      memcpy(&bench_arg_b[8U * i], &vb, sizeof vb);                            \
    }                                                                          \
    uint32_t h = BENCH_HASH_INIT;                                              \
    for (unsigned i = 0U; i < BENCH_N; i++) {                                  \
      before;                                                                  \
      Ta a;                                                                    \
      Tb b;                                                                    \
      memcpy(&a, &bench_arg_a[8U * i], sizeof a);                              \
      memcpy(&b, &bench_arg_b[8U * i], sizeof b);                              \
      BENCH_PIN(a);                                                            \
      BENCH_PIN(b);                                                            \
      uint32_t t0 = bench_now();                                               \
      BENCH_PIN(a);                                                            \
      BENCH_PIN(b);                                                            \
      Tr r = (expr);                                                           \
      BENCH_USE(r);                                                            \
      uint32_t t1 = bench_now();                                               \
      cyc[i] = t1 - t0;                                                        \
      Tr res = r;                                                              \
      BENCH_PIN(res);                                                          \
      h = bench_hash(h, &res, (unsigned)sizeof res);                           \
    }                                                                          \
    return h;                                                                  \
  }

#endif /* BENCH_H */

/**
 * @file bench.h
 * @brief Benchmark cases: cycle timing on the F103, output hashes on both the
 *        F103 and the host (golden check)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * A case times one kernel call per input over BENCH_N inputs and returns a
 * hash of the outputs. Built with -DBENCH_HOST, the same cases run on the PC
 * (`make golden`) to give the hashes the board must reproduce.
 *
 *   BENCH_UNARY(b_clz, uint32_t, in_u32, uint32_t, fx_clz(x))
 *   static const BenchCase cases[] = {{"fx_clz", "flexlib", b_clz, true}};
 *   const BenchGroup bench_group_foo = {cases, BENCH_COUNT(cases)};
 */

#ifndef BENCH_H
#define BENCH_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define BENCH_N 256U

/* For candidates timed as calls: no inlining, and no GCC clone specialised on
 * constant arguments, so they pay the same argument setup as the library. */
#if defined(__GNUC__) && !defined(__clang__)
#define BENCH_CALL __attribute__((noinline, noipa))
#else
#define BENCH_CALL __attribute__((noinline))
#endif
#define BENCH_COUNT(a) ((unsigned)(sizeof(a) / sizeof((a)[0])))

/* every case derives its arguments from these; see bench_inputs_init() */
extern volatile uint32_t bench_in[BENCH_N];

/* Converted arguments, one 8-byte slot per input, filled before the timed
 * loop so conversion temporaries don't crowd the registers it needs. */
extern uint8_t bench_arg_a[BENCH_N * 8U];
extern uint8_t bench_arg_b[BENCH_N * 8U];

typedef uint32_t (*BenchFn)(uint32_t cyc[BENCH_N]); /* returns the hash */

typedef struct {
  const char *name; /* unique: the golden hash is looked up by it */
  const char *impl; /* "flexlib", "libm", "libgcc", "CMSIS-DSP" */
  BenchFn fn;
  bool golden; /* false when the host build differs (e.g. glibc vs newlib) */
} BenchCase;

typedef struct {
  const BenchCase *cases;
  unsigned n;
} BenchGroup;

extern const BenchGroup *const bench_groups[];
extern const unsigned bench_groups_n;

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

/* Both run their case once untimed first, so samples are warm. */

/* Runs the empty case: the cycles a pair of CYCCNT reads adds to a sample. */
uint32_t bench_overhead(uint32_t cyc[BENCH_N]);

/* Runs c, subtracts overhead from every sample (clamped at 0). cyc keeps the
 * samples in input order. */
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

#ifdef BENCH_HOST
static inline uint32_t bench_now(void) { return 0U; }
#define BENCH_PIN(x) ((void)0)
#define BENCH_USE(y) ((void)(y))
#else
/* DWT->CYCCNT, by address so case files don't need CMSIS */
static inline uint32_t bench_now(void) {
  return *(volatile const uint32_t *)0xE0001004U;
}
/* The memory clobber orders these against the CYCCNT reads. Pinning x after
 * t0 stops its consumer starting early, USE before t1 makes the result finish
 * in time, and pinning the copy after t1 keeps the hash out of the window.
 * GCC re-extends a pinned 16-bit x (one SXTH in the window); the
 * "empty, int16 arg" case measures that. `make bench-windows` lists what each
 * window holds. */
#define BENCH_PIN(x) __asm__ volatile("" : "+r"(x) : : "memory")
#define BENCH_USE(y) __asm__ volatile("" : : "r"(y) : "memory")
#endif

/* Defines `static uint32_t name(uint32_t *cyc)`, timing `expr` (of x) once
 * per input. conv maps the raw uint32_t input to x and runs untimed. */
#define BENCH_UNARY(name, Tx, conv, Tr, expr)                                  \
  BENCH_UNARY_POST(name, Tx, conv, Tr, expr, (void)0)

/* For a call that writes text to buf and returns its length: the hash also
 * covers the text. The length is hashed as uint32_t, since size_t is 8 bytes
 * on a 64-bit host. */
#define BENCH_TEXT(name, Tx, conv, expr, buf)                                  \
  BENCH_UNARY_POST(name, Tx, conv, uint32_t, (uint32_t)(expr),                 \
                   h = bench_hash(h, (buf), (unsigned)out))

/* BENCH_UNARY, plus `post` (which sees h and out) after each hash */
#define BENCH_UNARY_POST(name, Tx, conv, Tr, expr, post)                       \
  static uint32_t name(uint32_t *cyc) {                                        \
    _Static_assert(sizeof(Tx) <= 8U, "argument wider than its slot");          \
    for (unsigned i = 0U; i < BENCH_N; i++) {                                  \
      Tx v = conv(bench_in[i]);                                                \
      memcpy(&bench_arg_a[8U * i], &v, sizeof v);                              \
    }                                                                          \
    uint32_t h = BENCH_HASH_INIT;                                              \
    for (unsigned i = 0U; i < BENCH_N; i++) {                                  \
      Tx x;                                                                    \
      memcpy(&x, &bench_arg_a[8U * i], sizeof x);                              \
      BENCH_PIN(x);                                                            \
      uint32_t t0 = bench_now();                                               \
      BENCH_PIN(x);                                                            \
      Tr r = (expr);                                                           \
      BENCH_USE(r);                                                            \
      uint32_t t1 = bench_now();                                               \
      cyc[i] = t1 - t0;                                                        \
      Tr out = r; /* hashing &r would put a store in the window */             \
      BENCH_PIN(out);                                                          \
      h = bench_hash(h, &out, (unsigned)sizeof out);                           \
      post;                                                                    \
    }                                                                          \
    return h;                                                                  \
  }

/* Same, for `expr` of a and b; b comes from the next input along. */
#define BENCH_BINARY(name, Ta, conva, Tb, convb, Tr, expr)                     \
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
      Tr out = r;                                                              \
      BENCH_PIN(out);                                                          \
      h = bench_hash(h, &out, (unsigned)sizeof out);                           \
    }                                                                          \
    return h;                                                                  \
  }

#endif /* BENCH_H */

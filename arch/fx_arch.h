/**
 * @file fx_arch.h
 * @brief Single-instruction wrappers: Arm instructions on target, plain C on
 *        the host
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FX_ARCH_H
#define FX_ARCH_H

#include <stdint.h>

#if defined(__arm__)
#include <arm_acle.h>
#define FX_ARM 1
#else
#define FX_ARM 0
#endif

/* M4/M7 DSP extension (QADD, SMMUL, SIMD). The M3 has none of it. */
#if defined(__ARM_FEATURE_DSP)
#define FX_HAVE_DSP 1
#else
#define FX_HAVE_DSP 0
#endif

#define FX_INLINE static inline __attribute__((always_inline))

/* Tables of 64 B or more: 32 B is the M7 cache line and a multiple of the F4
 * ART line and the F1 prefetch width, so a table touches the fewest lines. */
#define FX_ALIGN_LINE __attribute__((aligned(32)))

/* rare paths: out of line, so hot code packs into fewer cache lines */
#define FX_COLD __attribute__((cold, noinline))

/* hides x from the optimiser, e.g. so a copy loop isn't made a memcpy call */
#define FX_OPAQUE(x) __asm__("" : "+r"(x))

/* ---- programmer-mistake checks: trap (one UDF, no stdio) ----
 * Override with FX_ASSERT_FAIL(file, line); FX_DISABLE_ASSERT removes them. */
#ifndef FX_ASSERT_FAIL
#define FX_ASSERT_FAIL(file, line) __builtin_trap()
#endif
#ifdef FX_DISABLE_ASSERT
#define FX_ASSERT(cond) ((void)0)
#else
#define FX_ASSERT(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      FX_ASSERT_FAIL(__FILE__, __LINE__);                                      \
    }                                                                          \
  } while (0)
#endif

/* ---- bit operations ---- */

/* clz(0) = 32, as the instruction gives; the zero test folds into CLZ on Arm */
FX_INLINE uint32_t fx_clz(uint32_t x) {
  return x != 0U ? (uint32_t)__builtin_clz(x) : 32U;
}

/* ctz(0) = 32. RBIT + CLZ on Thumb-2. */
FX_INLINE uint32_t fx_ctz(uint32_t x) {
  return x != 0U ? (uint32_t)__builtin_ctz(x) : 32U;
}

FX_INLINE uint32_t fx_rbit(uint32_t x) {
#if FX_ARM && defined(__thumb2__)
#if defined(__clang__) || __GNUC__ >= 14
  return __rbit(x);
#else
  /* GCC 13's arm_acle.h has no __rbit */
  uint32_t r;
  __asm__("rbit %0, %1" : "=r"(r) : "r"(x));
  return r;
#endif
#else
  x = ((x >> 1) & 0x55555555U) | ((x & 0x55555555U) << 1);
  x = ((x >> 2) & 0x33333333U) | ((x & 0x33333333U) << 2);
  x = ((x >> 4) & 0x0F0F0F0FU) | ((x & 0x0F0F0F0FU) << 4);
  return __builtin_bswap32(x);
#endif
}

FX_INLINE uint32_t fx_rev(uint32_t x) { return __builtin_bswap32(x); }

/* swap the bytes within each halfword (REV16) */
FX_INLINE uint32_t fx_rev16(uint32_t x) {
  return ((x & 0x00FF00FFU) << 8) | ((x >> 8) & 0x00FF00FFU);
}

/* ---- saturation ---- */

/* Plain-C versions, for the host. n must be a constant in the macros below
 * (SSAT/USAT take it as an immediate). */
FX_INLINE int32_t fx_ssat_c(int32_t x, uint32_t n) {
  const int32_t hi = (int32_t)((1UL << (n - 1U)) - 1UL);
  const int32_t lo = -hi - 1;
  return x > hi ? hi : (x < lo ? lo : x);
}

FX_INLINE uint32_t fx_usat_c(int32_t x, uint32_t n) {
  const int32_t hi = (int32_t)((1UL << n) - 1UL);
  return (uint32_t)(x > hi ? hi : (x < 0 ? 0 : x));
}

/* FX_SSAT(x, n): clamp to n-bit signed, n in 1..32.
 * FX_USAT(x, n): clamp to n-bit unsigned, n in 0..31. */
#if FX_ARM && defined(__ARM_FEATURE_SAT)
#define FX_SSAT(x, n) ((int32_t)__ssat((x), (n)))
#define FX_USAT(x, n) ((uint32_t)__usat((x), (n)))
#else
#define FX_SSAT(x, n) fx_ssat_c((x), (n))
#define FX_USAT(x, n) fx_usat_c((x), (n))
#endif

/* ---- 32x32 -> 64 multiplies: GCC and Clang lower these to SMULL / UMULL /
 * SMLAL / UMLAL, which asm-audit checks ---- */

FX_INLINE int64_t fx_smull(int32_t a, int32_t b) { return (int64_t)a * b; }

FX_INLINE uint64_t fx_umull(uint32_t a, uint32_t b) { return (uint64_t)a * b; }

FX_INLINE int64_t fx_smlal(int64_t acc, int32_t a, int32_t b) {
  return acc + (int64_t)a * b;
}

FX_INLINE uint64_t fx_umlal(uint64_t acc, uint32_t a, uint32_t b) {
  return acc + (uint64_t)a * b;
}

/* high word of the product; the >> is arithmetic on every supported compiler */
FX_INLINE int32_t fx_smull_hi(int32_t a, int32_t b) {
  return (int32_t)(fx_smull(a, b) >> 32);
}

FX_INLINE uint32_t fx_umull_hi(uint32_t a, uint32_t b) {
  return (uint32_t)(fx_umull(a, b) >> 32);
}

/* ---- barriers ---- */

/* Orders memory accesses in the compiler only. Enough between the producer
 * and consumer of a ring on a single core. */
FX_INLINE void fx_compiler_barrier(void) { __asm__ volatile("" ::: "memory"); }

#if FX_ARM
FX_INLINE void fx_dmb(void) { __asm__ volatile("dmb" ::: "memory"); }
FX_INLINE void fx_dsb(void) { __asm__ volatile("dsb" ::: "memory"); }
FX_INLINE void fx_isb(void) { __asm__ volatile("isb" ::: "memory"); }
#else
FX_INLINE void fx_dmb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
FX_INLINE void fx_dsb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
FX_INLINE void fx_isb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#endif

#endif /* FX_ARCH_H */

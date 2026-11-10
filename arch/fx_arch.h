/**
 * @file fx_arch.h
 * @brief Single-instruction wrappers (Arm on target, plain C on host)
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

/* M4/M7 DSP extension; the M3 has none */
#if defined(__ARM_FEATURE_DSP)
#define FX_HAVE_DSP 1
#else
#define FX_HAVE_DSP 0
#endif

/* GCC 13: no ACLE bit intrinsics, and a zero check before every CLZ */
#if FX_ARM && defined(__thumb2__) && !defined(__clang__) && __GNUC__ < 14
#define FX_OLD_GCC 1
#else
#define FX_OLD_GCC 0
#endif

#define FX_INLINE static inline __attribute__((always_inline))

/* for tables of 64 B or more: 32 B = M7 cache line */
#define FX_ALIGN_LINE __attribute__((aligned(32)))

/* rare paths, kept out of line */
#define FX_COLD __attribute__((cold, noinline))

/* hides x from the optimiser */
#define FX_OPAQUE(x) __asm__("" : "+r"(x))

/* ---- asserts trap; override FX_ASSERT_FAIL, or FX_DISABLE_ASSERT ---- */
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

/* clz(0) = 32 */
FX_INLINE uint32_t fx_clz(uint32_t x) {
#if FX_OLD_GCC
  if (!__builtin_constant_p(x)) {
    uint32_t r;
    __asm__("clz %0, %1" : "=r"(r) : "r"(x));
    return r;
  }
#endif
  return x != 0U ? (uint32_t)__builtin_clz(x) : 32U;
}

/* ctz(0) = 32 */
FX_INLINE uint32_t fx_ctz(uint32_t x) {
#if FX_OLD_GCC
  if (!__builtin_constant_p(x)) {
    uint32_t r;
    __asm__("rbit %0, %1\n\tclz %0, %0" : "=r"(r) : "r"(x));
    return r;
  }
#endif
  return x != 0U ? (uint32_t)__builtin_ctz(x) : 32U;
}

FX_INLINE uint32_t fx_rbit(uint32_t x) {
#if FX_OLD_GCC
  uint32_t r;
  __asm__("rbit %0, %1" : "=r"(r) : "r"(x));
  return r;
#elif FX_ARM && defined(__thumb2__)
  return __rbit(x);
#else
  x = ((x >> 1) & 0x55555555U) | ((x & 0x55555555U) << 1);
  x = ((x >> 2) & 0x33333333U) | ((x & 0x33333333U) << 2);
  x = ((x >> 4) & 0x0F0F0F0FU) | ((x & 0x0F0F0F0FU) << 4);
  return __builtin_bswap32(x);
#endif
}

FX_INLINE uint32_t fx_rev(uint32_t x) { return __builtin_bswap32(x); }

/* rotate left by k (0..31) */
FX_INLINE uint32_t fx_rotl32(uint32_t x, uint32_t k) {
  return (x << (k & 31U)) | (x >> ((32U - k) & 31U));
}

/* swap bytes within each halfword */
FX_INLINE uint32_t fx_rev16(uint32_t x) {
#if FX_OLD_GCC
  if (!__builtin_constant_p(x)) {
    uint32_t r;
    __asm__("rev16 %0, %1" : "=r"(r) : "r"(x));
    return r;
  }
#endif
  return ((x & 0x00FF00FFU) << 8) | ((x >> 8) & 0x00FF00FFU);
}

/* ---- saturation ---- */

/* plain-C fallbacks; n must be constant in the macros (SSAT/USAT immediate) */
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

/* ---- 32x32 -> 64 multiplies (SMULL/UMULL/SMLAL/UMLAL) ---- */

FX_INLINE int64_t fx_smull(int32_t a, int32_t b) { return (int64_t)a * b; }

FX_INLINE uint64_t fx_umull(uint32_t a, uint32_t b) { return (uint64_t)a * b; }

FX_INLINE int64_t fx_smlal(int64_t acc, int32_t a, int32_t b) {
  return acc + (int64_t)a * b;
}

FX_INLINE uint64_t fx_umlal(uint64_t acc, uint32_t a, uint32_t b) {
  return acc + (uint64_t)a * b;
}

/* high word of the product */
FX_INLINE int32_t fx_smull_hi(int32_t a, int32_t b) {
  return (int32_t)(fx_smull(a, b) >> 32);
}

FX_INLINE uint32_t fx_umull_hi(uint32_t a, uint32_t b) {
  return (uint32_t)(fx_umull(a, b) >> 32);
}

/* ---- barriers ---- */

/* orders accesses in the compiler only (enough for a single-core ring) */
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

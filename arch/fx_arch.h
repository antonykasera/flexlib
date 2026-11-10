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

/* M4/M7 DSP extension; */
#if defined(__ARM_FEATURE_DSP)
#define FX_HAVE_DSP 1
#else
#define FX_HAVE_DSP 0
#endif

/* Kernels take their packed-pair path here; FX_EMULATE_DSP runs it on the
 * C fallbacks below */
#if FX_HAVE_DSP || defined(FX_EMULATE_DSP)
#define FX_DSP_PATH 1
#else
#define FX_DSP_PATH 0
#endif

/* single-precision FPU (M4F, M7) */
#if defined(__ARM_FP) && (__ARM_FP & 4)
#define FX_HAVE_FPU 1
#else
#define FX_HAVE_FPU 0
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

#define FX_ALIGN(n) __attribute__((aligned(n)))
#define FX_LIKELY(x) __builtin_expect(!!(x), 1)
#define FX_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define FX_PRAGMA(x) _Pragma(#x)
#define FX_UNROLL(n) FX_PRAGMA(GCC unroll n)

#ifdef __cplusplus
#define FX_RESTRICT __restrict
#else
#define FX_RESTRICT restrict
#endif

/* asserts trap; override FX_ASSERT_FAIL, or FX_DISABLE_ASSERT  */
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

/* ---- packed halfword pairs (SIMD32 on M4/M7) ----
 * A pair is an int32_t: bits 0-15 the low element, 16-31 the high one, as a
 * word load of two int16_t gives on little-endian. The C versions define the
 * results; the instructions match them. */

FX_INLINE int32_t fx_lo16(int32_t x) { return (int16_t)x; }
FX_INLINE int32_t fx_hi16(int32_t x) { return x >> 16; }

/* lo and hi (each taken mod 2^16) as a pair */
FX_INLINE int32_t fx_pack16(int32_t lo, int32_t hi) {
  return (int32_t)(((uint32_t)hi << 16) | ((uint32_t)lo & 0xFFFFU));
}

/* GCC doesn't form PKHBT, PKHTB, SMMULR or SMULWB from C: asm on DSP cores */

/* PKHBT: a's low half, b's low half on top */
FX_INLINE int32_t fx_pkhbt(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("pkhbt %0, %1, %2, lsl #16" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return fx_pack16(a, b);
#endif
}

/* PKHTB: a's high half on top, b's high half below */
FX_INLINE int32_t fx_pkhtb(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("pkhtb %0, %1, %2, asr #16" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return fx_pack16(fx_hi16(b), fx_hi16(a));
#endif
}

/* SMLAD: acc + lo*lo + hi*hi, wrapping */
FX_INLINE int32_t fx_smlad(int32_t acc, int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smlad(a, b, acc);
#else
  return (int32_t)((uint32_t)acc + (uint32_t)(fx_lo16(a) * fx_lo16(b)) +
                   (uint32_t)(fx_hi16(a) * fx_hi16(b)));
#endif
}

/* SMLALD: the same into 64 bits (exact) */
FX_INLINE int64_t fx_smlald(int64_t acc, int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smlald(a, b, acc);
#else
  return acc + (int64_t)(fx_lo16(a) * fx_lo16(b)) +
         (int64_t)(fx_hi16(a) * fx_hi16(b));
#endif
}

/* SMUAD: lo*lo + hi*hi, wrapping (only for both pairs all -32768) */
FX_INLINE int32_t fx_smuad(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smuad(a, b);
#else
  return fx_smlad(0, a, b);
#endif
}

/* SMUSDX: a.lo*b.hi - a.hi*b.lo, the imaginary part of a complex product */
FX_INLINE int32_t fx_smusdx(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smusdx(a, b);
#else
  return fx_lo16(a) * fx_hi16(b) - fx_hi16(a) * fx_lo16(b);
#endif
}

/* QADD16 / QSUB16: per half, saturated to int16 */
FX_INLINE int32_t fx_qadd16(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __qadd16(a, b);
#else
  return fx_pack16(fx_ssat_c(fx_lo16(a) + fx_lo16(b), 16U),
                   fx_ssat_c(fx_hi16(a) + fx_hi16(b), 16U));
#endif
}

FX_INLINE int32_t fx_qsub16(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __qsub16(a, b);
#else
  return fx_pack16(fx_ssat_c(fx_lo16(a) - fx_lo16(b), 16U),
                   fx_ssat_c(fx_hi16(a) - fx_hi16(b), 16U));
#endif
}

/* SHADD16: per half, (a + b) >> 1 rounded down; never overflows */
FX_INLINE int32_t fx_shadd16(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __shadd16(a, b);
#else
  return fx_pack16((fx_lo16(a) + fx_lo16(b)) >> 1,
                   (fx_hi16(a) + fx_hi16(b)) >> 1);
#endif
}

/* SXTB16: bytes 0 and 2, sign-extended into a pair */
FX_INLINE int32_t fx_sxtb16(int32_t x) {
#if FX_HAVE_DSP
  return __sxtb16(x);
#else
  return fx_pack16((int8_t)x, (int8_t)(x >> 16));
#endif
}

/* FX_SSAT16(x, n): each half clamped to n-bit signed, n in 1..16 constant */
#if FX_HAVE_DSP
#define FX_SSAT16(x, n) ((int32_t)__ssat16((x), (n)))
#else
#define FX_SSAT16(x, n)                                                        \
  fx_pack16(fx_ssat_c(fx_lo16(x), (n)), fx_ssat_c(fx_hi16(x), (n)))
#endif

/* SMMULR: high word of a*b, rounded half up */
FX_INLINE int32_t fx_smmulr(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("smmulr %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return (int32_t)((fx_smull(a, b) + 0x80000000LL) >> 32);
#endif
}

/* SMULWB / SMLAWB: (a * b.lo) >> 16 rounded down, the latter plus acc
 * (wrapping) */
FX_INLINE int32_t fx_smulwb(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("smulwb %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return (int32_t)(((int64_t)a * fx_lo16(b)) >> 16);
#endif
}

FX_INLINE int32_t fx_smlawb(int32_t acc, int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smlawb(a, b, acc);
#else
  return (int32_t)((uint32_t)acc + (uint32_t)fx_smulwb(a, b));
#endif
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

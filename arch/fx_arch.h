/**
 * @file fx_arch.h
 * @brief Single-instruction wrappers (Arm on target, plain C on host)
 *
 * Each wrapper is one instruction on a Cortex-M3 or later. Also the attribute
 * macros and the assert that the libraries use.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FX_ARCH_H
#define FX_ARCH_H

#include <stdint.h>

/**
 * @name Target
 * 1 or 0, from the compiler's predefined macros.
 * @{
 */

#if defined(__arm__)
#include <arm_acle.h>
/** 1 when compiling for Arm. */
#define FX_ARM 1
#else
/** 1 when compiling for Arm. */
#define FX_ARM 0
#endif

#if defined(__ARM_FEATURE_DSP)
/** 1 with the M4 / M7 DSP extension. */
#define FX_HAVE_DSP 1
#else
/** 1 with the M4 / M7 DSP extension. */
#define FX_HAVE_DSP 0
#endif

/** @cond */
/* the packed-pair paths; FX_EMULATE_DSP runs them on the C versions */
#if FX_HAVE_DSP || defined(FX_EMULATE_DSP)
#define FX_DSP_PATH 1
#else
#define FX_DSP_PATH 0
#endif
/** @endcond */

#if defined(__ARM_FP) && (__ARM_FP & 4)
/** 1 with a single-precision FPU (M4F, M7). */
#define FX_HAVE_FPU 1
#else
/** 1 with a single-precision FPU (M4F, M7). */
#define FX_HAVE_FPU 0
#endif

/** @} */

/** @cond */
/* the FPU paths; FX_EMULATE_FPU runs them on the host's float arithmetic */
#if FX_HAVE_FPU || defined(FX_EMULATE_FPU)
#define FX_FPU_PATH 1
#else
#define FX_FPU_PATH 0
#endif
/** @endcond */

/** @cond */
/* GCC 13: no ACLE bit intrinsics, and a zero check before every CLZ */
#if FX_ARM && defined(__thumb2__) && !defined(__clang__) && __GNUC__ < 14
#define FX_OLD_GCC 1
#else
#define FX_OLD_GCC 0
#endif
/** @endcond */

/**
 * @name Attributes
 * @{
 */

/** static inline, always inlined. */
#define FX_INLINE static inline __attribute__((always_inline))

/** 32-byte alignment, an M7 cache line: for tables of 64 bytes or more. */
#define FX_ALIGN_LINE __attribute__((aligned(32)))

/** A rare path, kept out of line. */
#define FX_COLD __attribute__((cold, noinline))

/** Hides x's value from the optimiser. */
#define FX_OPAQUE(x) __asm__("" : "+r"(x))

/** n-byte alignment. */
#define FX_ALIGN(n) __attribute__((aligned(n)))
/** x is usually true. */
#define FX_LIKELY(x) __builtin_expect(!!(x), 1)
/** x is usually false. */
#define FX_UNLIKELY(x) __builtin_expect(!!(x), 0)

/** A pragma from a macro. */
#define FX_PRAGMA(x) _Pragma(#x)
/** Unroll the next loop n times (GCC). */
#define FX_UNROLL(n) FX_PRAGMA(GCC unroll n)

#ifdef __cplusplus
/** restrict, also in C++. */
#define FX_RESTRICT __restrict
#else
/** restrict, also in C++. */
#define FX_RESTRICT restrict
#endif

/** @} */

/**
 * @name Asserts
 * Programmer errors trap. Define `FX_ASSERT_FAIL` to report them instead, or
 * `FX_DISABLE_ASSERT` to compile the checks out.
 * @{
 */

#ifndef FX_ASSERT_FAIL
/** Called on a failed assert; traps unless defined first. */
#define FX_ASSERT_FAIL(file, line) __builtin_trap()
#endif
#ifdef FX_DISABLE_ASSERT
/** Calls `FX_ASSERT_FAIL` when cond is false. */
#define FX_ASSERT(cond) ((void)0)
#else
/** Calls `FX_ASSERT_FAIL` when cond is false. */
#define FX_ASSERT(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      FX_ASSERT_FAIL(__FILE__, __LINE__);                                      \
    }                                                                          \
  } while (0)
#endif

/** @} */

/**
 * @name Bits
 * @{
 */

/** Leading zeros; 32 for 0. */
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

/** Trailing zeros; 32 for 0. */
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

/** The bits in reverse order. */
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

/** The bytes in reverse order. */
FX_INLINE uint32_t fx_rev(uint32_t x) { return __builtin_bswap32(x); }

/** Rotate left by k, $0 <= k <= 31$. */
FX_INLINE uint32_t fx_rotl32(uint32_t x, uint32_t k) {
  return (x << (k & 31U)) | (x >> ((32U - k) & 31U));
}

/** Swap the bytes within each halfword. */
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

/** @} */

/**
 * @name Saturation
 * n must be a constant: it is the instruction's immediate.
 * @{
 */

/** @cond */
FX_INLINE int32_t fx_ssat_c(int32_t x, uint32_t n) {
  const int32_t hi = (int32_t)((1UL << (n - 1U)) - 1UL);
  const int32_t lo = -hi - 1;
  return x > hi ? hi : (x < lo ? lo : x);
}

FX_INLINE uint32_t fx_usat_c(int32_t x, uint32_t n) {
  const int32_t hi = (int32_t)((1UL << n) - 1UL);
  return (uint32_t)(x > hi ? hi : (x < 0 ? 0 : x));
}
/** @endcond */

#if FX_ARM && defined(__ARM_FEATURE_SAT)
/** x clamped to n-bit signed, $1 <= n <= 32$. */
#define FX_SSAT(x, n) ((int32_t)__ssat((x), (n)))
/** x clamped to n-bit unsigned, $0 <= n <= 31$. */
#define FX_USAT(x, n) ((uint32_t)__usat((x), (n)))
#else
/** x clamped to n-bit signed, $1 <= n <= 32$. */
#define FX_SSAT(x, n) fx_ssat_c((x), (n))
/** x clamped to n-bit unsigned, $0 <= n <= 31$. */
#define FX_USAT(x, n) fx_usat_c((x), (n))
#endif

/** @} */

/**
 * @name 32 x 32 -> 64 multiplies
 * SMULL, UMULL, SMLAL, UMLAL.
 * @{
 */

/** $a b$, signed. */
FX_INLINE int64_t fx_smull(int32_t a, int32_t b) { return (int64_t)a * b; }

/** $a b$, unsigned. */
FX_INLINE uint64_t fx_umull(uint32_t a, uint32_t b) { return (uint64_t)a * b; }

/** $acc + a b$, signed; wraps past 64 bits. */
FX_INLINE int64_t fx_smlal(int64_t acc, int32_t a, int32_t b) {
  return acc + (int64_t)a * b;
}

/** $acc + a b$, unsigned; wraps past 64 bits. */
FX_INLINE uint64_t fx_umlal(uint64_t acc, uint32_t a, uint32_t b) {
  return acc + (uint64_t)a * b;
}

/** High word of $a b$, signed. */
FX_INLINE int32_t fx_smull_hi(int32_t a, int32_t b) {
  return (int32_t)(fx_smull(a, b) >> 32);
}

/** High word of $a b$, unsigned. */
FX_INLINE uint32_t fx_umull_hi(uint32_t a, uint32_t b) {
  return (uint32_t)(fx_umull(a, b) >> 32);
}

/** @} */

/**
 * @name int16 pairs
 * Two int16 in a word, the low element in bits 0-15: the SIMD32 instructions
 * on M4 / M7, plain C elsewhere.
 * @{
 */

/** The low element, sign-extended. */
FX_INLINE int32_t fx_lo16(int32_t x) { return (int16_t)x; }
/** The high element, sign-extended. */
FX_INLINE int32_t fx_hi16(int32_t x) { return x >> 16; }

/** lo and hi, each taken mod 2^16, as a pair. */
FX_INLINE int32_t fx_pack16(int32_t lo, int32_t hi) {
  return (int32_t)(((uint32_t)hi << 16) | ((uint32_t)lo & 0xFFFFU));
}

/** PKHBT: a's low half, b's low half on top. */
FX_INLINE int32_t fx_pkhbt(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("pkhbt %0, %1, %2, lsl #16" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return fx_pack16(a, b);
#endif
}

/** PKHTB: a's high half on top, b's high half below. */
FX_INLINE int32_t fx_pkhtb(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("pkhtb %0, %1, %2, asr #16" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return fx_pack16(fx_hi16(b), fx_hi16(a));
#endif
}

/** SMLAD: $acc + a_lo b_lo + a_hi b_hi$, wrapping. */
FX_INLINE int32_t fx_smlad(int32_t acc, int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smlad(a, b, acc);
#else
  return (int32_t)((uint32_t)acc + (uint32_t)(fx_lo16(a) * fx_lo16(b)) +
                   (uint32_t)(fx_hi16(a) * fx_hi16(b)));
#endif
}

/** SMLALD: fx_smlad() into 64 bits, exact. */
FX_INLINE int64_t fx_smlald(int64_t acc, int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smlald(a, b, acc);
#else
  return acc + (int64_t)(fx_lo16(a) * fx_lo16(b)) +
         (int64_t)(fx_hi16(a) * fx_hi16(b));
#endif
}

/** SMUAD: $a_lo b_lo + a_hi b_hi$; wraps only when all four are -32768. */
FX_INLINE int32_t fx_smuad(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smuad(a, b);
#else
  return fx_smlad(0, a, b);
#endif
}

/** SMUSDX: $a_lo b_hi - a_hi b_lo$, the imaginary part of a complex product. */
FX_INLINE int32_t fx_smusdx(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smusdx(a, b);
#else
  return fx_lo16(a) * fx_hi16(b) - fx_hi16(a) * fx_lo16(b);
#endif
}

/** QADD16: $a + b$ per half, saturated. */
FX_INLINE int32_t fx_qadd16(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __qadd16(a, b);
#else
  return fx_pack16(fx_ssat_c(fx_lo16(a) + fx_lo16(b), 16U),
                   fx_ssat_c(fx_hi16(a) + fx_hi16(b), 16U));
#endif
}

/** QSUB16: $a - b$ per half, saturated. */
FX_INLINE int32_t fx_qsub16(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __qsub16(a, b);
#else
  return fx_pack16(fx_ssat_c(fx_lo16(a) - fx_lo16(b), 16U),
                   fx_ssat_c(fx_hi16(a) - fx_hi16(b), 16U));
#endif
}

/** SHADD16: $(a + b) / 2$ per half, rounded down; never overflows. */
FX_INLINE int32_t fx_shadd16(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __shadd16(a, b);
#else
  return fx_pack16((fx_lo16(a) + fx_lo16(b)) >> 1,
                   (fx_hi16(a) + fx_hi16(b)) >> 1);
#endif
}

/** SXTB16: bytes 0 and 2, sign-extended into a pair. */
FX_INLINE int32_t fx_sxtb16(int32_t x) {
#if FX_HAVE_DSP
  return __sxtb16(x);
#else
  return fx_pack16((int8_t)x, (int8_t)(x >> 16));
#endif
}

#if FX_HAVE_DSP
/** Each half clamped to n-bit signed, $1 <= n <= 16$, n constant. */
#define FX_SSAT16(x, n) ((int32_t)__ssat16((x), (n)))
#else
/** Each half clamped to n-bit signed, $1 <= n <= 16$, n constant. */
#define FX_SSAT16(x, n)                                                        \
  fx_pack16(fx_ssat_c(fx_lo16(x), (n)), fx_ssat_c(fx_hi16(x), (n)))
#endif

/** SMMULR: high word of $a b$, rounded half up. */
FX_INLINE int32_t fx_smmulr(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("smmulr %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return (int32_t)((fx_smull(a, b) + 0x80000000LL) >> 32);
#endif
}

/** SMULWB: $a b_lo / 2^16$, rounded down. */
FX_INLINE int32_t fx_smulwb(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  int32_t r;
  __asm__("smulwb %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return (int32_t)(((int64_t)a * fx_lo16(b)) >> 16);
#endif
}

/** SMLAWB: $acc + a b_lo / 2^16$, wrapping. */
FX_INLINE int32_t fx_smlawb(int32_t acc, int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __smlawb(a, b, acc);
#else
  return (int32_t)((uint32_t)acc + (uint32_t)fx_smulwb(a, b));
#endif
}

/** @} */

#if FX_FPU_PATH
/**
 * @name Float
 * With an FPU, or on the host under `FX_EMULATE_FPU`.
 * @{
 */

/** $a b + c$ rounded once: VFMA on the FPU, libm's `fmaf` on the host. */
FX_INLINE float fx_fmaf(float a, float b, float c) {
  return __builtin_fmaf(a, b, c);
}

/** @} */
#endif

/**
 * @name Barriers
 * @{
 */

/** Orders accesses in the compiler only: enough between an interrupt and the
 * main loop on one core. */
FX_INLINE void fx_compiler_barrier(void) { __asm__ volatile("" ::: "memory"); }

#if FX_ARM
/** DMB: memory accesses complete in order. */
FX_INLINE void fx_dmb(void) { __asm__ volatile("dmb" ::: "memory"); }
/** DSB: memory accesses complete before the next instruction. */
FX_INLINE void fx_dsb(void) { __asm__ volatile("dsb" ::: "memory"); }
/** ISB: the pipeline refetches. */
FX_INLINE void fx_isb(void) { __asm__ volatile("isb" ::: "memory"); }
#else
/** DMB: memory accesses complete in order. */
FX_INLINE void fx_dmb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
/** DSB: memory accesses complete before the next instruction. */
FX_INLINE void fx_dsb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
/** ISB: the pipeline refetches. */
FX_INLINE void fx_isb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#endif

/** @} */

#endif /* FX_ARCH_H */

/**
 * @file test_fu_cobs.c
 * @brief Host tests for fu_cobs.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_cobs.h"
#include "test_stats.h"

/* the usual byte loop, shortest form */
static size_t ref_encode(uint8_t *d, const uint8_t *s, size_t n) {
  size_t code_at = 0U;
  size_t o = 1U;
  uint8_t code = 1U;
  for (size_t i = 0U; i < n; i++) {
    if (s[i] != 0U) {
      d[o++] = s[i];
      code++;
    }
    if (s[i] == 0U || code == 0xFFU) {
      d[code_at] = code;
      code = 1U;
      if (s[i] == 0U || i + 1U < n) {
        code_at = o++;
      } else {
        return o; /* a full block ends the input */
      }
    }
  }
  d[code_at] = code;
  return o;
}

static void check_vector(const uint8_t *in, size_t n, const uint8_t *want,
                         size_t wn) {
  uint8_t enc[600];
  uint8_t dec[600];
  size_t k = fu_cobs_encode(enc, in, n);
  ASSERT_EQ(k, wn);
  ASSERT_TRUE(memcmp(enc, want, wn) == 0);
  size_t len = 0U;
  ASSERT_TRUE(fu_cobs_decode(dec, enc, k, &len));
  ASSERT_EQ(len, n);
  ASSERT_TRUE(n == 0U || memcmp(dec, in, n) == 0);
}

/* the examples on Wikipedia's COBS page, without the delimiter */
void test_fu_cobs_vectors(void) {
  static const uint8_t z1[] = {0x00};
  static const uint8_t e1[] = {0x01, 0x01};
  static const uint8_t z2[] = {0x00, 0x00};
  static const uint8_t e2[] = {0x01, 0x01, 0x01};
  static const uint8_t a3[] = {0x00, 0x11, 0x00};
  static const uint8_t e3[] = {0x01, 0x02, 0x11, 0x01};
  static const uint8_t a4[] = {0x11, 0x22, 0x00, 0x33};
  static const uint8_t e4[] = {0x03, 0x11, 0x22, 0x02, 0x33};
  static const uint8_t a5[] = {0x11, 0x22, 0x33, 0x44};
  static const uint8_t e5[] = {0x05, 0x11, 0x22, 0x33, 0x44};
  static const uint8_t a6[] = {0x11, 0x00, 0x00, 0x00};
  static const uint8_t e6[] = {0x02, 0x11, 0x01, 0x01, 0x01};
  static const uint8_t e0[] = {0x01};
  check_vector(NULL, 0U, e0, 1U);
  check_vector(z1, 1U, e1, sizeof e1);
  check_vector(z2, 2U, e2, sizeof e2);
  check_vector(a3, 3U, e3, sizeof e3);
  check_vector(a4, 4U, e4, sizeof e4);
  check_vector(a5, 4U, e5, sizeof e5);
  check_vector(a6, 4U, e6, sizeof e6);

  /* the 254-byte block cases: 01..FE, 00 01..FE, 01..FF, 02..FF 00 */
  uint8_t in[256];
  uint8_t want[260];
  for (size_t i = 0U; i < 254U; i++) {
    in[i] = (uint8_t)(i + 1U);
  }
  want[0] = 0xFFU;
  memcpy(want + 1, in, 254U);
  check_vector(in, 254U, want, 255U);

  uint8_t in7[255];
  in7[0] = 0U;
  memcpy(in7 + 1, in, 254U);
  want[0] = 0x01U;
  want[1] = 0xFFU;
  memcpy(want + 2, in, 254U);
  check_vector(in7, 255U, want, 256U);

  for (size_t i = 0U; i < 255U; i++) {
    in[i] = (uint8_t)(i + 1U);
  }
  want[0] = 0xFFU;
  memcpy(want + 1, in, 254U);
  want[255] = 0x02U;
  want[256] = 0xFFU;
  check_vector(in, 255U, want, 257U);

  for (size_t i = 0U; i < 254U; i++) {
    in[i] = (uint8_t)(i + 2U);
  }
  in[254] = 0U;
  want[0] = 0xFFU;
  memcpy(want + 1, in, 254U);
  want[255] = 0x01U;
  want[256] = 0x01U;
  check_vector(in, 255U, want, 257U);
}

void test_fu_cobs_random(void) {
  static uint8_t in[1100];
  static uint8_t enc[1200];
  static uint8_t ref[1200];
  static uint8_t dec[1200];
  uint64_t st = 81U;
  for (int it = 0; it < 30000; it++) {
    uint32_t u = test_rng_u32(&st);
    size_t n = (u >> 8) % 1100U;
    uint32_t zeros = u % 5U; /* none, rare, 1 in 4, half, all */
    for (size_t i = 0U; i < n; i++) {
      uint32_t r = test_rng_u32(&st);
      bool z = zeros == 0U   ? false
               : zeros == 1U ? r % 300U == 0U
               : zeros == 2U ? r % 4U == 0U
               : zeros == 3U ? r % 2U == 0U
                             : true;
      in[i] = z ? 0U : (uint8_t)(1U + (r >> 8) % 255U);
    }
    size_t k = fu_cobs_encode(enc, in, n);
    ASSERT_TRUE(k <= FU_COBS_MAX(n));
    ASSERT_EQ(k, ref_encode(ref, in, n));
    ASSERT_TRUE(memcmp(enc, ref, k) == 0);
    for (size_t i = 0U; i < k; i++) {
      ASSERT_TRUE(enc[i] != 0U);
    }
    size_t len = 0U;
    ASSERT_TRUE(fu_cobs_decode(dec, enc, k, &len));
    ASSERT_EQ(len, n);
    ASSERT_TRUE(n == 0U || memcmp(dec, in, n) == 0);
    /* a 0 anywhere in an encoding is malformed */
    size_t j = (size_t)(u % k);
    uint8_t keep = enc[j];
    enc[j] = 0U;
    ASSERT_TRUE(!fu_cobs_decode(dec, enc, k, &len));
    enc[j] = keep;
    /* in place */
    ASSERT_TRUE(fu_cobs_decode(enc, enc, k, &len));
    ASSERT_EQ(len, n);
    ASSERT_TRUE(n == 0U || memcmp(enc, in, n) == 0);
  }
}

void test_fu_cobs_malformed(void) {
  uint8_t d[16];
  size_t len = 77U;
  static const uint8_t zero_code[] = {0x02, 0x11, 0x00, 0x22};
  static const uint8_t zero_data[] = {0x05, 0x11, 0x00, 0x33, 0x44};
  static const uint8_t zero_word[] = {0x09, 1, 2, 3, 0, 5, 6, 7, 8};
  static const uint8_t too_long[] = {0x05, 0x11, 0x22};
  ASSERT_TRUE(!fu_cobs_decode(d, zero_code, sizeof zero_code, &len));
  ASSERT_TRUE(!fu_cobs_decode(d, zero_data, sizeof zero_data, &len));
  ASSERT_TRUE(!fu_cobs_decode(d, zero_word, sizeof zero_word, &len));
  ASSERT_TRUE(!fu_cobs_decode(d, too_long, sizeof too_long, &len));
  ASSERT_EQ(len, 77U); /* untouched on failure */
  ASSERT_TRUE(fu_cobs_decode(d, NULL, 0U, &len)); /* empty input: no frame */
  ASSERT_EQ(len, 0U);
}

/* frames fed one byte at a time, some cut short, against the block decoder */
void test_fu_cobs_rx(void) {
  static uint8_t in[300];
  static uint8_t enc[320];
  static uint8_t ref[320];
  uint8_t buf[256];
  FU_CobsRx rx;
  fu_cobs_rx_init(&rx, buf, sizeof buf);
  uint64_t st = 91U;
  uint32_t errors = 0U;
  for (int it = 0; it < 20000; it++) {
    uint32_t u = test_rng_u32(&st);
    size_t n = (u >> 8) % 300U;
    uint32_t every = (u & 0x80U) != 0U ? 7U : 1000U; /* or 254-byte blocks */
    for (size_t i = 0U; i < n; i++) {
      uint32_t r = test_rng_u32(&st);
      in[i] = r % every == 0U ? 0U : (uint8_t)(1U + r % 255U);
    }
    size_t k = fu_cobs_encode(enc, in, n);
    if (u % 5U == 0U && k > 1U) {
      k = 1U + (u >> 4) % (k - 1U); /* lost bytes */
    }
    size_t want = 0U;
    bool ok = fu_cobs_decode(ref, enc, k, &want);
    bool good = ok && want <= sizeof buf;
    errors += good ? 0U : 1U;

    size_t got = 0U;
    for (size_t i = 0U; i < k; i++) {
      ASSERT_TRUE(!fu_cobs_rx_byte(&rx, enc[i], &got));
    }
    ASSERT_EQ(fu_cobs_rx_byte(&rx, 0U, &got), good);
    if (good) {
      ASSERT_EQ(got, want);
      ASSERT_TRUE(want == 0U || memcmp(buf, ref, want) == 0);
    }
    if (u % 17U == 0U) { /* idle 0s between frames: no frame, no error */
      ASSERT_TRUE(!fu_cobs_rx_byte(&rx, 0U, &got));
    }
    ASSERT_EQ(rx.errors, errors);
  }
  ASSERT_TRUE(errors > 1000U);
}

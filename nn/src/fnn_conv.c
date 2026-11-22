/**
 * @file fnn_conv.c
 * @brief Convolution layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_conv.h"
#include "fnn_copy.h"
#include "fnn_dense.h"
#include "fnn_internal.h"
#include "fx_arch.h"

static int fits(uint32_t in, uint32_t out, uint32_t k, uint32_t s, uint32_t p) {
  if (in == 0U || out == 0U || k == 0U || s == 0U || p >= k) {
    return 0;
  }
  /* The last window starts inside the input, and ends less than k past it. */
  return (uint64_t)(out - 1U) * s < (uint64_t)in + p;
}

FNN_Status fnn_conv_check(const FNN_Conv *l) {
  if (!fits(l->in.h, l->out.h, l->kh, l->sh, l->pt) ||
      !fits(l->in.w, l->out.w, l->kw, l->sw, l->pl) || l->in.c == 0U ||
      l->out.c == 0U || (l->q.n != 1U && l->q.n != l->out.c)) {
    return FNN_ERR_SHAPE;
  }
  if (l->zx < -128 || l->zx > 127) {
    return FNN_ERR_RANGE;
  }
  return FNN_OK;
}

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/* Copies the patch under the filter, one run per row, with the zero point
 * where it overhangs. */
static void patch(const FNN_Conv *l, const int8_t *x, int32_t iy, int32_t ix,
                  int8_t *p) {
  uint32_t c = l->in.c, run = l->kw * c;
  uint32_t kx0 = (uint32_t)clampi(-ix, 0, (int32_t)l->kw);
  uint32_t kx1 = (uint32_t)clampi((int32_t)l->in.w - ix, 0, (int32_t)l->kw);
  for (uint32_t ky = 0U; ky < l->kh; ky++, p += run) {
    int32_t y = iy + (int32_t)ky;
    if (y < 0 || y >= (int32_t)l->in.h) {
      fnn_fill(p, l->zx, run);
      continue;
    }
    const int8_t *row = x + ((uint32_t)y * l->in.w + (uint32_t)(ix + (int32_t)kx0)) * c;
    fnn_fill(p, l->zx, kx0 * c);
    fnn_copy(p + kx0 * c, row, (kx1 - kx0) * c);
    fnn_fill(p + kx1 * c, l->zx, (l->kw - kx1) * c);
  }
}

void fnn_conv_s8(const FNN_Conv *l, const int8_t *x, int8_t *y,
                 int8_t *scratch) {
  uint32_t c = l->in.c, m = l->out.c;
  /* Each output pixel is this dense layer over its patch. */
  const FNN_Dense d = {l->w, l->b, l->kh * l->kw * c, m, l->packed, l->q, l->o};
  for (uint32_t oy = 0U; oy < l->out.h; oy++) {
    int32_t iy = (int32_t)(oy * l->sh) - (int32_t)l->pt;
    int rows_in = iy >= 0 && iy + (int32_t)l->kh <= (int32_t)l->in.h;
    for (uint32_t ox = 0U; ox < l->out.w; ox++) {
      int32_t ix = (int32_t)(ox * l->sw) - (int32_t)l->pl;
      const int8_t *p;
      /* A window inside the input whose rows are one run needs no copy. */
      if (rows_in && ix >= 0 && ix + (int32_t)l->kw <= (int32_t)l->in.w &&
          (l->kh == 1U || l->kw == l->in.w)) {
        p = x + ((uint32_t)iy * l->in.w + (uint32_t)ix) * c;
      } else {
        patch(l, x, iy, ix, scratch);
        p = scratch;
      }
      FNN_DENSE_CONV(&d, p, y);
      y += m;
    }
  }
}

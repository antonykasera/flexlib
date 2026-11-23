/**
 * @file fnn_fused.c
 * @brief Layer pairs run together
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_fused.h"
#include "fnn_copy.h"

FNN_Status fnn_dwpw_check(const FNN_DwConv *dw, const FNN_Conv *pw) {
  FNN_Status s = fnn_dwconv_check(dw);
  if (s == FNN_OK) {
    s = fnn_conv_check(pw);
  }
  if (s != FNN_OK) {
    return s;
  }
  if (dw->sh != 1U || dw->sw != 1U || dw->out.h != dw->in.h ||
      dw->out.w != dw->in.w || pw->kh != 1U || pw->kw != 1U || pw->sh != 1U ||
      pw->sw != 1U || pw->pt != 0U || pw->pl != 0U || pw->in.h != dw->out.h ||
      pw->in.w != dw->out.w || pw->in.c != dw->out.c || pw->out.h != pw->in.h ||
      pw->out.w != pw->in.w) {
    return FNN_ERR_SHAPE;
  }
  return FNN_OK;
}

FNN_Status fnn_convpool_check(const FNN_Conv *c, const FNN_Pool *p) {
  FNN_Status s = fnn_conv_check(c);
  if (s == FNN_OK) {
    s = fnn_pool_check(p);
  }
  if (s != FNN_OK) {
    return s;
  }
  if (p->in.h != c->out.h || p->in.w != c->out.w || p->in.c != c->out.c) {
    return FNN_ERR_SHAPE;
  }
  return FNN_OK;
}

void fnn_convpool_s8(const FNN_Conv *c, const FNN_Pool *p, const int8_t *x,
                     int8_t *y, int8_t *scratch) {
  uint32_t row = c->in.w * c->in.c, out_row = p->out.w * p->out.c;
  int8_t *patch = scratch + p->kh * c->out.w * c->out.c;
  FNN_Conv rows = *c;
  FNN_Pool one = *p;
  one.out.h = 1U;
  one.pt = 0U;
  for (uint32_t r = 0U; r < p->out.h; r++) {
    /* Conv rows r0 to r1 - 1 feed pooled row r. */
    uint32_t top = r * p->sh, r0 = top > p->pt ? top - p->pt : 0U;
    uint32_t r1 =
        top + p->kh - p->pt < c->out.h ? top + p->kh - p->pt : c->out.h;
    uint32_t iy = r0 * c->sh, a = iy > c->pt ? iy - c->pt : 0U;
    rows.in.h = c->in.h - a;
    rows.out.h = r1 - r0;
    rows.pt = c->pt > iy ? c->pt - iy : 0U;
    fnn_conv_s8(&rows, x + a * row, scratch, patch);
    one.in.h = r1 - r0;
    fnn_maxpool_s8(&one, scratch, y + r * out_row);
  }
}

void fnn_dwpw_s8(const FNN_DwConv *dw, const FNN_Conv *pw, const int8_t *x,
                 int8_t *y, int8_t *scratch) {
  uint32_t h = dw->in.h, kh = dw->kh, pt = dw->pt;
  uint32_t row = dw->in.w * dw->in.c, out_row = pw->out.w * pw->out.c;
  int8_t *d = scratch + kh * row;
  FNN_DwConv one = *dw;
  FNN_Conv pone = *pw;
  one.out.h = 1U;
  pone.in.h = 1U;
  pone.out.h = 1U;
  /* Rows lo to hi - 1 of x, kept here because y may overwrite them. */
  uint32_t lo = 0U, hi = 0U;
  for (uint32_t r = 0U; r < h; r++) {
    uint32_t want_lo = r > pt ? r - pt : 0U;
    uint32_t want_hi = r + kh - pt < h ? r + kh - pt : h;
    if (want_lo > lo) {
      uint32_t drop = (want_lo - lo) * row, keep = (hi - want_lo) * row;
      /* The rows move down, which a forward copy allows. */
      fnn_copy(scratch, scratch + drop, keep);
      lo = want_lo;
    }
    for (; hi < want_hi; hi++) {
      fnn_copy(scratch + (hi - lo) * row, x + hi * row, row);
    }
    one.in.h = hi - lo;
    one.pt = pt > r ? pt - r : 0U;
    fnn_dwconv_s8(&one, scratch, d);
    fnn_conv_s8(&pone, d, y + r * out_row, NULL);
  }
}

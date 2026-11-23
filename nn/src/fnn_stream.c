/**
 * @file fnn_stream.c
 * @brief Causal 1-D layers run one input column at a time
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_stream.h"
#include "fnn_copy.h"
#include "fnn_internal.h"
#include <stddef.h>

FNN_Status fnn_stream_init(FNN_Stream *s, int8_t *buf, uint32_t len,
                           uint32_t n, uint32_t c, uint32_t stride) {
  if (n == 0U || c == 0U || stride == 0U || len < n) {
    return FNN_ERR_SHAPE;
  }
  s->buf = buf;
  s->len = len;
  s->n = n;
  s->c = c;
  s->stride = stride;
  fnn_stream_reset(s);
  return FNN_OK;
}

void fnn_stream_reset(FNN_Stream *s) {
  s->end = 0U;
  s->due = s->n;
}

const int8_t *fnn_stream_push(FNN_Stream *s, const int8_t *x) {
  uint32_t c = s->c;
  if (s->end == s->len) {
    /* The source lies past the destination, which a forward copy allows. */
    fnn_copy(s->buf, s->buf + (s->len - s->n + 1U) * c, (s->n - 1U) * c);
    s->end = s->n - 1U;
  }
  fnn_copy(s->buf + s->end * c, x, c);
  s->end++;
  if (--s->due != 0U) {
    return NULL;
  }
  s->due = s->stride;
  return s->buf + (s->end - s->n) * c;
}

FNN_Status fnn_stream_conv1d_check(const FNN_Conv *l, const FNN_Stream *s) {
  FNN_Status st = fnn_conv_check(l);
  if (st != FNN_OK) {
    return st;
  }
  if (l->in.h != 1U || l->kh != 1U || l->pl != 0U ||
      s->n != l->kw || s->c != l->in.c || s->stride != l->sw) {
    return FNN_ERR_SHAPE;
  }
  return FNN_OK;
}

uint32_t fnn_stream_conv1d_s8(const FNN_Conv *l, FNN_Stream *s,
                              const int8_t *x, int8_t *y) {
  const int8_t *win = fnn_stream_push(s, x);
  if (win == NULL) {
    return 0U;
  }
  const FNN_Dense d = {l->w, l->b, l->kw * l->in.c, l->out.c,
                       l->packed, l->q, l->o};
  FNN_DENSE_CONV(&d, win, y);
  return 1U;
}

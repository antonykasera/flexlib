/**
 * @file fnn_internal.h
 * @brief What flexnn's layers share and its users do not see
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_INTERNAL_H
#define FNN_INTERNAL_H

#include "fnn_dense.h"
#include "fnn_quant.h"

/* A convolution's pixels are dense layers with the convolution's rounding. */
#if FNN_RULE_FC != FNN_RULE
void fnn_dense_conv_s8(const FNN_Dense *l, const int8_t *x, int8_t *y);
#define FNN_DENSE_CONV fnn_dense_conv_s8
#else
#define FNN_DENSE_CONV fnn_dense_s8
#endif

#endif /* FNN_INTERNAL_H */

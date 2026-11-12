/**
 * @file fx_version.h
 * @brief flexlib release version; the .mk fragments read it from here
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FX_VERSION_H
#define FX_VERSION_H

#define FLEXLIB_VERSION_MAJOR 0
#define FLEXLIB_VERSION_MINOR 4
#define FLEXLIB_VERSION_PATCH 0

/* 0.2.1 -> 201 */
#define FLEXLIB_VERSION_NUM                                                    \
  (FLEXLIB_VERSION_MAJOR * 10000 + FLEXLIB_VERSION_MINOR * 100 +               \
   FLEXLIB_VERSION_PATCH)

#define FX_STR_(x) #x
#define FX_STR(x) FX_STR_(x)
#define FLEXLIB_VERSION                                                        \
  FX_STR(FLEXLIB_VERSION_MAJOR)                                                \
  "." FX_STR(FLEXLIB_VERSION_MINOR) "." FX_STR(FLEXLIB_VERSION_PATCH)

#endif /* FX_VERSION_H */

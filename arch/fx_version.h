/**
 * @file fx_version.h
 * @brief flexlib release version; the .mk fragments read it from here
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FX_VERSION_H
#define FX_VERSION_H

/** Major version. */
#define FLEXLIB_VERSION_MAJOR 0
/** Minor version. */
#define FLEXLIB_VERSION_MINOR 11
/** Patch version. */
#define FLEXLIB_VERSION_PATCH 1

/** The version as one number */
#define FLEXLIB_VERSION_NUM                                                    \
  (FLEXLIB_VERSION_MAJOR * 10000 + FLEXLIB_VERSION_MINOR * 100 +               \
   FLEXLIB_VERSION_PATCH)

/** @cond */
#define FX_STR_(x) #x
#define FX_STR(x) FX_STR_(x)
/** @endcond */
/** The version as a string, "MAJOR.MINOR.PATCH". */
#define FLEXLIB_VERSION                                                        \
  FX_STR(FLEXLIB_VERSION_MAJOR)                                                \
  "." FX_STR(FLEXLIB_VERSION_MINOR) "." FX_STR(FLEXLIB_VERSION_PATCH)

#endif /* FX_VERSION_H */

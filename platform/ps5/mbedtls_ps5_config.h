/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/*
 * Use mbedTLS's interoperable client/X.509 profile for Sunshine NVHTTP.
 * The generic net_sockets module is deliberately omitted: native builds use
 * the already-validated sceNet-backed adapter in platform/ps5/ps5_sockets.c.
 */
#include <mbedtls/mbedtls_config.h>

#undef MBEDTLS_NET_C
#undef MBEDTLS_DEBUG_C
#undef MBEDTLS_FS_IO
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_SELF_TEST

/* The minimal native runtime does not ship compiler-rt's 128-bit divider. */
#define MBEDTLS_NO_UDBL_DIVISION

/* Native PS5 entropy and pthread-backed PSA global-state protection. */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_PTHREAD

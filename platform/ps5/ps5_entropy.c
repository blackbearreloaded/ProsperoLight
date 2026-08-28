/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <mbedtls/entropy.h>

#include <stddef.h>
#include <string.h>

int sysctlbyname(const char *name, void *old_value, size_t *old_length,
                 const void *new_value, size_t new_length);

int ps5_get_random(void *output, size_t length)
{
    unsigned char block[64];
    unsigned char *destination = output;

    if (!output && length != 0)
        return -1;

    while (length != 0) {
        size_t block_length = sizeof(block);
        size_t copy_length = length < sizeof(block) ? length : sizeof(block);

        if (sysctlbyname("kern.rng_pseudo", block, &block_length, NULL, 0) != 0 ||
            block_length < copy_length) {
            memset(block, 0, sizeof(block));
            return -1;
        }

        memcpy(destination, block, copy_length);
        destination += copy_length;
        length -= copy_length;
    }

    memset(block, 0, sizeof(block));
    return 0;
}

int mbedtls_hardware_poll(void *context, unsigned char *output, size_t length,
                          size_t *output_length)
{
    (void)context;
    if (!output || !output_length ||
        ps5_get_random(output, length) != 0) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    *output_length = length;
    return 0;
}

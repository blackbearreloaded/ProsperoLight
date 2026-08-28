/*
 * ps5-native-app-boilerplate / ProsperoLight - PNG decoder link stub.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The executable imports these symbols from the system libScePngDec module.
 * These bodies exist only in the host-side link stub and are never packaged
 * or executed on the console.
 */

#include <stdint.h>

int scePngDecQueryMemorySize(void *param)
{
    (void)param;
    return -1;
}

int scePngDecCreate(void *param, void *memory, uint32_t memory_size, void **handle)
{
    (void)param;
    (void)memory;
    (void)memory_size;
    (void)handle;
    return -1;
}

int scePngDecDecode(void *handle, void *param, void *info)
{
    (void)handle;
    (void)param;
    (void)info;
    return -1;
}

int scePngDecDelete(void *handle)
{
    (void)handle;
    return -1;
}

int scePngDecParseHeader(void *param, void *info)
{
    (void)param;
    (void)info;
    return -1;
}

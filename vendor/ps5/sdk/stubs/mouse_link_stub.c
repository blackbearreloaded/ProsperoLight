/*
 * ps5-native-app-boilerplate / ProsperoLight - Mouse link stub.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * These bodies are link-time declarations only. The native module writer
 * replaces them with public libSceMouse imports in the packaged executable.
 */

#include <stdint.h>

int32_t sceMouseInit(void)
{
    return -1;
}

int32_t sceMouseOpen(int32_t user_id, int32_t type, int32_t index, const void *parameter)
{
    (void)user_id;
    (void)type;
    (void)index;
    (void)parameter;
    return -1;
}

int32_t sceMouseRead(int32_t handle, void *data, int32_t count)
{
    (void)handle;
    (void)data;
    (void)count;
    return -1;
}

int32_t sceMouseClose(int32_t handle)
{
    (void)handle;
    return -1;
}

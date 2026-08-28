/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*gs_log_sink_t)(const char *message);

    void gs_log_set_sink(gs_log_sink_t sink);
    void gs_logf(const char *level, const char *format, ...);

#define LOGI(...) gs_logf("I", __VA_ARGS__)
#define LOGW(...) gs_logf("W", __VA_ARGS__)
#define LOGE(...) gs_logf("E", __VA_ARGS__)

#ifdef __cplusplus
}
#endif

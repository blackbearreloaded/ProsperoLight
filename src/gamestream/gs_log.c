/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gs_log.h"

#include <stdarg.h>
#include <stdio.h>

static gs_log_sink_t log_sink;

void gs_log_set_sink(gs_log_sink_t sink)
{
    log_sink = sink;
}

void gs_logf(const char *level, const char *format, ...)
{
    char message[768];
    int prefix;
    va_list arguments;

    if (!log_sink)
        return;
    prefix = snprintf(message, sizeof(message), "NVHTTP[%s] ", level);
    if (prefix < 0 || prefix >= (int)sizeof(message))
        return;
    va_start(arguments, format);
    (void)vsnprintf(message + prefix, sizeof(message) - (size_t)prefix, format, arguments);
    va_end(arguments);
    log_sink(message);
}

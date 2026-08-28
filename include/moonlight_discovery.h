/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOONLIGHT_DISCOVERY_H
#define MOONLIGHT_DISCOVERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOONLIGHT_DISCOVERY_MAX_HOSTS 8U

typedef struct moonlight_discovered_host {
    char address[64];
    char name[64];
} moonlight_discovered_host_t;

uint32_t moonlight_discover_hosts(moonlight_discovered_host_t *hosts,
                                  uint32_t capacity);

#ifdef __cplusplus
}
#endif

#endif

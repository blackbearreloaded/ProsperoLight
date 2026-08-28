/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MLGPU_NATIVE_AGC_PRESENT_H
#define MLGPU_NATIVE_AGC_PRESENT_H

#include <stddef.h>
#include <stdint.h>

typedef struct native_agc_metrics
{
    uint32_t video_codec;
    uint32_t total_fps_x100;
    uint32_t incoming_fps_x100;
    uint32_t rendering_fps_x100;
    uint32_t network_drop_percent_x100;
    uint32_t rtt_ms;
    uint32_t rtt_variance_ms;
    uint32_t rtt_valid;
    uint32_t host_min_tenths_ms;
    uint32_t host_max_tenths_ms;
    uint32_t host_average_tenths_ms;
    uint64_t decode_average_us;
} native_agc_metrics_t;

int native_agc_present_nv12(const void *source, size_t source_bytes, uint32_t pitch,
                            uint32_t surface_height, uint32_t visible_width,
                            uint32_t visible_height, const native_agc_metrics_t *metrics);
int native_agc_present_main10(const void *source, size_t source_bytes, uint32_t pitch,
                              uint32_t surface_height, uint32_t visible_width,
                              uint32_t visible_height, const native_agc_metrics_t *metrics);
int native_agc_present_loading(void *surface, size_t surface_bytes, uint32_t phase, int hdr);
void native_agc_set_hud_enabled(int enabled);
int native_agc_hud_enabled(void);
void native_agc_set_tv_safe_area(int enabled);
int native_agc_present_shutdown(void);

#endif

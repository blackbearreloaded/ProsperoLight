/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/* PS5 PNG decoder ABI mirrored from the hardware-tested SharpProspero
   interop definitions in this workspace. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScePngDecCreateParam {
    uint32_t this_size;
    uint32_t attribute;
    uint32_t max_image_width;
} ScePngDecCreateParam;

typedef struct ScePngDecDecodeParam {
    const void *png_mem_addr;
    void *image_mem_addr;
    uint32_t png_mem_size;
    uint32_t image_mem_size;
    uint16_t pixel_format;
    uint16_t alpha_value;
    uint32_t image_pitch;
} ScePngDecDecodeParam;

typedef struct ScePngDecParseParam {
    const void *png_mem_addr;
    uint32_t png_mem_size;
    uint32_t reserved0;
} ScePngDecParseParam;

typedef struct ScePngDecImageInfo {
    uint32_t image_width;
    uint32_t image_height;
    uint16_t color_space;
    uint16_t bit_depth;
    uint32_t image_flag;
} ScePngDecImageInfo;

int scePngDecQueryMemorySize(ScePngDecCreateParam *param);
int scePngDecCreate(ScePngDecCreateParam *param, void *memory,
                    uint32_t memory_size, void **handle);
int scePngDecDecode(void *handle, ScePngDecDecodeParam *param,
                    ScePngDecImageInfo *info);
int scePngDecDelete(void *handle);
int scePngDecParseHeader(ScePngDecParseParam *param,
                         ScePngDecImageInfo *info);

#ifdef __cplusplus
}
#endif

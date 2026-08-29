/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

struct NativeAgcOutputGeometry
{
    std::uint32_t width;
    std::uint32_t height;
    std::size_t framebuffer_bytes;
};

constexpr NativeAgcOutputGeometry native_agc_output_geometry(std::uint32_t source_width,
                                                             std::uint32_t source_height)
{
    return source_width > 1920u || source_height > 1080u
               ? NativeAgcOutputGeometry{3840u, 2160u, 0x2000000u}
               : NativeAgcOutputGeometry{1920u, 1080u, 0x0a00000u};
}

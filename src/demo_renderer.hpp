/*
 * ps5-native-app-boilerplate - Small CPU-rendered demonstration API.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Keeps PS5 VideoOut setup and the bitmap font out of the starter main file.
 */

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ps5::demo
{
enum class Color : std::uint32_t
{
    background = UINT32_C(0xff190d0a),
    panel = UINT32_C(0xff301f17),
    white = UINT32_C(0xffffffff),
    cyan = UINT32_C(0xffffff00),
    magenta = UINT32_C(0xffff00ff),
    yellow = UINT32_C(0xff00ffff),
};

class Canvas;
using DrawScene = void (*)(Canvas &) noexcept;
[[noreturn]] void run(DrawScene draw, std::string_view ready_message) noexcept;

class Canvas final
{
  public:
    void clear(Color color) noexcept;
    void rectangle(unsigned x, unsigned y, unsigned width, unsigned height, Color color) noexcept;
    void circle(unsigned center_x, unsigned center_y, unsigned radius, Color color) noexcept;
    void triangle(unsigned center_x, unsigned top, unsigned half_width, unsigned height,
                  Color color) noexcept;
    void text(unsigned x, unsigned y, std::string_view value, unsigned scale, Color color) noexcept;

  private:
    explicit Canvas(std::uint32_t *pixels) noexcept : pixels_{pixels}
    {
    }

    std::uint32_t *pixels_;

    friend void run(DrawScene draw, std::string_view ready_message) noexcept;
};

void read_asset_text(const char *path, std::span<char> destination,
                     std::string_view fallback) noexcept;
} // namespace ps5::demo

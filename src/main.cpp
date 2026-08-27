/*
 * ps5-native-app-boilerplate - Minimal native application example.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Draws a small CPU-rendered scene and keeps the process alive for normal
 * shell-mediated closure.
 */

#include "demo_renderer.hpp"

#include <array>
#include <span>

namespace
{
std::array<char, 48> banner{};

void draw_scene(ps5::demo::Canvas &canvas) noexcept
{
    using ps5::demo::Color;

    canvas.clear(Color::background);
    canvas.rectangle(120, 430, 500, 470, Color::panel);
    canvas.rectangle(710, 430, 500, 470, Color::panel);
    canvas.rectangle(1300, 430, 500, 470, Color::panel);
    canvas.rectangle(120, 375, 1680, 8, Color::white);

    canvas.text(120, 90, "HELLO WORLD", 14, Color::white);
    canvas.text(120, 245, "MODERN CXX20 RAII", 6, Color::white);
    canvas.text(120, 315, banner.data(), 5, Color::cyan);

    canvas.circle(370, 665, 130, Color::cyan);
    canvas.rectangle(840, 535, 240, 240, Color::yellow);
    canvas.triangle(1550, 520, 170, 285, Color::magenta);

    canvas.text(250, 830, "CIRCLE", 5, Color::white);
    canvas.text(870, 830, "SQUARE", 5, Color::white);
    canvas.text(1420, 830, "TRIANGLE", 5, Color::white);
}
} // namespace

int main()
{
    ps5::demo::read_asset_text("/app0/assets/banner.txt", std::span{banner}, "APP0 ASSET FAILED");
    ps5::demo::run(draw_scene, banner.data());
}

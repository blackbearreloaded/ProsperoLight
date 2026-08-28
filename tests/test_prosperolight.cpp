/*
 * ps5-native-app-boilerplate / ProsperoLight - Stream shortcut tests.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "moonlight_stream_input.hpp"

#include <gtest/gtest.h>

namespace
{
TEST(StreamShortcuts, SelectL1ReturnsToLauncher)
{
    EXPECT_TRUE(
        moonlight_stream_disconnect_requested(MOONLIGHT_PS5_PAD_SELECT | MOONLIGHT_PS5_PAD_L1));
    EXPECT_FALSE(moonlight_stream_disconnect_requested(MOONLIGHT_PS5_PAD_SELECT));
}

TEST(StreamShortcuts, SelectR1TogglesMetrics)
{
    EXPECT_TRUE(
        moonlight_stream_hud_toggle_requested(MOONLIGHT_PS5_PAD_SELECT | MOONLIGHT_PS5_PAD_R1));
    EXPECT_FALSE(moonlight_stream_hud_toggle_requested(MOONLIGHT_PS5_PAD_R1));
}

TEST(StreamShortcuts, OptionsReleaseAfterLongPressTogglesMouseMode)
{
    constexpr uint64_t started_at = 1000;
    constexpr uint64_t released_at = started_at + MOONLIGHT_MOUSE_EMULATION_LONG_PRESS_US + 1;

    EXPECT_TRUE(moonlight_stream_mouse_toggle_released(MOONLIGHT_PS5_PAD_OPTIONS, 0, started_at,
                                                       released_at));
    EXPECT_FALSE(moonlight_stream_mouse_toggle_released(
        MOONLIGHT_PS5_PAD_OPTIONS, 0, started_at,
        started_at + MOONLIGHT_MOUSE_EMULATION_LONG_PRESS_US));
    EXPECT_FALSE(moonlight_stream_mouse_toggle_released(
        MOONLIGHT_PS5_PAD_OPTIONS, MOONLIGHT_PS5_PAD_OPTIONS, started_at, released_at));
}

TEST(StreamShortcuts, MouseAxisHasDeadzoneAndDirection)
{
    EXPECT_EQ(moonlight_stream_mouse_axis_delta(0), 0);
    EXPECT_GT(moonlight_stream_mouse_axis_delta(32766), 0);
    EXPECT_LT(moonlight_stream_mouse_axis_delta(-32766), 0);
}
} // namespace

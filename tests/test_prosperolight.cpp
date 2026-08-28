/*
 * ps5-native-app-boilerplate / ProsperoLight - Stream shortcut tests.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "moonlight_stream_input.hpp"
#include "moonlight_config.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C"
{
    int sceKernelOpen(const char *, int, std::uint16_t)
    {
        return -1;
    }

    int sceKernelClose(int)
    {
        return 0;
    }

    std::int64_t sceKernelRead(int, void *, std::size_t)
    {
        return -1;
    }

    std::int64_t sceKernelWrite(int, const void *, std::size_t)
    {
        return -1;
    }

    int sceKernelRename(const char *, const char *)
    {
        return -1;
    }

    int sceKernelUnlink(const char *)
    {
        return -1;
    }
}

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

TEST(StreamShortcuts, SelectTriangleOpensKeyboard)
{
    EXPECT_TRUE(
        moonlight_stream_keyboard_requested(MOONLIGHT_PS5_PAD_SELECT | MOONLIGHT_PS5_PAD_TRIANGLE));
    EXPECT_FALSE(moonlight_stream_keyboard_requested(MOONLIGHT_PS5_PAD_TRIANGLE));
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

TEST(Configuration, DefaultsMatchLauncherDefaults)
{
    moonlight_config_t config{};
    moonlight_config_defaults(&config);

    EXPECT_EQ(config.host_count, 0U);
    EXPECT_EQ(config.bitrate_mbps, 20U);
    EXPECT_EQ(config.display_area, MOONLIGHT_DISPLAY_AREA_FULL);
    EXPECT_EQ(config.video_codec, MOONLIGHT_VIDEO_CODEC_H264);
    EXPECT_EQ(config.stream_resolution, MOONLIGHT_STREAM_RESOLUTION_1080P);
    EXPECT_EQ(config.hdr_enabled, 0U);
}

TEST(Configuration, UpsertUpdatesAHostByStableIdentity)
{
    moonlight_config_t config{};
    moonlight_config_defaults(&config);

    EXPECT_EQ(moonlight_config_upsert_host(&config, "192.168.1.10", "Gaming PC", "host-1", false),
              0);
    EXPECT_EQ(moonlight_config_upsert_host(&config, "192.168.1.20", "", "host-1", true), 0);
    ASSERT_EQ(config.host_count, 1U);
    EXPECT_STREQ(config.hosts[0].address, "192.168.1.20");
    EXPECT_STREQ(config.hosts[0].name, "Gaming PC");
    EXPECT_STREQ(config.hosts[0].unique_id, "host-1");
    EXPECT_EQ(config.hosts[0].manual, 1U);
}

TEST(Configuration, UpsertRejectsAHostBeyondCapacity)
{
    moonlight_config_t config{};
    moonlight_config_defaults(&config);
    for (std::uint32_t index = 0; index < MOONLIGHT_CONFIG_MAX_HOSTS; ++index)
    {
        char address[MOONLIGHT_CONFIG_ADDRESS_SIZE]{};
        std::snprintf(address, sizeof(address), "192.168.1.%u", index + 1);
        ASSERT_EQ(moonlight_config_upsert_host(&config, address, "PC", "", false),
                  static_cast<int>(index));
    }

    EXPECT_EQ(moonlight_config_upsert_host(&config, "192.168.1.99", "Extra", "", false), -1);
    EXPECT_EQ(config.host_count, MOONLIGHT_CONFIG_MAX_HOSTS);
}

TEST(Configuration, FailedLoadLeavesSafeDefaults)
{
    moonlight_config_t config{};
    std::memset(&config, 0xff, sizeof(config));

    EXPECT_FALSE(moonlight_config_load(&config));
    EXPECT_EQ(config.host_count, 0U);
    EXPECT_EQ(config.bitrate_mbps, 20U);
    EXPECT_EQ(config.display_area, MOONLIGHT_DISPLAY_AREA_FULL);
}
} // namespace

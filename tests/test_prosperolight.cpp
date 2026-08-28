/*
 * ps5-native-app-boilerplate / ProsperoLight - Stream shortcut tests.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lan_http_report.hpp"
#include "moonlight_stream_input.hpp"
#include "moonlight_stream_keyboard.hpp"
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
TEST(LanTelemetry, NormalBuildIsAnImmediateNoOp)
{
    lan_http_report_set_host("203.0.113.1");
    EXPECT_EQ(lan_http_report_text("must not open a socket"), 0);
}

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

TEST(StreamShortcuts, SelectTriangleTogglesStreamKeyboard)
{
    EXPECT_TRUE(
        moonlight_stream_keyboard_requested(MOONLIGHT_PS5_PAD_SELECT | MOONLIGHT_PS5_PAD_TRIANGLE));
    EXPECT_FALSE(moonlight_stream_keyboard_requested(MOONLIGHT_PS5_PAD_TRIANGLE));
}

TEST(StreamShortcuts, SelectSquareTogglesMouseMode)
{
    EXPECT_TRUE(moonlight_stream_mouse_toggle_requested(MOONLIGHT_PS5_PAD_SELECT |
                                                        MOONLIGHT_PS5_PAD_SQUARE));
    EXPECT_FALSE(moonlight_stream_mouse_toggle_requested(MOONLIGHT_PS5_PAD_SQUARE));
    EXPECT_FALSE(moonlight_stream_mouse_toggle_requested(MOONLIGHT_PS5_PAD_SELECT));
}

TEST(StreamShortcuts, MouseAxisHasDeadzoneAndDirection)
{
    EXPECT_EQ(moonlight_stream_mouse_axis_delta(0), 0);
    EXPECT_GT(moonlight_stream_mouse_axis_delta(32766), 0);
    EXPECT_LT(moonlight_stream_mouse_axis_delta(-32766), 0);
}

TEST(StreamKeyboard, NavigationWrapsAndPreservesAValidColumn)
{
    EXPECT_EQ(moonlight_keyboard_move(0, -1, 0), 12u);
    EXPECT_EQ(moonlight_keyboard_move(12, 1, 0), 0u);
    EXPECT_EQ(moonlight_keyboard_move(12, 0, 1), 25u);
    EXPECT_EQ(moonlight_keyboard_move(51, 0, 1), 4u);
}

TEST(StreamKeyboard, ShiftedLabelsMatchTheSentVirtualKeys)
{
    EXPECT_STREQ(moonlight_keyboard_label(0, false), "1");
    EXPECT_STREQ(moonlight_keyboard_label(0, true), "!");
    EXPECT_STREQ(moonlight_keyboard_label(12, false), "`");
    EXPECT_STREQ(moonlight_keyboard_label(12, true), "~");
    EXPECT_STREQ(moonlight_keyboard_label(13, false), "q");
    EXPECT_STREQ(moonlight_keyboard_label(13, true), "Q");
    EXPECT_EQ(moonlight_keyboard_keys[13].virtual_key, 0x51u);
    EXPECT_EQ(moonlight_keyboard_keys[47].action, moonlight_keyboard_action::shift);
}

TEST(StreamKeyboard, CoversEveryPrintableUsAsciiPasswordCharacter)
{
    for (int character = 0x20; character <= 0x7e; ++character)
    {
        bool found = character == ' ';

        for (uint32_t index = 0; !found && index < moonlight_keyboard_key_count; ++index)
        {
            const char *normal = moonlight_keyboard_keys[index].normal;
            const char *shifted = moonlight_keyboard_keys[index].shifted;

            found = (normal[0] == character && normal[1] == '\0') ||
                    (shifted[0] == character && shifted[1] == '\0');
        }
        EXPECT_TRUE(found) << "Missing printable ASCII character " << character;
    }
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

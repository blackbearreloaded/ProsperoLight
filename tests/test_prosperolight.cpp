/*
 * ps5-native-app-boilerplate / ProsperoLight - Stream shortcut tests.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lan_http_report.hpp"
#include "native_agc_output.hpp"
#include "moonlight_stream_input.hpp"
#include "moonlight_stream_keyboard.hpp"
#include "moonlight_config.hpp"
#include "moonlight_health.hpp"
#include "moonlight_physical_input.hpp"

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

TEST(VideoOutput, Uses1080pFor1080pSources)
{
    const auto output = native_agc_output_geometry(1920u, 1080u);

    EXPECT_EQ(output.width, 1920u);
    EXPECT_EQ(output.height, 1080u);
    EXPECT_EQ(output.framebuffer_bytes, 0x0a00000u);
}

TEST(VideoOutput, Uses4kFor1440pAnd2160pSources)
{
    const auto output_1440p = native_agc_output_geometry(2560u, 1440u);
    const auto output_2160p = native_agc_output_geometry(3840u, 2160u);

    EXPECT_EQ(output_1440p.width, 3840u);
    EXPECT_EQ(output_1440p.height, 2160u);
    EXPECT_EQ(output_1440p.framebuffer_bytes, 0x2000000u);
    EXPECT_EQ(output_2160p.width, 3840u);
    EXPECT_EQ(output_2160p.height, 2160u);
    EXPECT_EQ(output_2160p.framebuffer_bytes, 0x2000000u);
}

TEST(VideoOutput, UsesHardwareValidatedBilinearSampler)
{
    EXPECT_EQ(kNativeAgcBilinearSamplerWord, 0x09500000u);
    EXPECT_NE(kNativeAgcBilinearSamplerWord, 0x08000000u);
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

TEST(PhysicalInput, MapsUsbKeyboardLikeMoonlightQt)
{
    using prosperolight::physical_input::MapKey;

    EXPECT_EQ(MapKey(4).virtual_key, 0x41u);
    EXPECT_EQ(MapKey(29).virtual_key, 0x5au);
    EXPECT_EQ(MapKey(30).virtual_key, 0x31u);
    EXPECT_EQ(MapKey(39).virtual_key, 0x30u);
    EXPECT_EQ(MapKey(40).virtual_key, 0x0du);
    EXPECT_EQ(MapKey(80).virtual_key, 0x25u);
    EXPECT_EQ(MapKey(58).virtual_key, 0x70u);
    EXPECT_EQ(MapKey(69).virtual_key, 0x7bu);
    EXPECT_EQ(MapKey(100).virtual_key, 0xe2u);
    EXPECT_EQ(MapKey(100).flags, prosperolight::physical_input::kNonNormalized);
    EXPECT_FALSE(static_cast<bool>(MapKey(0)));
}

TEST(PhysicalInput, ConvertsBothSidesOfEveryModifier)
{
    using namespace prosperolight::physical_input;

    EXPECT_EQ(MoonlightModifiers(kLeftShift | kRightShift), kModifierShift);
    EXPECT_EQ(MoonlightModifiers(kRightControl | kLeftAlt | kRightMeta),
              kModifierControl | kModifierAlt | kModifierMeta);
    EXPECT_EQ(MapKey(ModifierUsage(kLeftControl)).virtual_key, 0xa2u);
    EXPECT_EQ(MapKey(ModifierUsage(kRightMeta)).virtual_key, 0x5cu);
}

TEST(PhysicalInput, ClampsNativeMouseRangesForMoonlightPackets)
{
    using namespace prosperolight::physical_input;

    EXPECT_EQ(ClampMotion(40000), INT16_MAX);
    EXPECT_EQ(ClampMotion(-40000), INT16_MIN);
    EXPECT_EQ(ClampMotion(12), 12);
    EXPECT_EQ(ClampScroll(200), INT8_MAX);
    EXPECT_EQ(ClampScroll(-200), -INT8_MAX);
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

TEST(HostHealth, DebouncesTransientFailuresAndRecovers)
{
    MoonlightHealthState health{};

    EXPECT_EQ(health.Record(false), MoonlightHealthState::recovery_delay_ms);
    EXPECT_TRUE(health.Reconnecting());
    EXPECT_EQ(health.Record(false), MoonlightHealthState::recovery_delay_ms);
    EXPECT_TRUE(health.Reconnecting());
    EXPECT_EQ(health.Record(false), MoonlightHealthState::steady_delay_ms);
    EXPECT_FALSE(health.Reconnecting());
    EXPECT_EQ(health.consecutive_failures, MoonlightHealthState::offline_failure_threshold);

    EXPECT_EQ(health.Record(true), MoonlightHealthState::steady_delay_ms);
    EXPECT_EQ(health.consecutive_failures, 0U);
    EXPECT_FALSE(health.Reconnecting());
}
} // namespace

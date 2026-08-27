/*
 * ps5-native-app-boilerplate - Host tests for reusable application behavior.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Validates bounded asset loading without requiring PS5 hardware.
 */

#include "demo_renderer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <span>

namespace
{
class AssetTextTest : public testing::Test
{
  protected:
    static constexpr const char *missing_path = "build/tests/missing-banner.txt";
    static constexpr const char *asset_path = "build/tests/banner.txt";

    void SetUp() override
    {
        (void)std::remove(missing_path);
        std::ofstream asset{asset_path, std::ios::binary};
        ASSERT_TRUE(asset.is_open());
        asset << "FROM APP0\r\n";
        ASSERT_TRUE(asset.good());
    }

    void TearDown() override
    {
        (void)std::remove(asset_path);
        (void)std::remove(missing_path);
    }
};

TEST_F(AssetTextTest, MissingAssetUsesFallback)
{
    std::array<char, 32> text{};
    ps5::demo::read_asset_text(missing_path, std::span{text}, "FALLBACK");
    EXPECT_STREQ(text.data(), "FALLBACK");
}

TEST_F(AssetTextTest, AssetTextTrimsLineEndings)
{
    std::array<char, 32> text{};
    ps5::demo::read_asset_text(asset_path, std::span{text}, "FALLBACK");
    EXPECT_STREQ(text.data(), "FROM APP0");
}

TEST_F(AssetTextTest, AssetTextRemainsNullTerminatedWhenTruncated)
{
    std::array<char, 5> bounded{};
    ps5::demo::read_asset_text(asset_path, std::span{bounded}, "FALLBACK");
    EXPECT_STREQ(bounded.data(), "FROM");
}
} // namespace

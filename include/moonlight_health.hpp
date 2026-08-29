/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

struct MoonlightHealthState
{
    static constexpr unsigned offline_failure_threshold = 3;
    static constexpr std::uint64_t recovery_delay_ms = 1000;
    static constexpr std::uint64_t steady_delay_ms = 5000;

    unsigned consecutive_failures = 0;

    std::uint64_t Record(bool success)
    {
        if (success)
        {
            consecutive_failures = 0;
            return steady_delay_ms;
        }
        if (consecutive_failures < offline_failure_threshold)
            ++consecutive_failures;
        return Reconnecting() ? recovery_delay_ms : steady_delay_ms;
    }

    bool Reconnecting() const
    {
        return consecutive_failures > 0 && consecutive_failures < offline_failure_threshold;
    }
};

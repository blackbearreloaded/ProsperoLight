/*
 * ps5-native-app-boilerplate - PS5 pacing policies.
 * ProsperoLight: PS5 source-clock playout timeline.
 * Copyright (C) 2026 BlackBearReloaded and contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Derived independently for PS5 from the source-clock/readiness-feedback
 * architecture described in Nonary Moonlight-Qt VRR18; no implementation
 * code from that project is copied.
 */
#pragma once
#include <algorithm>
#include <cstdint>

namespace moonlight
{
// RTP and system clocks do not share an epoch. Use source timestamp *deltas*
// to drive a local monotonic clock, with bounded phase tracking.
// Without this mapping, adding a constant 'reserve' to ready_us adds latency
// but cannot smooth early/late arrivals.
class VrrSourceTimeline
{
  public:
    void reset()
    {
        *this = VrrSourceTimeline{};
    }

    void observe(uint64_t source_us, uint64_t ready_us)
    {
        if (!source_us || !ready_us)
            return;
        if (!anchored_ || source_us <= last_source_ || source_us - last_source_ > 250000 ||
            (last_ready_ && ready_us + 2000 < last_ready_))
        {
            anchor_source_ = source_us;
            anchor_local_ = ready_us;
            anchored_ = true;
            late_run_ = 0;
        }
        else
        {
            const uint64_t elapsed = source_us - anchor_source_;
            const uint64_t expected = anchor_local_ + elapsed;
            // Clock drift and lasting decode delay must not cause an ever
            // increasing queue. An isolated late arrival is *not* a phase
            // shift; recurring evidence releases/advances the anchor slowly.
            if (ready_us + 600 < expected)
            {
                const uint64_t lead = expected - ready_us - 600;
                anchor_local_ -= std::min<uint64_t>(100, std::max<uint64_t>(1, lead / 8));
                late_run_ = 0;
            }
            else if (ready_us > expected + 3500)
            {
                late_run_ = std::min(late_run_ + 1u, 100u);
                if (late_run_ >= 8)
                    anchor_local_ += std::min<uint64_t>(100, (ready_us - expected - 3500) / 16 + 1);
            }
            else
            {
                late_run_ = 0;
            }
            // A large discontinuity indicates a source/host pause or failed
            // decode; rebase rather than spending seconds on old phase.
            const uint64_t mapped = anchor_local_ + elapsed;
            if (ready_us > mapped + 100000 || mapped > ready_us + 100000)
            {
                anchor_source_ = source_us;
                anchor_local_ = ready_us;
                late_run_ = 0;
            }
        }
        last_source_ = source_us;
        last_ready_ = ready_us;
    }

    uint64_t target(uint64_t ready_us, uint64_t reserve_us) const
    {
        if (!anchored_ || !last_source_ || !reserve_us)
            return ready_us;
        const uint64_t mapped = anchor_local_ + (last_source_ - anchor_source_);
        // Cannot add an unbounded buffer after a stall or a clock jump.
        const uint64_t capped =
            std::min<uint64_t>(mapped + reserve_us, ready_us + reserve_us + 2000);
        return std::max(ready_us, capped);
    }

    bool anchored() const
    {
        return anchored_;
    }
    uint64_t phase_us() const
    {
        return anchored_ ? anchor_local_ : 0;
    }

  private:
    uint64_t anchor_source_{}, anchor_local_{};
    uint64_t last_source_{}, last_ready_{};
    unsigned late_run_{};
    bool anchored_{};
};
} // namespace moonlight

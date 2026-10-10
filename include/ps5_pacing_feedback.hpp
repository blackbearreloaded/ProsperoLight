/*
 * ps5-native-app-boilerplate - PS5 pacing policies.
 * ProsperoLight PS5 pacing policies inspired by Nonary's feedback architecture.
 * Copyright (C) 2026 BlackBearReloaded and contributors
 * Independent adaptation; no Nonary source is copied.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace moonlight
{

// All publication/read operations are wait-free. In particular the receiver
// callback cannot block the presentation thread, take a lock or allocate.
// The callback's clock is LiGetMicroseconds(), NOT the PS5 CLOCK_MONOTONIC
// used for presenter deadlines. Conversion is performed when publishing.
class Ps5ReceiveDeadline
{
  public:
    void clear()
    {
        anchor_.store(0, std::memory_order_release);
    }

    // The target belongs to the RTP timestamp of a frame that reached the
    // presentation worker. Receive-side work must finish before this deadline.
    // Never publish an Unpaced deadline, since there is no playout slot.
    void publish(uint32_t rtp, uint64_t playout_monotonic_us, uint64_t monotonic_now_us,
                 uint64_t limelight_now_us, uint64_t decode_and_prepare_us, bool paced)
    {
        if (!paced || !playout_monotonic_us || !monotonic_now_us || !limelight_now_us ||
            playout_monotonic_us + 250000 < monotonic_now_us ||
            playout_monotonic_us > monotonic_now_us + 200000)
        {
            clear();
            return;
        }
        const int64_t distance =
            static_cast<int64_t>(playout_monotonic_us) - static_cast<int64_t>(monotonic_now_us);
        const uint64_t lead = std::min<uint64_t>(decode_and_prepare_us + 500, 30000);
        const int64_t converted =
            static_cast<int64_t>(limelight_now_us) + distance - static_cast<int64_t>(lead);
        if (converted <= 0)
        {
            clear();
            return;
        }
        const uint64_t packed = (uint64_t(uint32_t(converted)) << 32) | rtp;
        anchor_.store(packed, std::memory_order_release);
    }

    uint64_t lookup(uint32_t rtp, uint64_t limelight_now_us) const
    {
        const uint64_t packed = anchor_.load(std::memory_order_acquire);
        if (!packed || !limelight_now_us)
            return 0;
        const int32_t source_ticks = static_cast<int32_t>(rtp - uint32_t(packed));
        // Valid only for a successor within one second. Prevent stale epochs,
        // 32-bit RTP wrap confusion and outrageous future receive deadlines.
        if (source_ticks < 0 || source_ticks > 90000)
            return 0;
        const int64_t anchor_delta = int32_t(uint32_t(packed >> 32) - uint32_t(limelight_now_us));
        const int64_t offset = anchor_delta + int64_t(source_ticks) * 100 / 9;
        // A deadline that passed recently is still meaningful: it allows the
        // existing common-c reorder rule to expire earlier. Too old is not.
        if (offset < -15000 || offset > 200000)
            return 0;
        const int64_t due = int64_t(limelight_now_us) + offset;
        return due > 0 ? uint64_t(due) : 0;
    }

  private:
    std::atomic<uint64_t> anchor_{0};
};

// This is service time (dequeue -> GPU-prepared image), not network RTT or
// source cadence. Do not learn deliberate presentation waiting as decoder
// cost, and exclude exceptional hangs from the prediction window.
class Ps5ReadinessEstimator
{
  public:
    void observe(uint64_t dequeued_us, uint64_t prepared_us)
    {
        if (!dequeued_us || prepared_us < dequeued_us)
            return;
        const uint64_t service = prepared_us - dequeued_us;
        if (service > 100000)
            return; // A stream pause is not steady readiness.
        samples_[index_] = static_cast<uint32_t>(service);
        index_ = (index_ + 1u) % samples_.size();
        count_ = std::min(count_ + 1u, samples_.size());
    }
    uint64_t percentile_us() const
    {
        if (count_ == 0)
            return 3500; // Conservative startup estimate.
        auto samples = samples_;
        const std::size_t rank = (count_ - 1u) * 95u / 100u;
        std::nth_element(samples.begin(), samples.begin() + rank, samples.begin() + count_);
        return std::min<uint64_t>(30000, std::max<uint64_t>(500, samples[rank]));
    }
    unsigned samples() const
    {
        return static_cast<unsigned>(count_);
    }

  private:
    std::array<uint32_t, 64> samples_{};
    std::size_t index_{}, count_{};
};

// Nonary's central lesson: a source-vs-presentation spacing error should add
// reserve only when there is evidence that *late readiness* caused it. Host
// stutters, repeat frames, intentionally delayed present and normal timestamp
// quantization must not increase a shared buffer.
class Ps5SpacingFeedback
{
  public:
    void configure(unsigned profile)
    {
        *this = Ps5SpacingFeedback{};
        cap_ = profile == 0 ? 0u : profile == 1 ? 8000u : 16000u;
    }

    // A capture gap, lost confirmation or new timestamp epoch invalidates the
    // previous timing distribution. Lifetime counters remain available for logs.
    void reset_epoch()
    {
        continuity_ = false;
        consecutive_ = 0;
        reserve_us_ = 0;
        last_frame_ = 0;
        last_source_ = last_ready_ = last_observed_ = last_requested_ = 0;
        ++epoch_resets_;
    }

    // observed_us is CPU time when VideoOut completion was noticed, not HDMI
    // scanout. requested_us records when the *same unique* flip was requested.
    // Older tests and adapters may omit requested_us; those use the less
    // informative source/ready/completion evidence without inventing a value.
    void observe(uint32_t frame, uint64_t source_us, uint64_t ready_us, uint64_t observed_us,
                 bool eligible, uint64_t requested_us = 0)
    {
        // An intentionally disabled learner is not a broken timestamp epoch.
        if (!eligible || !cap_)
        {
            if (continuity_)
                reset_epoch();
            return;
        }
        if (!frame || !source_us || !ready_us || !observed_us || observed_us < ready_us ||
            (requested_us && (requested_us < ready_us || requested_us > observed_us)))
        {
            reset_epoch();
            return;
        }
        const bool consecutive = continuity_ && frame == last_frame_ + 1u &&
                                 source_us > last_source_ && ready_us >= last_ready_ &&
                                 observed_us >= last_observed_;
        if (continuity_ && !consecutive)
            reset_epoch();
        if (consecutive)
        {
            const uint64_t source_delta = source_us - last_source_;
            const uint64_t ready_delta = ready_us - last_ready_;
            const uint64_t observed_delta = observed_us - last_observed_;
            if (source_delta < 6000 || source_delta > 100000 || ready_delta >= 200000 ||
                observed_delta >= 200000)
            {
                reset_epoch();
            }
            else
            {
                ++observations_;
                const uint64_t extra_ready =
                    ready_delta > source_delta ? ready_delta - source_delta : 0;
                const uint64_t extra_output =
                    observed_delta > source_delta ? observed_delta - source_delta : 0;
                const bool request_evidence =
                    requested_us && last_requested_ && requested_us >= last_requested_;
                const uint64_t request_delta =
                    request_evidence ? requested_us - last_requested_ : 0;
                const uint64_t extra_request =
                    request_delta > source_delta ? request_delta - source_delta : 0;
                // Feedback is *not* learned from sampling jitter alone.
                // 750 us excludes tiny CPU scheduling noise; 3000 us is a
                // conservative maximum unexplained completion-poll interval.
                const bool attribution_ok =
                    !request_evidence ||
                    (extra_request >= 750 && observed_delta <= request_delta + 3000);
                const bool attributable =
                    extra_ready >= 1250 && extra_output >= 1250 && attribution_ok;
                if (attributable)
                {
                    ++misses_;
                    consecutive_ = std::min(consecutive_ + 1u, 100u);
                    if (consecutive_ >= 3u && cap_)
                        reserve_us_ = std::min<uint64_t>(cap_, reserve_us_ + 250);
                }
                else
                {
                    if (!attribution_ok && extra_ready >= 1250 && extra_output >= 1250)
                        ++ambiguous_;
                    consecutive_ = 0;
                    reserve_us_ -= std::min<uint64_t>(reserve_us_, 40);
                }
            }
        }
        last_frame_ = frame;
        last_source_ = source_us;
        last_ready_ = ready_us;
        last_observed_ = observed_us;
        last_requested_ = requested_us;
        continuity_ = true;
    }
    uint64_t reserve_us() const
    {
        return reserve_us_;
    }
    uint64_t misses() const
    {
        return misses_;
    }
    uint64_t observations() const
    {
        return observations_;
    }
    uint64_t ambiguous() const
    {
        return ambiguous_;
    }
    uint64_t epoch_resets() const
    {
        return epoch_resets_;
    }

  private:
    uint32_t last_frame_{};
    uint64_t last_source_{}, last_ready_{}, last_observed_{}, last_requested_{};
    uint64_t reserve_us_{}, cap_{8000}, misses_{}, observations_{};
    uint64_t ambiguous_{}, epoch_resets_{};
    unsigned consecutive_{};
    bool continuity_{};
};

} // namespace moonlight

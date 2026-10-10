/*
 * ps5-native-app-boilerplate / ProsperoLight - Bounded source-clock
 * presentation timing. Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <algorithm>
#include <cstdint>

namespace moonlight
{
// RTP is a relative 90 kHz source clock; never subtract it from local time.
// Validate forward movement and unwrap rollover before estimating cadence.
class SourceTimestamp
{
  public:
    uint64_t update(uint32_t rtp, uint64_t fallback_us)
    {
        if (!initialized_)
        {
            initialized_ = true;
            last_ = rtp;
            ticks_ = rtp;
            return rtp ? ticks_ * 1000000 / 90000 : fallback_us;
        }
        const uint32_t delta = rtp - last_;
        last_ = rtp;
        if (delta && delta < 900000)
        {
            ticks_ += delta;
            return ticks_ * 1000000 / 90000;
        }
        if (!delta)
            return fallback_us;
        ticks_ = rtp;
        return fallback_us;
    }

  private:
    uint64_t ticks_{};
    uint32_t last_{};
    bool initialized_{};
};

// A target is a submission deadline. GPU completion and physical display
// completion remain distinct measurements owned by the presentation backend.
class FramePacing
{
  public:
    struct Stats
    {
        uint64_t misses{}, resets{}, submissions{}, reserve_us{}, period_us{};
        uint64_t wait_total_us{}, late_max_us{}, spacing_error_max_us{};
    } stats;

    void reset(unsigned fps)
    {
        *this = FramePacing{};
        period_q16_ = (UINT64_C(1000000) << 16) / std::max(1u, fps);
        period_ = period_q16_ >> 16;
        nominal_period_ = period_;
        stats.period_us = period_;
    }

    // Resume from a sparse desktop without losing learned readiness reserve
    // or replacing the confirmed cadence with three noisy source intervals.
    void resume(uint64_t ready_us)
    {
        slot_ = ready_us > reserve_ ? ready_us - reserve_ : ready_us;
        cadence_source_ = 0;
        candidate_count_ = 0;
        fractional_ = fixed_fraction_ = 0;
        resume_pending_ = true;
        ++stats.resets;
    }

    uint64_t admission_limit_us(uint64_t successor_interval, uint64_t legal_wait) const
    {
        return std::max(stale_limit_us(),
                        std::min<uint64_t>(100000, successor_interval + reserve_ + legal_wait));
    }

    uint64_t target(int32_t frame, uint64_t source_us, uint64_t ready_us,
                    uint32_t fixed_refresh_x100 = 0, uint64_t flip_anchor_us = 0,
                    uint32_t display_ceiling_x100 = 0, uint64_t preparation_lead_us = 1000)
    {
        if (!initialized_)
        {
            initialized_ = true;
            last_frame_ = frame;
            last_source_ = source_us;
            cadence_source_ = source_us;
            cadence_frame_ = frame;
            slot_ = ready_us;
        }
        else if (frame != last_frame_)
        {
            bool rate_changed = false;
            const int64_t frames = int64_t(frame) - last_frame_;
            uint64_t advance = frames > 0 && frames < 240 ? uint64_t(frames) * period_ : 0;
            // Estimate over a source-frame window. Filtering individual deltas
            // rejects short jitter samples but accepts long ones, biasing the
            // period upward until a 60 FPS source is paced at about 40 FPS.
            const int64_t cadence_frames = int64_t(frame) - cadence_frame_;
            if (cadence_frames >= 32 && cadence_frames < 240 && source_us > cadence_source_ &&
                cadence_source_)
            {
                const uint64_t delta = source_us - cadence_source_;
                const uint64_t sample = delta / uint64_t(cadence_frames);
                cadence_source_ = source_us;
                cadence_frame_ = frame;
                if (sample >= 8000 && sample <= 40000)
                {
                    if (sample > period_ * 112 / 100 || sample < period_ * 88 / 100)
                    {
                        if (candidate_ && sample > candidate_ * 95 / 100 &&
                            sample < candidate_ * 105 / 100)
                            ++candidate_count_;
                        else
                        {
                            candidate_ = sample;
                            candidate_count_ = 1;
                        }
                        if (candidate_count_ >= 3)
                        {
                            period_q16_ = (delta << 16) / uint64_t(cadence_frames);
                            candidate_count_ = 0;
                            rate_changed = true;
                        }
                    }
                    else
                    {
                        period_q16_ =
                            (period_q16_ * 7 + (delta << 16) / uint64_t(cadence_frames)) / 8;
                        candidate_count_ = 0;
                    }
                    period_q16_ = std::clamp<uint64_t>(period_q16_, (UINT64_C(1000000) << 16) / 120,
                                                       (UINT64_C(1000000) << 16) / 30);
                    period_ = period_q16_ >> 16;
                }
            }
            else if (cadence_frames <= 0 || cadence_frames >= 240 || source_us < cadence_source_ ||
                     !cadence_source_)
            {
                cadence_source_ = source_us;
                cadence_frame_ = frame;
                candidate_count_ = 0;
            }
            if (advance)
            {
                const uint64_t scaled = uint64_t(frames) * period_q16_ + fractional_;
                advance = scaled >> 16;
                fractional_ = scaled & 65535;
            }
            if (!advance || advance > 1000000 || ready_us > slot_ + advance + 250000)
            {
                slot_ = ready_us;
                reserve_ = 1500;
                ++stats.resets;
            }
            else
            {
                slot_ += advance;
                if (rate_changed)
                {
                    const uint64_t desired = ready_us > reserve_ ? ready_us - reserve_ : ready_us;
                    if (slot_ < desired)
                        slot_ += std::min<uint64_t>(1000, desired - slot_);
                    else
                        slot_ -= std::min<uint64_t>(1000, slot_ - desired);
                }
            }
            last_frame_ = frame;
            last_source_ = source_us;
        }
        if (resume_pending_)
        {
            slot_ = ready_us > reserve_ ? ready_us - reserve_ : ready_us;
            resume_pending_ = false;
        }
        // Initial GPU/decoder setup must not become a permanent playout delay.
        if (stats.submissions < 3)
            slot_ = ready_us;
        const uint64_t cap = std::min<uint64_t>(10000, period_);
        const uint64_t demand = ready_us > slot_ ? ready_us - slot_ : 0;
        if (demand > reserve_)
        {
            reserve_ = std::min(cap, demand + 250);
            ++stats.misses;
            clean_ = 0;
        }
        else if (++clean_ >= 120 && reserve_ > 500)
        {
            reserve_ -= std::min<uint64_t>(reserve_ - 500, 25);
        }
        reserve_ = std::min(reserve_, cap);
        uint64_t deadline = std::min(std::max(ready_us, slot_ + reserve_), ready_us + cap);
        // A bounded interval guard prevents a late frame followed by a rush.
        // If fixed display output is slower, its own interval is the floor.
        uint64_t minimum = period_ * 98 / 100;
        bool display_matches = false;
        if (fixed_refresh_x100)
        {
            const uint64_t display_period = UINT64_C(100000000) / fixed_refresh_x100;
            const uint64_t divisor =
                std::max<uint64_t>(1, (nominal_period_ + display_period / 2) / display_period);
            const uint64_t matched = display_period * divisor;
            const uint64_t error =
                matched > nominal_period_ ? matched - nominal_period_ : nominal_period_ - matched;
            // Divisible rates keep their source interval guard. Non-divisible
            // rates need alternating display intervals (75/90 on 120 Hz).
            display_matches = error <= nominal_period_ / 100;
            minimum = (display_matches ? matched : display_period) * 98 / 100;
        }
        if (display_ceiling_x100)
            minimum = std::max(minimum, UINT64_C(100000000) / display_ceiling_x100);
        if (submitted_)
            deadline = std::max(deadline, submitted_ + minimum);
        if (fixed_refresh_x100 && flip_anchor_us && display_matches)
        {
            // VSync latches at the next vblank. Waiting until just before it
            // misses that vblank and the output stays at half rate.
            deadline = ready_us;
            if (submitted_)
                deadline = std::max(deadline, submitted_ + minimum);
        }
        else if (fixed_refresh_x100 && flip_anchor_us)
        {
            const uint64_t display_period = UINT64_C(100000000) / fixed_refresh_x100;
            const uint64_t lead = std::min<uint64_t>(preparation_lead_us, display_period / 4);
            // Fractional rates need 1/1/2 refresh slots (90 on 120), not
            // rounding each 11 ms source deadline to two display intervals.
            fixed_fraction_ += period_q16_;
            const uint64_t unit = display_period << 16;
            const uint64_t ticks = std::max<uint64_t>(1, fixed_fraction_ / unit);
            fixed_fraction_ %= unit;
            deadline = std::max(ready_us, flip_anchor_us + (ticks - 1) * display_period + lead);
        }
        stats.reserve_us = reserve_;
        stats.period_us = period_;
        return deadline;
    }

    void submitted(uint64_t planned_us, uint64_t actual_us, uint64_t wait_us)
    {
        if (submitted_)
        {
            const uint64_t spacing = actual_us > submitted_ ? actual_us - submitted_ : 0;
            const uint64_t error = spacing > period_ ? spacing - period_ : period_ - spacing;
            stats.spacing_error_max_us = std::max(stats.spacing_error_max_us, error);
        }
        if (actual_us > planned_us)
        {
            const uint64_t late = actual_us - planned_us;
            stats.late_max_us = std::max(stats.late_max_us, late);
            wake_lead_ = std::clamp<uint64_t>((wake_lead_ * 7 + late) / 8, 50, 250);
        }
        else
            wake_lead_ = std::max<uint64_t>(50, wake_lead_ - 1);
        submitted_ = actual_us;
        stats.wait_total_us += wait_us;
        ++stats.submissions;
    }

    uint64_t wake_lead_us() const
    {
        return wake_lead_;
    }

    uint64_t stale_limit_us() const
    {
        return std::max(period_ * 2, period_ + reserve_);
    }

  private:
    uint64_t period_q16_ = (UINT64_C(1000000) << 16) / 60, fractional_{};
    uint64_t period_ = 16666, nominal_period_ = 16666, slot_{}, submitted_{}, last_source_{},
             cadence_source_{};
    uint64_t reserve_ = 1500, candidate_{}, wake_lead_ = 100, fixed_fraction_{};
    unsigned candidate_count_{}, clean_{};
    int32_t last_frame_{}, cadence_frame_{};
    bool initialized_{}, resume_pending_{};
};

// Both backends classify the same public API result combinations. These
// answers describe the process state, not a measured HDMI refresh rate.
inline bool output_released(uint32_t unpeg)
{
    return unpeg == 0 || unpeg == 0x8029001cu;
}
inline bool variable_after_peg(bool released, uint32_t peg)
{
    return released && peg == 0x8029001cu;
}

// Fixed refresh tolerates two missing source periods before idle repetition.
// VRR repeats are timed from observed scanout completion, never GPU completion.
// Aim near a 60 Hz floor with submission lead, without filling every 120 Hz
// slot.
inline uint64_t idle_scanout_delay_us(unsigned fps, unsigned refresh_x100, bool vrr_active,
                                      bool repeating)
{
    if (vrr_active)
        return 16000u;
    const uint64_t period = UINT64_C(100000000) / (refresh_x100 ? refresh_x100 : 5994u);
    return repeating ? period / 2u
                     : std::max<uint64_t>(period, UINT64_C(2000000) / (fps ? fps : 60u));
}

// A queued real picture takes precedence over an idle repeat. Never enqueue
// a repeat behind a picture still waiting for physical scanout.
inline bool idle_repeat_ready(uint64_t requested, uint64_t shown, bool available)
{
    return available && requested && shown != UINT64_MAX && shown >= requested;
}

// PS5 adapter for timestamp-driven VRR playout. Inspired by Nonary's
// separation of source cadence, presentation floors and bounded scheduling:
// https://github.com/Nonary/moonlight-qt/tree/master/app/streaming/video/ffmpeg-renderers/pacer
// No Qt/renderer code is copied. Repeat compensation is PS5-specific.
class VrrRepeatPolicy
{
  public:
    struct Stats
    {
        uint64_t pictures{}, repeats{}, wait_total_us{}, late_max_us{}, gap_max_us{};
    } stats;
    void waited(uint64_t ready, uint64_t planned, uint64_t submitted)
    {
        stats.wait_total_us += submitted > ready ? submitted - ready : 0;
        if (submitted > planned)
            stats.late_max_us = std::max(stats.late_max_us, submitted - planned);
    }
    uint64_t period() const
    {
        return period_;
    }
    void reset(unsigned fps)
    {
        *this = VrrRepeatPolicy{};
        period_ = UINT64_C(1000000) / std::max(1u, fps);
        moving_period_ = nominal_period_ = period_;
        update_repeat_policy();
    }
    void observe_picture(uint64_t pts, uint32_t frame_number = 0)
    {
        if (!pts || !last_pts_ || pts <= last_pts_ || pts - last_pts_ >= 1000000)
        {
            samples_ = fast_samples_ = fast_sum_ = 0;
            candidate_sum_ = candidate_min_ = candidate_max_ = 0;
        }
        else
        {
            // Dropped client frames must not look like a slower host cadence.
            // Frame numbers count transmitted pictures, including ones skipped
            // locally.
            const uint32_t frames = frame_number && last_frame_number_
                                        ? uint32_t(frame_number - last_frame_number_)
                                        : 1;
            const uint64_t delta = (pts - last_pts_) / (frames && frames < 120 ? frames : 1);
            if (delta >= 6000 && delta <= nominal_period_ * 105 / 100)
            {
                ++fast_samples_;
                fast_sum_ += delta;
            }
            else
                fast_samples_ = fast_sum_ = 0;
            // Fast recovery is only for a genuinely sparse source, not integral
            // duplication during normal 30--59 FPS motion.
            if (period_ > nominal_period_ * 125 / 100 && fast_samples_ >= 3)
            {
                gap_ = false;
                period_ =
                    std::clamp<uint64_t>(stable_period(fast_sum_ / fast_samples_), 8333, 250000);
                moving_period_ = period_;
                update_repeat_policy();
                samples_ = candidate_sum_ = candidate_min_ = candidate_max_ = 0;
                fast_samples_ = fast_sum_ = 0;
            }
            // A mixed static/moving segment is not a new source cadence.
            // Allow capture-clock quantization (90 FPS may alternate 8/16 ms).
            // Confirmation requires 200 ms
            // of homogeneous sender time, not a wall-clock timeout or burst.
            const bool quantized = frame_number != 0;
            if (!samples_ ||
                (quantized ? delta * 21 < candidate_max_ * 10 || delta * 10 > candidate_min_ * 21
                           : delta * 4 < candidate_max_ * 3 || delta * 3 > candidate_min_ * 4))
            {
                samples_ = 0;
                candidate_sum_ = 0;
                candidate_min_ = candidate_max_ = delta;
            }
            candidate_min_ = std::min(candidate_min_, delta);
            candidate_max_ = std::max(candidate_max_, delta);
            candidate_sum_ += pts - last_pts_;
            samples_ += frames && frames < 120 ? frames : 1;
            if (samples_ >= 8 && candidate_sum_ >= 200000)
            {
                period_ =
                    std::clamp<uint64_t>(stable_period(candidate_sum_ / samples_), 8333, 250000);
                update_repeat_policy();
                if (!low_)
                    moving_period_ = period_;
                samples_ = candidate_sum_ = candidate_min_ = candidate_max_ = 0;
            }
        }
        last_pts_ = pts;
        last_frame_number_ = frame_number;
        update_repeat_policy();
    }
    void scanned(uint64_t count, uint64_t observed)
    {
        if (count && count != scanned_count_)
        {
            scanned_count_ = count;
            scanned_at_ = observed;
        }
    }
    bool grid_active() const
    {
        return low_ || gap_;
    }
    uint64_t picture_target(uint64_t ready) const
    {
        if (!grid_active())
            return std::max(ready, submitted_at_
                                       ? submitted_at_ +
                                             std::max<uint64_t>(8333, moving_period_ * 98 / 100)
                                       : ready);
        uint64_t target =
            std::max(next_, submitted_at_ ? submitted_at_ + interval_ * 95 / 100 : ready);
        // A prepared image just late for its slot uses it immediately. Rounding
        // to the following slot would create a double-length scanout gap.
        return std::max(target, ready);
    }
    void presented(uint64_t submitted)
    {
        ++stats.pictures;
        advance(submitted);
        if (!low_)
            gap_ = false;
    }
    void picture(uint64_t pts, uint64_t submitted)
    {
        observe_picture(pts);
        presented(submitted);
    }
    uint64_t deadline() const
    {
        return next_;
    }
    void repeated(uint64_t submitted)
    {
        ++stats.repeats;
        gap_ = true;
        advance(submitted);
    }
    uint64_t interval() const
    {
        return interval_;
    }
    bool compensating() const
    {
        return low_;
    }
    unsigned repeat_factor() const
    {
        return copies_;
    }
    unsigned target_refresh_x100() const
    {
        return nominal_period_ > kSingleScanoutLimitUs
                   ? unsigned(UINT64_C(100000000) / interval_)
                   : unsigned(UINT64_C(100000000) * copies_ / period_);
    }
    unsigned source_rate() const
    {
        return static_cast<unsigned>(
            std::clamp<uint64_t>((1000000 + period_ / 2) / period_, 1, 120));
    }

  private:
    // One decision for initialization, fitted cadence, and sparse recovery.
    // Target >=60 Hz, with 1% clock tolerance so nominal 60/120 FPS does
    // not oscillate between repetition factors due to timestamp rounding.
    static constexpr uint64_t kSingleScanoutLimitUs = 16833;
    uint64_t stable_period(uint64_t measured) const
    {
        // Capture timestamps jitter across the LFC boundary even during motion.
        // Negotiated FPS is an upper bound: short capture bursts cannot speed
        // up the scanout grid. Genuine slower/sparse rates still fit.
        const uint64_t difference =
            measured > nominal_period_ ? measured - nominal_period_ : nominal_period_ - measured;
        return measured < nominal_period_ || difference * 100 <= nominal_period_ * 5
                   ? nominal_period_
                   : measured;
    }
    void update_repeat_policy()
    {
        if (nominal_period_ > kSingleScanoutLimitUs)
        {
            // Low-FPS custom output already needs duplication. Keep its HDMI
            // grid across sparse desktop capture instead of switching 100/102
            // Hz to 64 Hz (OLED brightness flicker). Incoming cadence remains
            // observable; retained pictures fill empty slots without decoding.
            const uint64_t nominal_copies =
                (nominal_period_ + kSingleScanoutLimitUs - 1) / kSingleScanoutLimitUs;
            interval_ = std::clamp<uint64_t>(nominal_period_ / nominal_copies, 8333, 20000);
            copies_ = unsigned(std::max<uint64_t>(1, (period_ + interval_ / 2) / interval_));
            low_ = true;
            return;
        }
        copies_ = unsigned((period_ + kSingleScanoutLimitUs - 1) / kSingleScanoutLimitUs);
        low_ = copies_ > 1;
        interval_ = low_ ? std::clamp<uint64_t>(period_ / copies_, 8333, 20000) : 20000;
    }
    void advance(uint64_t submitted)
    {
        // Counter completion authorizes another flip. Its observation time is
        // an upper bound, NOT the time that scanout began. Keep deadlines on
        // the submission grid instead of restarting them when polling is late.
        if (submitted_at_ && submitted > submitted_at_)
            stats.gap_max_us = std::max(stats.gap_max_us, submitted - submitted_at_);
        submitted_at_ = submitted;
        if (grid_active() && next_)
        {
            if (next_ <= submitted)
                next_ += ((submitted - next_) / interval_ + 1) * interval_;
            // A late show must not be followed by a short recovery interval.
            if (next_ < submitted + interval_ * 95 / 100)
                next_ = submitted + interval_;
        }
        else
            next_ = submitted + interval_;
    }
    uint64_t period_{16666}, last_pts_{}, samples_{}, next_{}, interval_{20000};
    uint64_t scanned_count_{}, scanned_at_{}, submitted_at_{}, fast_samples_{}, fast_sum_{},
        moving_period_{16666}, nominal_period_{16666};
    uint64_t candidate_sum_{}, candidate_min_{}, candidate_max_{};
    uint32_t last_frame_number_{};
    unsigned copies_{1};
    bool low_{}, gap_{};
};

// A rejected VRR request is fixed-refresh pacing. The selected mode stays
// Paced+VRR; the active name does not.
inline const char *effective_pacing_name(unsigned mode, bool vrr_active)
{
    if (vrr_active && mode != 0u)
        return mode == 2u ? "Paced+VRR" : "Paced (variable output)";
    if (mode != 0u)
        return "Paced";
    return "Unpaced";
}
} // namespace moonlight

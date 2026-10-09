// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "app_storage.hpp"
#include "lan_http_report.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Bounded, worker-owned CPU observations of actual VideoOut counters.
// Counter jumps are preserved, not interpolated into invented scanout times.
class ScanoutTrace
{
  public:
    ScanoutTrace(const char *backend, unsigned mode) : backend_(backend), mode_(mode)
    {
        if (prosperolight_logs_enabled())
            samples_ = static_cast<Sample *>(calloc(capacity, sizeof(Sample)));
    }
    void observe(uint64_t count, uint64_t argument, uint64_t now, bool repeat)
    {
        if (!samples_ || (started_ && count == previous_))
            return;
        const uint64_t delta = started_ && count >= previous_ ? count - previous_ : 0;
        samples_[next_] = {now, count, argument, delta, repeat};
        previous_ = count;
        started_ = true;
        next_ = (next_ + 1) % capacity;
        if (count_ < capacity)
            ++count_;
        else
            ++omitted_;
    }
    ~ScanoutTrace()
    {
        if (samples_ && prosperolight_logs_enabled())
        {
            char path[192], temporary[200];
            snprintf(path, sizeof(path), "%s/%s-scanout-mode%u.csv", storage::paths().performance,
                     backend_, mode_);
            snprintf(temporary, sizeof(temporary), "%s.tmp", path);
            FILE *f = fopen(temporary, "w");
            if (f)
            {
                bool ok = fprintf(f,
                                  "# software_counter_observations,count=%zu,omitted=%zu\n"
                                  "observed_us,flip_count,shown_argument,count_delta,kind\n",
                                  count_, omitted_) > 0;
                for (size_t i = 0; ok && i < count_; ++i)
                {
                    const auto &s = samples_[(count_ == capacity ? next_ + i : i) % capacity];
                    ok = fprintf(f, "%llu,%llu,%llu,%llu,%s\n", (unsigned long long)s.time,
                                 (unsigned long long)s.count, (unsigned long long)s.argument,
                                 (unsigned long long)s.delta,
                                 s.delta > 1 ? "coalesced"
                                 : s.repeat  ? "repeat"
                                             : "picture") > 0;
                }
                if (fclose(f) != 0)
                    ok = false;
                if (!ok || rename(temporary, path) != 0)
                    remove(temporary);
            }
        }
        free(samples_);
    }
    ScanoutTrace(const ScanoutTrace &) = delete;
    ScanoutTrace &operator=(const ScanoutTrace &) = delete;

  private:
    struct Sample
    {
        uint64_t time, count, argument, delta;
        bool repeat;
    };
    static constexpr size_t capacity = 32768;
    const char *backend_;
    unsigned mode_;
    Sample *samples_{};
    size_t count_{}, omitted_{}, next_{};
    uint64_t previous_{};
    bool started_{};
};

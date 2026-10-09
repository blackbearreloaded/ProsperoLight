// ps5-native-app-boilerplate / ProsperoLight - Bounded presentation diagnostics.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "app_storage.hpp"
#include "lan_http_report.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Bounded event trace. PyroWave arrival and presentation producers share this
// ring; no disk writes occur on either hot path. Export after workers stop.
#include <pthread.h>
class PacingDecisionTrace
{
  public:
    PacingDecisionTrace(const char *backend, unsigned mode) : backend_(backend), mode_(mode)
    {
        pthread_mutex_init(&lock_, nullptr);
        if (prosperolight_logs_enabled())
            samples_ = static_cast<Sample *>(calloc(capacity, sizeof(Sample)));
    }
    void record(int frame, uint64_t source, uint64_t now, uint64_t target, const char *action)
    {
        if (!samples_)
            return;
        pthread_mutex_lock(&lock_);
        samples_[next_] = {frame, source, now, target, action};
        next_ = (next_ + 1) % capacity;
        if (count_ < capacity)
            ++count_;
        else
            ++omitted_;
        pthread_mutex_unlock(&lock_);
    }
    ~PacingDecisionTrace()
    {
        if (samples_ && prosperolight_logs_enabled())
        {
            char path[192], temporary[200];
            snprintf(path, sizeof(path), "%s/%s-decisions-mode%u.csv", storage::paths().performance,
                     backend_, mode_);
            snprintf(temporary, sizeof(temporary), "%s.tmp", path);
            FILE *f = fopen(temporary, "w");
            if (f)
            {
                bool ok = fprintf(f,
                                  "# count=%zu,omitted=%zu\n"
                                  "frame,source,observed_us,target_us,action\n",
                                  count_, omitted_) > 0;
                for (size_t i = 0; ok && i < count_; ++i)
                {
                    const auto &s = samples_[(count_ == capacity ? next_ + i : i) % capacity];
                    ok = fprintf(f, "%d,%llu,%llu,%llu,%s\n", s.frame, (unsigned long long)s.source,
                                 (unsigned long long)s.now, (unsigned long long)s.target,
                                 s.action) > 0;
                }
                if (fclose(f) != 0)
                    ok = false;
                if (!ok || rename(temporary, path) != 0)
                    remove(temporary);
            }
        }
        free(samples_);
        pthread_mutex_destroy(&lock_);
    }
    PacingDecisionTrace(const PacingDecisionTrace &) = delete;
    PacingDecisionTrace &operator=(const PacingDecisionTrace &) = delete;

  private:
    struct Sample
    {
        int frame;
        uint64_t source, now, target;
        const char *action;
    };
    static constexpr size_t capacity = 32768;
    const char *backend_;
    unsigned mode_;
    Sample *samples_{};
    size_t count_{}, omitted_{}, next_{};
    pthread_mutex_t lock_{};
};

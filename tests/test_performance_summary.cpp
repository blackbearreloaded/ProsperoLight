/*
 * ps5-native-app-boilerplate / ProsperoLight - Summary serialization and partial-write checks.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define PROSPEROLIGHT_LAN_TELEMETRY 0
#include "../src/moonlight_stream.cpp"
#include <cassert>
#include <string>

static std::string temporary_report, saved_report;
static bool write_failure, close_failure;
extern "C"
{
    int sceKernelOpen(const char *, int flags, uint16_t mode)
    {
        assert(flags == 0x601 && mode == 0600);
        temporary_report.clear();
        return 1;
    }
    int64_t sceKernelWrite(int, const void *data, size_t size)
    {
        if (write_failure)
            return -1;
        const size_t count = size > 7 ? 7 : size; // Deliberately short writes.
        temporary_report.append(static_cast<const char *>(data), count);
        return static_cast<int64_t>(count);
    }
    int sceKernelClose(int)
    {
        return close_failure ? -1 : 0;
    }
    int sceKernelRename(const char *, const char *)
    {
        saved_report = temporary_report;
        return 0;
    }
}
void native_agc_output_status(uint32_t *width, uint32_t *height, uint32_t *refresh)
{
    *width = 3840;
    *height = 2160;
    *refresh = 11988;
}

int main()
{
    native_renderer_state_t state{};
    state.mode = &video_modes[0];
    state.stream_fps = 120;
    state.client_refresh_x100 = 11988;
    state.reassembly_invalid_samples = 1;
    assert(moonlight::record_reassembly(state.reassembly_timing, 1000, 4000, 9000));
    state.access_units = 100;
    state.presented = 95;
    state.stale_presentation_drops = 5;
    for (unsigned i = 0; i < 100; ++i)
        state.decode_timing.add(3000);
    moonlight_stream_options_t options{};
    options.bitrate_kbps = 80000;
    moonlight::TimingHistogram input;
    input.add(4000);
    save_performance_summary(state, input, &options, 0);
    const std::string original = saved_report;
    assert(!original.empty());
    write_failure = true;
    save_performance_summary(state, input, &options, -1);
    assert(saved_report == original);
    write_failure = false;
    close_failure = true;
    save_performance_summary(state, input, &options, -2);
    assert(saved_report == original);
    puts(original.c_str()); // The runner parses and validates the actual JSON.
}

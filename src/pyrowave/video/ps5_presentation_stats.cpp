// SPDX-License-Identifier: MIT
#include "ps5_presentation_stats.hpp"
#include "../common.hpp"
#include "frame_pacing.hpp"
#include "native_agc_present.hpp"
#include "lan_http_report.hpp"
#include "app_storage.hpp"
#include "presentation_preferences.hpp"
#include <atomic>
#include <cstdio>
#ifdef __PROSPERO__
extern "C" int __real_sceVideoOutOpen(int32_t, int32_t, int32_t, const void *);
extern "C" int __real_sceVideoOutSubmitFlip(int32_t, int32_t, uint32_t, int64_t);
extern "C" int sceVideoOutGetFlipStatus(int32_t, void *);
extern "C" int sceVideoOutGetVblankStatus(int32_t, void *);
extern "C" int sceKernelUsleep(uint32_t);
extern "C" int sceVideoOutConfigureOutput(int32_t, uint32_t, const void *, const void *,
                                          const void *);
extern "C" int __real_sceVideoOutVrrUnpegFromFixedRate(int32_t);
static std::atomic<bool> vrr_output{false};
static void record_output_policy(int handle, const char *operation, int result)
{
    // Infrequent mode setup only: bypass buffered stdout so this receipt also
    // works on SDKs where reopening the process streams failed. Bounded file.
    if (!prosperolight_logs_enabled())
        return;
    const std::string path = std::string(storage::paths().logs) + "/videoout-policy.log";
    FILE *file = fopen(path.c_str(), "a");
    if (!file)
        return;
    fseek(file, 0, SEEK_END);
    if (ftell(file) > 65536)
    {
        fclose(file);
        file = fopen(path.c_str(), "w");
        if (!file)
            return;
    }
    fprintf(file, "handle=%08x mode=%u operation=%s rc=%08x vrr_api=%d\n", unsigned(handle),
            moonlight::presentation_mode(), operation, unsigned(result), vrr_output.load());
    fclose(file);
}
extern "C" int __real_sceVideoOutVrrPegToFixedRate(int32_t, uint64_t, uint64_t);
extern "C" int __wrap_sceVideoOutVrrPegToFixedRate(int32_t handle, uint64_t reserved1,
                                                   uint64_t reserved2)
{
    // Peg requires an Unpeg state transition. Even an unsuccessful Unpeg can
    // leave that state set in VideoOut, so finish the pair in all cases. Neither
    // return code alone proves a physically fixed HDMI refresh rate.
    const int prepared = __real_sceVideoOutVrrUnpegFromFixedRate(handle);
    record_output_policy(handle, "peg-prepare-unpeg", prepared);
    const int result = __real_sceVideoOutVrrPegToFixedRate(handle, reserved1, reserved2);
    if (result == 0)
        vrr_output.store(false);
    // A released output that this process may not peg (an elevated app counts as a
    // system process on system software 6.02) stays variable.
    else if (moonlight::variable_after_peg(
                 moonlight::output_released(static_cast<uint32_t>(prepared)),
                 static_cast<uint32_t>(result)))
        vrr_output.store(true);
    record_output_policy(handle, "peg", result);
    return result;
}
extern "C" int __wrap_sceVideoOutVrrUnpegFromFixedRate(int32_t handle)
{
    int result = __real_sceVideoOutVrrUnpegFromFixedRate(handle);
    record_output_policy(handle, "unpeg-initial", result);
    // VideoOut rejects Unpeg when its process-local flag is already set.
    // A failed request can also leave that flag set. Normalize through the
    // public Peg API, then retry once; never treat the initial refusal as
    // successful VRR or retry indefinitely when the kernel rejects the mode.
    if (static_cast<uint32_t>(result) == 0x8029001cu)
    {
        const int reset = __real_sceVideoOutVrrPegToFixedRate(handle, 0, 0);
        record_output_policy(handle, "unpeg-reset-peg", reset);
        if (reset == 0)
        {
            vrr_output.store(false);
            result = __real_sceVideoOutVrrUnpegFromFixedRate(handle);
            record_output_policy(handle, "unpeg-retry", result);
        }
        // The peg is refused too: the system released the output itself when it opened
        // (system software 6.02) and does not let this process peg it. It is released.
        else if (static_cast<uint32_t>(reset) == 0x8029001cu)
            result = 0;
    }
    vrr_output.store(result == 0);
    record_output_policy(handle, "unpeg", result);
    return result;
}
static std::atomic<int> video_handle{-1};
static std::atomic<bool> launcher_output{false};
static std::atomic<uint64_t> flips{0}, errors{0};
extern "C" int __wrap_sceVideoOutOpen(int32_t user, int32_t bus, int32_t index, const void *p)
{
    int handle = __real_sceVideoOutOpen(user, bus, index, p);
    if (handle >= 0)
    {
        if (launcher_output.load())
        {
            const int configured =
                sceVideoOutConfigureOutput(handle, 1u, nullptr, nullptr, nullptr);
            std::fprintf(stderr, "Launcher SDL fixed-output handle=%d rc=%08x\n", handle,
                         static_cast<unsigned>(configured));
            char message[128];
            std::snprintf(message, sizeof(message), "fixed_output handle=%d rc=%08x", handle,
                          static_cast<unsigned>(configured));
            prosperolight_log_append("prosperolight-menu-output.log", message);
        }
        vrr_output.store(false);
        video_handle.store(handle);
        // Low-refresh RADV sessions do not enter the high-refresh preset
        // branch. They still need an explicit fixed-output request.
        if (!launcher_output.load() && moonlight::presentation_mode() != 2)
            (void)__wrap_sceVideoOutVrrPegToFixedRate(handle, 0, 0);
        flips.store(0);
        errors.store(0);
    }
    return handle;
}
extern "C" int __wrap_sceVideoOutSubmitFlip(int32_t handle, int32_t buffer, uint32_t mode,
                                            int64_t argument)
{
    int result = __real_sceVideoOutSubmitFlip(handle, buffer, mode, argument);
    if (handle == video_handle.load())
    {
        if (result == 0)
            flips.fetch_add(1);
        else
            errors.fetch_add(1);
    }
    return result;
}
#endif
void ps5_launcher_output_policy(bool enabled)
{
#ifdef __PROSPERO__
    launcher_output.store(enabled);
#else
    (void)enabled;
#endif
}

PresentationStats ps5_presentation_stats()
{
    PresentationStats result;
#ifdef __PROSPERO__
    int handle = video_handle.load();
    uint64_t flip[16] = {}, vblank[16] = {};
    result.submitted = flips.load();
    result.failed = errors.load();
    if (handle >= 0 && sceVideoOutGetFlipStatus(handle, flip) == 0)
    {
        result.shown = flip[3];
        result.flip_count = flip[0];
        result.available = true;
    }
    // The public export has an opaque status ABI. Only word 0 is consumed,
    // and the caller verifies its count rate against CLOCK_MONOTONIC first.
    if (handle >= 0 && sceVideoOutGetVblankStatus(handle, vblank) == 0)
    {
        result.vblanks = vblank[0];
        result.vblank_available = true;
    }
#endif
    return result;
}
void ps5_drain_presents(uint64_t expected)
{
#ifdef __PROSPERO__
    for (unsigned i = 0; i < 10000; ++i)
    {
        auto s = ps5_presentation_stats();
        if (s.available && s.shown >= expected)
            return;
        sceKernelUsleep(1000);
    }
    fail("VideoOut did not show queued frames within 10s");
#endif
}

int ps5_hdr_output_active()
{
#ifdef __PROSPERO__
    return native_videoout_hdr_active(video_handle.load());
#else
    return -1;
#endif
}

bool ps5_vrr_output_active()
{
#ifdef __PROSPERO__
    return vrr_output.load();
#else
    return false;
#endif
}

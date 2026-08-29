/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Native game Moonlight/Sunshine Videodec2 zero-copy stream. */

#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdarg.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <opus_multistream.h>

#include "moonlight_stream.hpp"
#include "moonlight_config.hpp"
#include "moonlight_physical_input.hpp"
#include "moonlight_stream_input.hpp"
#include "moonlight_stream_keyboard.hpp"
#include "lan_http_report.hpp"
#include "native_agc_present.hpp"
#include "gamestream/certgen.h"
#include "gamestream/client.h"
#include "gamestream/gs_errors.h"
#include "gamestream/gs_http.h"
#include "gamestream/gs_log.h"
#include "../../third_party/moonlight-common-c/src/Limelight.h"

#define PIPELINE_BUFFER_COUNT 3u
#define SUBMISSION_QUEUE_CAPACITY 8u
#define INPUT_SLOT_BYTES 0x800000u
#define MOONLIGHT_IDENTITY_DIRECTORY "/download0/moonlight"
#define CONTROLLER_KEEPALIVE_US UINT64_C(1000000)
#define CONNECTION_SETUP_TIMEOUT_US UINT64_C(20000000)
#define FIRST_VIDEO_FRAME_TIMEOUT_US UINT64_C(10000000)
#define AUDIO_GRAIN_FRAMES 256u
#define AUDIO_CHANNELS 2u
#define AUDIO_RING_FRAMES (AUDIO_GRAIN_FRAMES * 6u)
#define AUDIO_DECODE_MAX_FRAMES 5760u
#define AUDIO_OUT_ALREADY_INIT UINT32_C(0x8026000e)
#define PS5_AUDIO_USER_SYSTEM 0xff
#define PS5_AUDIO_PORT_MAIN 0
#define PS5_AUDIO_FORMAT_S16_STEREO 1
#define VIDEO_SLICES_PER_FRAME 4
#define HUD_STATS_REFRESH_FRAMES 60u

#define PS5_PAD_BUTTON_L3 0x000002u
#define PS5_PAD_BUTTON_R3 0x000004u
#define PS5_PAD_BUTTON_OPTIONS 0x000008u
#define PS5_PAD_BUTTON_UP 0x000010u
#define PS5_PAD_BUTTON_RIGHT 0x000020u
#define PS5_PAD_BUTTON_DOWN 0x000040u
#define PS5_PAD_BUTTON_LEFT 0x000080u
#define PS5_PAD_BUTTON_L1 0x000400u
#define PS5_PAD_BUTTON_R1 0x000800u
#define PS5_PAD_BUTTON_TRIANGLE 0x001000u
#define PS5_PAD_BUTTON_CIRCLE 0x002000u
#define PS5_PAD_BUTTON_CROSS 0x004000u
#define PS5_PAD_BUTTON_SQUARE 0x008000u
#define PS5_PAD_BUTTON_TOUCH_PAD 0x100000u
#define PS5_PAD_BUTTON_INTERCEPTED UINT32_C(0x80000000)
#define PS5_PAD_SAMPLE_CAPACITY 64

extern "C"
{
    int sceKernelUsleep(uint32_t microseconds);
    int64_t sceKernelGetDirectMemorySize(void);
    int32_t sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t length,
                                          size_t alignment, int memory_type,
                                          int64_t *direct_memory_start);
    int32_t sceKernelMapDirectMemory(void **address, size_t length, int protection, int flags,
                                     int64_t direct_memory_start, size_t alignment);
    int32_t sceKernelAvailableFlexibleMemorySize(size_t *out_size);
    int32_t sceKernelMapNamedFlexibleMemory(void **address, size_t length, int protection,
                                            int flags, const char *name);
    int32_t sceKernelReleaseFlexibleMemory(void *address, size_t length);
    int32_t sceKernelVirtualQuery(void *address, int flags, void *info, size_t info_size);
    int32_t sceKernelMunmap(void *address, size_t length);
    int32_t sceKernelReleaseDirectMemory(int64_t direct_memory_start, size_t length);
    int32_t sceKernelSendNotificationRequest(uint32_t device, void *request, size_t size,
                                             int32_t blocking);
    int sceSystemServiceHideSplashScreen(void);
    int32_t sceSysmoduleLoadModule(uint32_t id);
    int32_t sceSysmoduleUnloadModule(uint32_t id);
    int32_t sceUserServiceInitialize(void *params);
    int32_t sceUserServiceGetInitialUser(int32_t *user_id);
    int32_t sceUserServiceTerminate(void);
    int32_t scePadInit(void);
    int32_t scePadOpen(int32_t user_id, int32_t port_type, int32_t index, const void *params);
    int32_t scePadClose(int32_t handle);
    int32_t scePadRead(int32_t handle, void *samples, int32_t capacity);
    int32_t sceKeyboardInit(void);
    int32_t sceKeyboardOpen(int32_t user_id, int32_t type, int32_t index, const void *params);
    int32_t sceKeyboardRead(int32_t handle, void *data, int32_t capacity);
    int32_t sceKeyboardClose(int32_t handle);
    int32_t sceMouseInit(void);
    int32_t sceMouseOpen(int32_t user_id, int32_t type, int32_t index, const void *params);
    int32_t sceMouseRead(int32_t handle, void *data, int32_t count);
    int32_t sceMouseClose(int32_t handle);
    int32_t sceAudioOutInit(void);
    int32_t sceAudioOutOpen(int32_t user_id, int32_t type, int32_t index, uint32_t length,
                            uint32_t frequency, uint32_t format);
    int32_t sceAudioOutOutput(int32_t handle, const void *buffer);
    int32_t sceAudioOutClose(int32_t handle);
}
typedef struct notification_request
{
    uint8_t reserved[45];
    char message[3075];
} notification_request_t;

typedef struct videodec2_decoder_config
{
    uint64_t size;
    uint32_t resource_type, codec_type, profile, max_level;
    int32_t max_width, max_height, max_dpb_frames;
    uint32_t pipeline_depth;
    uint64_t compute_queue, cpu_affinity;
    int32_t cpu_priority;
    uint32_t optimize_progressive, check_memory_type, reserved;
} videodec2_decoder_config_t;

typedef struct videodec2_decoder_memory
{
    uint64_t size, cpu_size;
    void *cpu;
    uint64_t gpu_size;
    void *gpu;
    uint64_t cpu_gpu_size;
    void *cpu_gpu;
    uint64_t max_frame_size;
    uint32_t frame_alignment, reserved;
} videodec2_decoder_memory_t;

typedef struct videodec2_compute_config
{
    uint64_t size;
    uint16_t pipe_id, queue_id;
    uint8_t check_memory_type, reserved0;
    uint16_t reserved1;
} videodec2_compute_config_t;

typedef struct videodec2_compute_memory
{
    uint64_t size, cpu_gpu_size;
    void *cpu_gpu;
} videodec2_compute_memory_t;

typedef struct videodec2_direct_memory
{
    uint64_t size;
    uint64_t allocation_size;
    void *address;
    int64_t direct_start;
} videodec2_direct_memory_t;

typedef struct videodec2_input
{
    uint64_t size;
    void *au;
    uint64_t au_size, pts, dts, attached;
} videodec2_input_t;

typedef struct videodec2_frame
{
    uint64_t size;
    void *buffer;
    uint64_t buffer_size;
    uint32_t accepted, reserved;
} videodec2_frame_t;

typedef struct videodec2_output
{
    uint64_t size;
    uint8_t valid, error, picture_count, padding;
    uint32_t codec, width, pitch, height, reserved;
    void *buffer;
    uint64_t buffer_size;
    uint32_t frame_format, pitch_bytes;
} videodec2_output_t;

typedef struct native_video_mode
{
    uint32_t codec_preference;
    uint32_t resolution_preference;
    int video_format;
    uint32_t codec_type;
    uint32_t profile;
    uint32_t max_level;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t output_width;
    uint32_t output_height;
    uint32_t alternate_output_height;
    uint32_t output_pitch;
    uint32_t visible_width;
    uint32_t visible_height;
    uint32_t hdr;
    const char *name;
} native_video_mode_t;

static const native_video_mode_t video_modes[] = {
    {MOONLIGHT_VIDEO_CODEC_H264, MOONLIGHT_STREAM_RESOLUTION_1080P, VIDEO_FORMAT_H264, 1, 100, 51,
     1920, 1088, 1920, 1088, 0, 2048, 1920, 1080, 0, "H.264 1080p60"},
    {MOONLIGHT_VIDEO_CODEC_H264, MOONLIGHT_STREAM_RESOLUTION_1440P, VIDEO_FORMAT_H264, 1, 100, 51,
     2560, 1440, 2560, 1440, 0, 2560, 2560, 1440, 0, "H.264 1440p60"},
    {MOONLIGHT_VIDEO_CODEC_H264, MOONLIGHT_STREAM_RESOLUTION_2160P, VIDEO_FORMAT_H264, 1, 100, 52,
     3840, 2176, 3840, 2160, 2176, 3840, 3840, 2160, 0, "H.264 2160p60 beta"},
    {MOONLIGHT_VIDEO_CODEC_HEVC, MOONLIGHT_STREAM_RESOLUTION_1080P, VIDEO_FORMAT_H265, 0x000ee049,
     1, 123, 1920, 1088, 1920, 1088, 0, 2048, 1920, 1080, 0, "HEVC Main 1080p60"},
    {MOONLIGHT_VIDEO_CODEC_HEVC, MOONLIGHT_STREAM_RESOLUTION_1440P, VIDEO_FORMAT_H265, 0x000ee049,
     1, 150, 2560, 1440, 2560, 1440, 0, 2560, 2560, 1440, 0, "HEVC Main 1440p60"},
    {MOONLIGHT_VIDEO_CODEC_HEVC, MOONLIGHT_STREAM_RESOLUTION_2160P, VIDEO_FORMAT_H265, 0x000ee049,
     1, 153, 3840, 2176, 3840, 2160, 2176, 3840, 3840, 2160, 0, "HEVC Main 2160p60 beta"},
    {MOONLIGHT_VIDEO_CODEC_HEVC, MOONLIGHT_STREAM_RESOLUTION_1080P, VIDEO_FORMAT_H265_MAIN10,
     0x000ee049, 2, 123, 1920, 1088, 1920, 1088, 0, 1920, 1920, 1080, 1, "HEVC Main10 HDR 1080p60"},
};

static_assert(sizeof(video_modes) / sizeof(video_modes[0]) == 7,
              "every supported SDR mode plus Main10 HDR needs one mode");

static const native_video_mode_t *find_video_mode(uint32_t codec, uint32_t resolution, uint32_t hdr)
{
    size_t index;

    for (index = 0; index < sizeof(video_modes) / sizeof(video_modes[0]); ++index)
    {
        if (video_modes[index].codec_preference == codec &&
            video_modes[index].resolution_preference == resolution &&
            video_modes[index].hdr == (hdr != 0))
            return &video_modes[index];
    }
    return NULL;
}

typedef struct ps5_pad_sample
{
    uint32_t buttons;
    uint8_t left_x, left_y, right_x, right_y;
    uint8_t left_trigger, right_trigger;
    uint8_t reserved_to_connected[66];
    int32_t connected;
    uint64_t timestamp_us;
    uint8_t extension[16];
    uint8_t connected_count;
    uint8_t remaining[15];
} ps5_pad_sample_t;

static_assert(sizeof(ps5_pad_sample_t) == 120,
              "normal Pad samples must use the verified 120-byte ABI");
static_assert(offsetof(ps5_pad_sample_t, connected) == 0x4c,
              "Pad connection state offset must stay verified");
static_assert(offsetof(ps5_pad_sample_t, timestamp_us) == 0x50,
              "Pad timestamp offset must stay verified");
static_assert(offsetof(ps5_pad_sample_t, connected_count) == 0x68,
              "Pad connection generation offset must stay verified");

typedef struct controller_event
{
    int buttons;
    uint8_t left_trigger, right_trigger;
    int16_t left_x, left_y, right_x, right_y;
} controller_event_t;

typedef struct ps5_controller_state
{
    int32_t user_service_result, user_result, pad_init_result;
    int32_t user_id, handle, arrival_result, removal_result;
    uint32_t polls, samples, empty_reads, max_batch, read_errors;
    uint32_t events, send_errors, nonneutral_samples;
    uint32_t disconnected_samples, intercepted_samples;
    uint32_t observed_raw_buttons, observed_moonlight_buttons;
    uint32_t last_raw_buttons;
    uint32_t last_mouse_buttons, mouse_buttons_down;
    uint32_t mouse_toggles, mouse_motion_events, mouse_button_events;
    uint32_t mouse_scroll_events, mouse_errors;
    controller_event_t last_event;
    controller_event_t mouse_event;
    uint64_t last_event_us;
    uint64_t next_mouse_motion_us;
    uint8_t connected_count;
    uint8_t connected_count_valid;
    int announced;
    int mouse_mode;
    int keyboard_mode;
    uint32_t keyboard_selected;
    int keyboard_shifted;
    std::atomic<int> requested_stop;
    ps5_pad_sample_t sample_batch[PS5_PAD_SAMPLE_CAPACITY];
} ps5_controller_state_t;

typedef struct ps5_keyboard_state
{
    uint64_t timestamp_us;
    uint8_t intercepted;
    uint8_t reserved0[7];
    uint8_t connected;
    uint8_t reserved1[3];
    int32_t length;
    uint32_t leds;
    uint32_t modifiers;
    uint16_t keys[16];
    uint8_t reserved2[32];
} ps5_keyboard_state_t;

static_assert(sizeof(ps5_keyboard_state_t) == 96, "Keyboard state must use the verified PS5 ABI");
static_assert(offsetof(ps5_keyboard_state_t, intercepted) == 0x08,
              "Keyboard intercepted offset must stay verified");
static_assert(offsetof(ps5_keyboard_state_t, connected) == 0x10,
              "Keyboard connected offset must stay verified");
static_assert(offsetof(ps5_keyboard_state_t, length) == 0x14,
              "Keyboard length offset must stay verified");
static_assert(offsetof(ps5_keyboard_state_t, modifiers) == 0x1c,
              "Keyboard modifier offset must stay verified");
static_assert(offsetof(ps5_keyboard_state_t, keys) == 0x20,
              "Keyboard key array offset must stay verified");

typedef struct ps5_mouse_data
{
    uint64_t timestamp_us;
    uint8_t connected;
    uint8_t padding0[3];
    uint32_t buttons;
    int32_t x_axis, y_axis, wheel, tilt;
    uint8_t reserved[8];
} ps5_mouse_data_t;

static_assert(sizeof(ps5_mouse_data_t) == 40, "Mouse data must use the verified PS5 ABI");
static_assert(offsetof(ps5_mouse_data_t, buttons) == 0x0c,
              "Mouse button offset must stay verified");
static_assert(offsetof(ps5_mouse_data_t, x_axis) == 0x10, "Mouse axis offset must stay verified");

typedef struct ps5_mouse_open_param
{
    uint8_t behavior_flag;
    uint8_t reserved[7];
} ps5_mouse_open_param_t;

typedef struct ps5_physical_input_state
{
    int32_t keyboard_module_result, keyboard_unload_result;
    int32_t keyboard_init_result, keyboard_open_result, keyboard_close_result;
    int32_t mouse_module_result, mouse_unload_result;
    int32_t mouse_init_result, mouse_open_result, mouse_close_result;
    int32_t keyboard_handles[12], mouse_handles[8];
    uint32_t keyboard_handle_count, mouse_handle_count;
    uint32_t keyboard_polls, keyboard_read_errors, keyboard_events, keyboard_send_errors;
    uint32_t mouse_polls, mouse_samples, mouse_read_errors, mouse_motion_events;
    uint32_t mouse_button_events, mouse_scroll_events, mouse_send_errors;
    uint32_t mouse_buttons[8];
    ps5_keyboard_state_t keyboards[12];
    ps5_keyboard_state_t keyboard_samples_batch[16];
    ps5_mouse_data_t mouse_samples_batch[64];
    int initialization_attempted;
} ps5_physical_input_state_t;

typedef struct ps5_audio_state
{
    OpusMSDecoder *decoder;
    int32_t init_result, open_result, drain_result, close_result;
    int32_t handle, opus_error;
    int channels, samples_per_frame;
    uint32_t ring_head, ring_tail, ring_count;
    uint32_t packets, plc_packets, decode_errors;
    uint32_t output_calls, output_errors, overruns;
    uint32_t packet_samples_min, packet_samples_max, packet_sample_mismatches;
    uint32_t peak_sample;
    RTP_AUDIO_STATS rtp;
    uint64_t decoded_frames, nonzero_samples, dropped_frames;
    uint64_t first_packet_us, last_packet_us;
    uint64_t interval_total_us, interval_min_us, interval_max_us;
    uint64_t decode_total_us, decode_max_us;
    uint64_t output_total_us, output_max_us;
    int16_t ring[AUDIO_RING_FRAMES * AUDIO_CHANNELS];
    int16_t output[AUDIO_GRAIN_FRAMES * AUDIO_CHANNELS];
    int16_t decoded[AUDIO_DECODE_MAX_FRAMES * AUDIO_CHANNELS];
} ps5_audio_state_t;

static ps5_audio_state_t make_audio_state()
{
    ps5_audio_state_t state{};

    state.open_result = -1;
    state.drain_result = -1;
    state.close_result = -1;
    state.handle = -1;
    return state;
}

static ps5_audio_state_t audio_state = make_audio_state();

extern "C"
{
    int32_t sceVideodec2QueryDecoderMemoryInfo(const videodec2_decoder_config_t *config,
                                               videodec2_decoder_memory_t *memory);
    int32_t sceVideodec2QueryComputeMemoryInfo(videodec2_compute_memory_t *memory);
    int32_t sceVideodec2AllocateComputeQueue(const videodec2_compute_config_t *config,
                                             const videodec2_compute_memory_t *memory,
                                             void **queue);
    int32_t sceVideodec2ReleaseComputeQueue(void *queue);
    int32_t sceVideodec2CreateDecoder(const videodec2_decoder_config_t *config,
                                      const videodec2_decoder_memory_t *memory, void **decoder);
    int32_t sceVideodec2DeleteDecoder(void *decoder);
    int32_t sceVideodec2MapDirectMemory(void *decoder, const videodec2_direct_memory_t *memory);
    int32_t sceVideodec2Reset(void *decoder);
    int32_t sceVideodec2Decode(void *decoder, videodec2_input_t *input, videodec2_frame_t *frame,
                               videodec2_output_t *output);
    int32_t sceVideodec2Flush(void *decoder, videodec2_frame_t *frame, videodec2_output_t *output);
}

static notification_request_t notification;
static std::atomic<int> connection_terminated;
static std::atomic<int> connection_error;
static std::atomic<uint32_t> host_hdr_active;
static std::atomic<uint32_t> host_hdr_transitions;

typedef struct native_renderer_state
{
    const native_video_mode_t *mode;
    void *decoder;
    void *input_memory;
    void *frame_memory;
    size_t input_size;
    size_t frame_size;
    size_t stream_bytes;
    uint32_t network_dropped_frames;
    int32_t last_network_frame;
    uint32_t host_latency_frames;
    uint32_t host_latency_min_tenths_ms;
    uint32_t host_latency_max_tenths_ms;
    uint64_t host_latency_total_tenths_ms;
    uint32_t rtt_ms;
    uint32_t rtt_variance_ms;
    int rtt_valid;
    uint32_t access_units;
    std::atomic<uint32_t> presented;
    uint32_t fragments;
    uint32_t decode_calls;
    uint64_t copy_total_us;
    uint64_t copy_max_us;
    uint64_t decode_total_us;
    uint64_t decode_max_us;
    uint32_t flush_calls;
    uint64_t flush_total_us;
    uint64_t flush_max_us;
    uint64_t present_total_us;
    uint64_t present_max_us;
    uint64_t callback_to_decode_total_us;
    uint64_t callback_to_decode_min_us;
    uint64_t callback_to_decode_max_us;
    uint64_t callback_to_flip_total_us;
    uint64_t callback_to_flip_min_us;
    uint64_t callback_to_flip_max_us;
    uint64_t submission_arrival_us[SUBMISSION_QUEUE_CAPACITY];
    uint64_t submission_pts_us[SUBMISSION_QUEUE_CAPACITY];
    int32_t submission_frame[SUBMISSION_QUEUE_CAPACITY];
    uint32_t submission_head;
    uint32_t submission_count;
    uint32_t latency_calls;
    uint64_t first_video_us;
    uint64_t last_video_us;
    uint64_t first_present_us;
    uint64_t last_present_us;
    int32_t last_result;
    uint32_t hdr_mismatch_reported;
    std::atomic<int> running;
} native_renderer_state_t;

static native_renderer_state_t *active_renderer;

static int ps5_controller_disconnect_only(ps5_controller_state_t *state)
{
    int count;

    if (!state || state->handle < 0)
        return 0;
    count = scePadRead(state->handle, state->sample_batch, PS5_PAD_SAMPLE_CAPACITY);
    if (count <= 0)
        return 0;
    for (int index = 0; index < count; ++index)
    {
        const ps5_pad_sample_t *sample = &state->sample_batch[index];

        if (sample->connected && !(sample->buttons & PS5_PAD_BUTTON_INTERCEPTED) &&
            moonlight_stream_disconnect_requested(sample->buttons))
            return 1;
    }
    return 0;
}

typedef struct connection_loading_state
{
    void *surface;
    size_t surface_bytes;
    ps5_controller_state_t *controller;
    int hdr;
    uint32_t output_source_width;
    uint32_t output_source_height;
    std::atomic<int> animation_enabled;
    std::atomic<int> animation_presenting;
    std::atomic<int> active;
    std::atomic<int> cancel_requested;
    std::atomic<int> timed_out;
    std::atomic<int> connection_pending;
    uint64_t started_us;
    pthread_t thread;
    int thread_started;
    int create_result;
    int present_result;
} connection_loading_state_t;

static std::atomic<connection_loading_state_t *> active_connection_loading;
static uint64_t monotonic_us(void);

static void *connection_loading_thread(void *context)
{
    auto *state = static_cast<connection_loading_state_t *>(context);
    uint32_t phase = 1;

    while (std::atomic_load_explicit(&state->active, std::memory_order_relaxed))
    {
        if (std::atomic_load_explicit(&state->animation_enabled, std::memory_order_acquire))
        {
            std::atomic_store_explicit(&state->animation_presenting, 1, std::memory_order_release);
            if (std::atomic_load_explicit(&state->animation_enabled, std::memory_order_acquire))
                state->present_result = native_agc_present_loading(
                    state->surface, state->surface_bytes, phase++, state->hdr,
                    state->output_source_width, state->output_source_height);
            if (state->present_result != 0)
            {
                std::atomic_store_explicit(&state->animation_enabled, 0, std::memory_order_release);
                (void)native_agc_present_shutdown();
            }
            std::atomic_store_explicit(&state->animation_presenting, 0, std::memory_order_release);
        }
        for (unsigned slice = 0; slice < 25; ++slice)
        {
            if (!std::atomic_load_explicit(&state->active, std::memory_order_relaxed))
                break;
            if (ps5_controller_disconnect_only(state->controller))
                state->controller->requested_stop = 1;
            if (state->controller && state->controller->requested_stop)
                std::atomic_store_explicit(&state->cancel_requested, 1, std::memory_order_relaxed);
            if (monotonic_us() - state->started_us >= CONNECTION_SETUP_TIMEOUT_US)
            {
                std::atomic_store_explicit(&state->timed_out, 1, std::memory_order_relaxed);
                std::atomic_store_explicit(&state->cancel_requested, 1, std::memory_order_relaxed);
            }
            if (std::atomic_load_explicit(&state->cancel_requested, std::memory_order_relaxed))
            {
                http_interrupt();
                if (std::atomic_load_explicit(&state->connection_pending,
                                              std::memory_order_acquire))
                {
                    LiInterruptConnection();
                    std::atomic_store_explicit(&state->active, 0, std::memory_order_relaxed);
                    break;
                }
            }
            sceKernelUsleep(10000);
        }
    }
    return NULL;
}

static int start_connection_loading(connection_loading_state_t *state, void *surface,
                                    size_t surface_bytes, int hdr, uint32_t output_source_width,
                                    uint32_t output_source_height,
                                    ps5_controller_state_t *controller)
{
    state->surface = surface;
    state->surface_bytes = surface_bytes;
    state->hdr = hdr;
    state->output_source_width = output_source_width;
    state->output_source_height = output_source_height;
    state->controller = controller;
    state->started_us = monotonic_us();
    state->present_result = 0;
    std::atomic_store_explicit(&state->animation_enabled, 0, std::memory_order_relaxed);
    std::atomic_store_explicit(&state->animation_presenting, 0, std::memory_order_relaxed);
    if (surface && surface_bytes)
    {
        state->present_result = native_agc_present_loading(
            surface, surface_bytes, 0, hdr, output_source_width, output_source_height);
        std::atomic_store_explicit(&state->animation_enabled, state->present_result == 0,
                                   std::memory_order_relaxed);
        if (state->present_result != 0)
            (void)native_agc_present_shutdown();
    }

    std::atomic_store_explicit(&state->active, 1, std::memory_order_relaxed);
    std::atomic_store_explicit(&state->cancel_requested, 0, std::memory_order_relaxed);
    std::atomic_store_explicit(&state->timed_out, 0, std::memory_order_relaxed);
    std::atomic_store_explicit(&state->connection_pending, 0, std::memory_order_relaxed);
    std::atomic_store_explicit(&active_connection_loading, state, std::memory_order_release);
    state->create_result = pthread_create(&state->thread, NULL, connection_loading_thread, state);
    if (state->create_result == 0)
        state->thread_started = 1;
    else
        std::atomic_store_explicit(&state->active, 0, std::memory_order_relaxed);
    return state->create_result;
}

static void stop_connection_animation(void)
{
    connection_loading_state_t *state =
        std::atomic_load_explicit(&active_connection_loading, std::memory_order_acquire);

    if (!state)
        return;
    std::atomic_store_explicit(&state->animation_enabled, 0, std::memory_order_release);
    while (std::atomic_load_explicit(&state->animation_presenting, std::memory_order_acquire))
        sceKernelUsleep(1000);
}

static void stop_connection_loading(void)
{
    connection_loading_state_t *state =
        std::atomic_exchange_explicit(&active_connection_loading, NULL, std::memory_order_acq_rel);

    if (!state)
        return;
    std::atomic_store_explicit(&state->active, 0, std::memory_order_relaxed);
    if (state->thread_started)
    {
        (void)pthread_join(state->thread, NULL);
        state->thread_started = 0;
    }
}

static size_t align_16k(size_t value)
{
    return (value + 0x3fff) & ~(size_t)0x3fff;
}

static uint64_t monotonic_us(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * UINT64_C(1000000) + (uint64_t)now.tv_nsec / UINT64_C(1000);
}

static int frame_is_in_pool(const void *frame, const void *pool, size_t stride)
{
    for (uint32_t index = 0; index < PIPELINE_BUFFER_COUNT; ++index)
        if (frame == (const uint8_t *)pool + index * stride)
            return 1;
    return 0;
}

static int32_t allocate_direct(size_t size, int protection, int64_t limit, int64_t *start,
                               void **address)
{
    int32_t result = sceKernelAllocateDirectMemory(0, limit, size, 0x4000, 12, start);
    if (result == 0)
        result = sceKernelMapDirectMemory(address, size, protection, 0, *start, 0x4000);
    return result;
}

static void release_direct(void *address, int64_t start, size_t size)
{
    if (address)
        (void)sceKernelMunmap(address, size);
    if (start >= 0)
        (void)sceKernelReleaseDirectMemory(start, size);
}

static int moonlight_renderer_setup(int video_format, int width, int height, int redraw_rate,
                                    void *context, int dr_flags)
{
    char receipt[256];

    auto *state = static_cast<native_renderer_state_t *>(context);

    if (!state || !state->mode || video_format != state->mode->video_format ||
        width != (int)state->mode->visible_width || height != (int)state->mode->visible_height ||
        redraw_rate != 60 || dr_flags != 0)
        return -1;

    active_renderer = state;
    active_renderer->last_result = 0;
    snprintf(receipt, sizeof(receipt),
             "Moonlight callbacks setup: format=%x display=%dx%d fps=%d flags=%x capabilities=%x",
             video_format, width, height, redraw_rate, dr_flags,
             CAPABILITY_SLICES_PER_FRAME(VIDEO_SLICES_PER_FRAME));
    (void)lan_http_report_text(receipt);
    return 0;
}

static void moonlight_renderer_start(void)
{
    stop_connection_animation();
    if (active_renderer)
        active_renderer->running = 1;
}

static void moonlight_renderer_stop(void)
{
    if (active_renderer)
        active_renderer->running = 0;
}

static void moonlight_renderer_cleanup(void)
{
    active_renderer = NULL;
}

static int moonlight_renderer_submit(PDECODE_UNIT decode_unit)
{
    native_renderer_state_t *state = active_renderer;
    videodec2_input_t input = {};
    videodec2_frame_t frame = {};
    videodec2_output_t output = {};
    PLENTRY entry;
    uint32_t slot;
    uint32_t fragment_count = 0;
    uint8_t *input_slot;
    void *frame_slot;
    size_t copied = 0;
    uint64_t started;
    uint64_t elapsed;
    uint64_t callback_arrival_us = monotonic_us();
    uint64_t decode_completed_us;
    uint64_t output_arrival_us = 0;
    uint64_t output_pts_us = 0;
    int32_t output_frame = 0;
    uint32_t hdr_active;
    uint32_t hdr_transitions;
    uint32_t submission_index;
    int32_t result;
    const char *decode_phase = "decode";
    char receipt[768];

    if (!state || !decode_unit)
        return DR_NEED_IDR;
    if (!state->running)
        return DR_OK;
    hdr_active = std::atomic_load_explicit(&host_hdr_active, std::memory_order_relaxed);
    hdr_transitions = std::atomic_load_explicit(&host_hdr_transitions, std::memory_order_relaxed);
    if (state->mode->hdr &&
        ((hdr_transitions && !hdr_active) || decode_unit->colorspace != COLORSPACE_REC_2020))
    {
        if (!state->hdr_mismatch_reported)
        {
            notification_request_t hdr_notification = {};

            state->hdr_mismatch_reported = 1;
            snprintf(receipt, sizeof(receipt),
                     "Moonlight HDR unavailable: frame=%d reported=%u confirmed=%u transitions=%u "
                     "colorspace=%u expected=%u",
                     decode_unit->frameNumber, decode_unit->hdrActive ? 1u : 0u, hdr_active,
                     hdr_transitions, decode_unit->colorspace, COLORSPACE_REC_2020);
            (void)lan_http_report_text(receipt);
            snprintf(hdr_notification.message, sizeof(hdr_notification.message),
                     hdr_active
                         ? "ProsperoLight HDR stopped: Sunshine sent an unexpected color space."
                         : "ProsperoLight HDR unavailable: enable HDR on Sunshine's captured "
                           "display and retry.");
            (void)sceKernelSendNotificationRequest(0, &hdr_notification, sizeof(hdr_notification),
                                                   0);
        }
        state->last_result = -20;
        state->running = 0;
        connection_error = -20;
        connection_terminated = 1;
        return DR_NEED_IDR;
    }
    if (decode_unit->fullLength <= 0 || (size_t)decode_unit->fullLength > state->input_size ||
        !decode_unit->bufferList)
    {
        state->last_result = -12;
        return DR_NEED_IDR;
    }

    if (!state->first_video_us)
        state->first_video_us = callback_arrival_us;
    else if (decode_unit->frameNumber > state->last_network_frame + 1)
        state->network_dropped_frames +=
            (uint32_t)(decode_unit->frameNumber - (state->last_network_frame + 1));
    state->last_network_frame = decode_unit->frameNumber;
    state->last_video_us = callback_arrival_us;
    if (decode_unit->frameHostProcessingLatency)
    {
        uint32_t latency = decode_unit->frameHostProcessingLatency;

        if (!state->host_latency_frames || latency < state->host_latency_min_tenths_ms)
            state->host_latency_min_tenths_ms = latency;
        if (latency > state->host_latency_max_tenths_ms)
            state->host_latency_max_tenths_ms = latency;
        state->host_latency_total_tenths_ms += latency;
        ++state->host_latency_frames;
    }

    slot = state->access_units % PIPELINE_BUFFER_COUNT;
    input_slot = (uint8_t *)state->input_memory + slot * state->input_size;
    frame_slot = (uint8_t *)state->frame_memory + slot * state->frame_size;
    started = monotonic_us();
    for (entry = decode_unit->bufferList; entry; entry = entry->next)
    {
        if (!entry->data || entry->length <= 0 ||
            (size_t)entry->length > state->input_size - copied)
        {
            state->last_result = -13;
            return DR_NEED_IDR;
        }
        memcpy(input_slot + copied, entry->data, (size_t)entry->length);
        copied += (size_t)entry->length;
        ++fragment_count;
    }
    elapsed = monotonic_us() - started;
    state->copy_total_us += elapsed;
    if (elapsed > state->copy_max_us)
        state->copy_max_us = elapsed;
    if (copied != (size_t)decode_unit->fullLength)
    {
        state->last_result = -14;
        return DR_NEED_IDR;
    }

    input.size = sizeof(input);
    input.au = input_slot;
    input.au_size = copied;
    input.pts = decode_unit->presentationTimeUs;
    input.dts = UINT64_MAX;
    frame.size = sizeof(frame);
    frame.buffer = frame_slot;
    frame.buffer_size = state->frame_size;
    output.size = sizeof(output);
    started = monotonic_us();
    result = sceVideodec2Decode(state->decoder, &input, &frame, &output);
    decode_completed_us = monotonic_us();
    elapsed = decode_completed_us - started;
    state->decode_total_us += elapsed;
    if (elapsed > state->decode_max_us)
        state->decode_max_us = elapsed;
    ++state->decode_calls;
    ++state->access_units;
    state->fragments += fragment_count;
    state->stream_bytes += copied;

    if (result != 0 || output.error)
    {
        snprintf(receipt, sizeof(receipt),
                 "Moonlight submit error: phase=%s rc=%08x frame=%d au=%u slot=%u fragments=%u "
                 "full=%d copied=%zx accepted=%u valid=%u error=%u pictures=%u output=%ux%u "
                 "pitch=%u ptr=%p current=%p in_pool=%u",
                 decode_phase, (uint32_t)result, decode_unit->frameNumber, state->access_units,
                 slot, fragment_count, decode_unit->fullLength, copied, frame.accepted,
                 output.valid, output.error, output.picture_count, output.width, output.height,
                 output.pitch, output.buffer, frame_slot,
                 frame_is_in_pool(output.buffer, state->frame_memory, state->frame_size));
        (void)lan_http_report_text(receipt);
        state->last_result = result != 0 ? result : -15;
        return DR_NEED_IDR;
    }

    if (state->submission_count == SUBMISSION_QUEUE_CAPACITY)
    {
        state->last_result = -16;
        return DR_NEED_IDR;
    }
    submission_index =
        (state->submission_head + state->submission_count) % SUBMISSION_QUEUE_CAPACITY;
    state->submission_arrival_us[submission_index] = callback_arrival_us;
    state->submission_pts_us[submission_index] = decode_unit->presentationTimeUs;
    state->submission_frame[submission_index] = decode_unit->frameNumber;
    ++state->submission_count;

    if (!output.valid)
    {
        memset(&output, 0, sizeof(output));
        output.size = sizeof(output);
        started = monotonic_us();
        result = sceVideodec2Flush(state->decoder, &frame, &output);
        decode_completed_us = monotonic_us();
        elapsed = decode_completed_us - started;
        state->flush_total_us += elapsed;
        if (elapsed > state->flush_max_us)
            state->flush_max_us = elapsed;
        ++state->flush_calls;
        decode_phase = "flush";
    }

    if (result != 0 || !output.valid || output.error ||
        !frame_is_in_pool(output.buffer, state->frame_memory, state->frame_size) ||
        !frame.accepted || output.codec != state->mode->codec_type ||
        output.width != state->mode->output_width ||
        (output.height != state->mode->output_height &&
         (!state->mode->alternate_output_height ||
          output.height != state->mode->alternate_output_height)) ||
        output.pitch != state->mode->output_pitch || output.picture_count != 1)
    {
        snprintf(receipt, sizeof(receipt),
                 "Moonlight submit error: phase=%s rc=%08x frame=%d au=%u slot=%u fragments=%u "
                 "full=%d copied=%zx accepted=%u valid=%u error=%u pictures=%u output=%ux%u "
                 "pitch=%u ptr=%p current=%p in_pool=%u",
                 decode_phase, (uint32_t)result, decode_unit->frameNumber, state->access_units,
                 slot, fragment_count, decode_unit->fullLength, copied, frame.accepted,
                 output.valid, output.error, output.picture_count, output.width, output.height,
                 output.pitch, output.buffer, frame_slot,
                 frame_is_in_pool(output.buffer, state->frame_memory, state->frame_size));
        (void)lan_http_report_text(receipt);
        state->last_result = result != 0 ? result : -17;
        return DR_NEED_IDR;
    }
    if (state->mode->hdr && (output.pitch_bytes != state->mode->output_pitch * sizeof(uint16_t) ||
                             output.buffer_size == 0 || output.buffer_size > state->frame_size))
    {
        snprintf(receipt, sizeof(receipt),
                 "Moonlight Main10 layout rejected: frame=%d pitch=%u pitch_bytes=%u buffer=%llx "
                 "slot=%zx",
                 decode_unit->frameNumber, output.pitch, output.pitch_bytes,
                 (unsigned long long)output.buffer_size, state->frame_size);
        (void)lan_http_report_text(receipt);
        state->last_result = -19;
        return DR_NEED_IDR;
    }

    {
        native_agc_metrics_t hud_metrics = {};
        const native_agc_metrics_t *hud = NULL;

        output_arrival_us = state->submission_arrival_us[state->submission_head];
        output_pts_us = state->submission_pts_us[state->submission_head];
        output_frame = state->submission_frame[state->submission_head];
        state->submission_head = (state->submission_head + 1u) % SUBMISSION_QUEUE_CAPACITY;
        --state->submission_count;

        elapsed = decode_completed_us - output_arrival_us;
        state->callback_to_decode_total_us += elapsed;
        if (state->latency_calls == 0 || elapsed < state->callback_to_decode_min_us)
            state->callback_to_decode_min_us = elapsed;
        if (elapsed > state->callback_to_decode_max_us)
            state->callback_to_decode_max_us = elapsed;

        if (native_agc_hud_enabled())
        {
            uint64_t video_span = state->last_video_us > state->first_video_us
                                      ? state->last_video_us - state->first_video_us
                                      : 0;
            uint64_t render_span = state->last_present_us > state->first_present_us
                                       ? state->last_present_us - state->first_present_us
                                       : 0;
            uint64_t total_frames = (uint64_t)state->access_units + state->network_dropped_frames;

            if (state->access_units == 1u || state->access_units % HUD_STATS_REFRESH_FRAMES == 0u)
                state->rtt_valid = LiGetEstimatedRttInfo(&state->rtt_ms, &state->rtt_variance_ms);
            hud_metrics.video_codec = state->mode->codec_preference;
            hud_metrics.total_fps_x100 =
                video_span && total_frames > 1u
                    ? (uint32_t)((total_frames - 1u) * UINT64_C(100000000) / video_span)
                    : 0;
            hud_metrics.incoming_fps_x100 = video_span && state->access_units > 1u
                                                ? (uint32_t)((uint64_t)(state->access_units - 1u) *
                                                             UINT64_C(100000000) / video_span)
                                                : 0;
            hud_metrics.rendering_fps_x100 = render_span && state->presented > 1u
                                                 ? (uint32_t)((uint64_t)(state->presented - 1u) *
                                                              UINT64_C(100000000) / render_span)
                                                 : 0;
            hud_metrics.network_drop_percent_x100 =
                total_frames ? (uint32_t)((uint64_t)state->network_dropped_frames *
                                          UINT64_C(10000) / total_frames)
                             : 0;
            hud_metrics.rtt_ms = state->rtt_ms;
            hud_metrics.rtt_variance_ms = state->rtt_variance_ms;
            hud_metrics.rtt_valid = state->rtt_valid;
            hud_metrics.host_min_tenths_ms = state->host_latency_min_tenths_ms;
            hud_metrics.host_max_tenths_ms = state->host_latency_max_tenths_ms;
            hud_metrics.host_average_tenths_ms =
                state->host_latency_frames
                    ? (uint32_t)(state->host_latency_total_tenths_ms / state->host_latency_frames)
                    : 0;
            hud_metrics.decode_average_us =
                state->decode_calls ? state->decode_total_us / state->decode_calls : 0;
            hud = &hud_metrics;
        }
        started = monotonic_us();
        result =
            state->mode->hdr
                ? native_agc_present_main10(output.buffer, (size_t)output.buffer_size, output.pitch,
                                            output.height, state->mode->visible_width,
                                            state->mode->visible_height, hud)
                : native_agc_present_nv12(output.buffer, (size_t)output.buffer_size, output.pitch,
                                          output.height, state->mode->visible_width,
                                          state->mode->visible_height, hud);
        elapsed = monotonic_us() - started;
        state->present_total_us += elapsed;
        if (elapsed > state->present_max_us)
            state->present_max_us = elapsed;
        if (result != 0)
        {
            state->last_result = result;
            return DR_NEED_IDR;
        }
        state->last_present_us = monotonic_us();
        if (state->presented == 0)
            state->first_present_us = state->last_present_us;
        elapsed = state->last_present_us - output_arrival_us;
        state->callback_to_flip_total_us += elapsed;
        if (state->latency_calls == 0 || elapsed < state->callback_to_flip_min_us)
            state->callback_to_flip_min_us = elapsed;
        if (elapsed > state->callback_to_flip_max_us)
            state->callback_to_flip_max_us = elapsed;
        ++state->latency_calls;
        ++state->presented;
        if (state->presented == 1 || state->presented % 300u == 0u)
        {
            snprintf(receipt, sizeof(receipt),
                     "Moonlight callback progress: submit_frame=%d output_frame=%d "
                     "output_pts_us=%llu au=%u presented=%u pending=%u callback_to_flip_us=%llu "
                     "fragments=%u bytes=%zx decoder=%p pool=%p agc=%p in_pool=%u",
                     decode_unit->frameNumber, output_frame, (unsigned long long)output_pts_us,
                     state->access_units,
                     std::atomic_load_explicit(&state->presented, std::memory_order_relaxed),
                     state->submission_count, (unsigned long long)elapsed, state->fragments,
                     state->stream_bytes, output.buffer, state->frame_memory, output.buffer,
                     frame_is_in_pool(output.buffer, state->frame_memory, state->frame_size));
            (void)lan_http_report_text(receipt);
        }
    }

    state->last_result = 0;
    return DR_OK;
}

static DECODER_RENDERER_CALLBACKS moonlight_video_callbacks = {
    .setup = moonlight_renderer_setup,
    .start = moonlight_renderer_start,
    .stop = moonlight_renderer_stop,
    .cleanup = moonlight_renderer_cleanup,
    .submitDecodeUnit = moonlight_renderer_submit,
    .capabilities = CAPABILITY_SLICES_PER_FRAME(VIDEO_SLICES_PER_FRAME),
};

static void audio_ring_drop(ps5_audio_state_t *state, uint32_t frames)
{
    state->ring_head = (state->ring_head + frames) % AUDIO_RING_FRAMES;
    state->ring_count -= frames;
    state->dropped_frames += frames;
}

static void audio_ring_push(ps5_audio_state_t *state, const int16_t *pcm, uint32_t frames)
{
    uint32_t i;

    if (frames > AUDIO_RING_FRAMES)
    {
        uint32_t skip = frames - AUDIO_RING_FRAMES;

        pcm += skip * AUDIO_CHANNELS;
        state->dropped_frames += skip + state->ring_count;
        state->ring_head = 0;
        state->ring_tail = 0;
        state->ring_count = 0;
        frames = AUDIO_RING_FRAMES;
        ++state->overruns;
    }
    else if (state->ring_count + frames > AUDIO_RING_FRAMES)
    {
        audio_ring_drop(state, state->ring_count + frames - AUDIO_RING_FRAMES);
        ++state->overruns;
    }

    for (i = 0; i < frames; ++i)
    {
        memcpy(&state->ring[state->ring_tail * AUDIO_CHANNELS], &pcm[i * AUDIO_CHANNELS],
               AUDIO_CHANNELS * sizeof(int16_t));
        state->ring_tail = (state->ring_tail + 1u) % AUDIO_RING_FRAMES;
    }
    state->ring_count += frames;
}

static void audio_ring_pop(ps5_audio_state_t *state, int16_t *pcm, uint32_t frames)
{
    uint32_t i;

    for (i = 0; i < frames; ++i)
    {
        memcpy(&pcm[i * AUDIO_CHANNELS], &state->ring[state->ring_head * AUDIO_CHANNELS],
               AUDIO_CHANNELS * sizeof(int16_t));
        state->ring_head = (state->ring_head + 1u) % AUDIO_RING_FRAMES;
    }
    state->ring_count -= frames;
}

static int ps5_audio_init(int audio_configuration, const POPUS_MULTISTREAM_CONFIGURATION opus,
                          void *context, int flags)
{
    char receipt[512];

    (void)audio_configuration;
    (void)context;
    (void)flags;
    memset(&audio_state, 0, sizeof(audio_state));
    audio_state.handle = -1;
    audio_state.open_result = -1;
    audio_state.drain_result = -1;
    audio_state.close_result = -1;
    audio_state.opus_error = OPUS_BAD_ARG;
    audio_state.channels = opus->channelCount;
    audio_state.samples_per_frame = opus->samplesPerFrame;

    if (opus->sampleRate != 48000 || opus->channelCount != AUDIO_CHANNELS ||
        opus->samplesPerFrame <= 0 || opus->samplesPerFrame > (int)AUDIO_DECODE_MAX_FRAMES)
    {
        snprintf(receipt, sizeof(receipt),
                 "Moonlight audio rejected: rate=%d channels=%d frame_samples=%d", opus->sampleRate,
                 opus->channelCount, opus->samplesPerFrame);
        (void)lan_http_report_text(receipt);
        return -1;
    }

    audio_state.decoder = opus_multistream_decoder_create(
        opus->sampleRate, opus->channelCount, opus->streams, opus->coupledStreams,
        (const unsigned char *)opus->mapping, &audio_state.opus_error);
    if (!audio_state.decoder)
        goto failed;

    audio_state.init_result = sceAudioOutInit();
    if (audio_state.init_result != 0 && (uint32_t)audio_state.init_result != AUDIO_OUT_ALREADY_INIT)
        goto failed;

    audio_state.handle = sceAudioOutOpen(PS5_AUDIO_USER_SYSTEM, PS5_AUDIO_PORT_MAIN, 0,
                                         AUDIO_GRAIN_FRAMES, 48000, PS5_AUDIO_FORMAT_S16_STEREO);
    audio_state.open_result = audio_state.handle;
    if (audio_state.handle <= 0)
        goto failed;

    snprintf(receipt, sizeof(receipt),
             "Moonlight audio ready: rate=%d channels=%d streams=%d coupled=%d frame_samples=%d "
             "opus=%d init=%08x handle=%08x grain=%u",
             opus->sampleRate, opus->channelCount, opus->streams, opus->coupledStreams,
             opus->samplesPerFrame, audio_state.opus_error, (uint32_t)audio_state.init_result,
             (uint32_t)audio_state.handle, AUDIO_GRAIN_FRAMES);
    (void)lan_http_report_text(receipt);
    return 0;

failed:
    snprintf(receipt, sizeof(receipt),
             "Moonlight audio init failed: rate=%d channels=%d frame_samples=%d opus=%d init=%08x "
             "open=%08x",
             opus->sampleRate, opus->channelCount, opus->samplesPerFrame, audio_state.opus_error,
             (uint32_t)audio_state.init_result, (uint32_t)audio_state.open_result);
    (void)lan_http_report_text(receipt);
    if (audio_state.decoder)
    {
        opus_multistream_decoder_destroy(audio_state.decoder);
        audio_state.decoder = NULL;
    }
    audio_state.handle = -1;
    return -1;
}

static void ps5_audio_cleanup(void)
{
    const RTP_AUDIO_STATS *rtp = LiGetRTPAudioStats();

    if (rtp)
        audio_state.rtp = *rtp;
    if (audio_state.handle > 0)
    {
        audio_state.drain_result = sceAudioOutOutput(audio_state.handle, NULL);
        audio_state.close_result = sceAudioOutClose(audio_state.handle);
        audio_state.handle = -1;
    }
    if (audio_state.decoder)
    {
        opus_multistream_decoder_destroy(audio_state.decoder);
        audio_state.decoder = NULL;
    }
}

static void ps5_audio_sample(char *sample_data, int sample_length)
{
    uint64_t now, started, elapsed;
    uint32_t pcm_samples;
    int decode_capacity;
    int decoded;

    if (!audio_state.decoder || audio_state.handle <= 0)
        return;

    now = monotonic_us();
    if (audio_state.packets == 0)
    {
        audio_state.first_packet_us = now;
    }
    else
    {
        elapsed = now - audio_state.last_packet_us;
        audio_state.interval_total_us += elapsed;
        if (audio_state.packets == 1 || elapsed < audio_state.interval_min_us)
            audio_state.interval_min_us = elapsed;
        if (elapsed > audio_state.interval_max_us)
            audio_state.interval_max_us = elapsed;
    }
    audio_state.last_packet_us = now;
    ++audio_state.packets;
    if (!sample_data)
        ++audio_state.plc_packets;
    else
    {
        int packet_samples =
            opus_packet_get_nb_samples((const unsigned char *)sample_data, sample_length, 48000);

        if (packet_samples > 0)
        {
            if (audio_state.packet_samples_min == 0 ||
                packet_samples < (int)audio_state.packet_samples_min)
                audio_state.packet_samples_min = (uint32_t)packet_samples;
            if (packet_samples > (int)audio_state.packet_samples_max)
                audio_state.packet_samples_max = (uint32_t)packet_samples;
            if (packet_samples != audio_state.samples_per_frame)
                ++audio_state.packet_sample_mismatches;
        }
    }
    decode_capacity = sample_data ? AUDIO_DECODE_MAX_FRAMES : audio_state.samples_per_frame;
    started = monotonic_us();
    decoded = opus_multistream_decode(
        audio_state.decoder, sample_data ? (const unsigned char *)sample_data : NULL,
        sample_data ? sample_length : 0, audio_state.decoded, decode_capacity, 0);
    elapsed = monotonic_us() - started;
    audio_state.decode_total_us += elapsed;
    if (elapsed > audio_state.decode_max_us)
        audio_state.decode_max_us = elapsed;
    if (decoded <= 0)
    {
        ++audio_state.decode_errors;
        return;
    }

    pcm_samples = (uint32_t)decoded * AUDIO_CHANNELS;
    for (uint32_t i = 0; i < pcm_samples; ++i)
    {
        int32_t magnitude = audio_state.decoded[i];

        if (magnitude < 0)
            magnitude = -magnitude;
        if (magnitude != 0)
            ++audio_state.nonzero_samples;
        if ((uint32_t)magnitude > audio_state.peak_sample)
            audio_state.peak_sample = (uint32_t)magnitude;
    }
    audio_state.decoded_frames += (uint32_t)decoded;
    audio_ring_push(&audio_state, audio_state.decoded, (uint32_t)decoded);
    while (audio_state.ring_count >= AUDIO_GRAIN_FRAMES)
    {
        int result;

        audio_ring_pop(&audio_state, audio_state.output, AUDIO_GRAIN_FRAMES);
        started = monotonic_us();
        result = sceAudioOutOutput(audio_state.handle, audio_state.output);
        elapsed = monotonic_us() - started;
        audio_state.output_total_us += elapsed;
        if (elapsed > audio_state.output_max_us)
            audio_state.output_max_us = elapsed;
        ++audio_state.output_calls;
        if (result < 0)
        {
            ++audio_state.output_errors;
            break;
        }
    }
}

static AUDIO_RENDERER_CALLBACKS moonlight_audio_callbacks = {
    .init = ps5_audio_init,
    .start = nullptr,
    .stop = nullptr,
    .cleanup = ps5_audio_cleanup,
    .decodeAndPlaySample = ps5_audio_sample,
    .capabilities = CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION,
};

static int16_t controller_axis(uint8_t value, int inverted)
{
    int32_t axis = ((int32_t)value - 128) * 256;

    if (inverted)
        axis = -axis;
    if (axis > INT16_MAX)
        axis = INT16_MAX;
    if (axis < INT16_MIN)
        axis = INT16_MIN;
    return (int16_t)axis;
}

static int ps5_controller_init(ps5_controller_state_t *state)
{
    int32_t user_id = -1;

    memset(state, 0, sizeof(*state));
    state->user_id = -1;
    state->handle = -1;
    state->arrival_result = -1;
    state->removal_result = -1;
    state->user_service_result = sceUserServiceInitialize(NULL);
    state->user_result = sceUserServiceGetInitialUser(&user_id);
    state->user_id = user_id;
    if (state->user_result < 0)
        return state->user_result;
    state->pad_init_result = scePadInit();
    if (state->pad_init_result < 0)
        return state->pad_init_result;
    state->handle = scePadOpen(user_id, 0, 0, NULL);
    return state->handle < 0 ? state->handle : 0;
}

static int ps5_keyboard_has_key(const ps5_keyboard_state_t *state, uint16_t key)
{
    for (size_t index = 0; index < sizeof(state->keys) / sizeof(state->keys[0]); ++index)
    {
        if (state->keys[index] == key)
            return 1;
    }
    return 0;
}

static void ps5_physical_send_key(ps5_physical_input_state_t *state, uint16_t usage, int down,
                                  uint32_t modifiers)
{
    const auto mapping = prosperolight::physical_input::MapKey(usage);

    if (!mapping)
        return;
    const int result = LiSendKeyboardEvent2(
        (short)(UINT16_C(0x8000) | mapping.virtual_key), down ? KEY_ACTION_DOWN : KEY_ACTION_UP,
        (char)prosperolight::physical_input::MoonlightModifiers(modifiers), (char)mapping.flags);
    if (result != 0)
        ++state->keyboard_send_errors;
    else
        ++state->keyboard_events;
}

static void ps5_physical_process_keyboard(ps5_physical_input_state_t *state,
                                          ps5_keyboard_state_t *previous,
                                          const ps5_keyboard_state_t *sample)
{
    ps5_keyboard_state_t neutral = {};
    const ps5_keyboard_state_t *current =
        sample->connected && !sample->intercepted ? sample : &neutral;
    const uint32_t changed_modifiers = previous->modifiers ^ current->modifiers;

    for (uint32_t bit = 1; bit <= prosperolight::physical_input::kRightMeta; bit <<= 1)
    {
        if (changed_modifiers & bit)
            ps5_physical_send_key(state, prosperolight::physical_input::ModifierUsage(bit),
                                  (current->modifiers & bit) != 0, current->modifiers);
    }
    for (size_t index = 0; index < sizeof(previous->keys) / sizeof(previous->keys[0]); ++index)
    {
        const uint16_t key = previous->keys[index];

        if (key != 0 && (key < 224 || key > 231) && !ps5_keyboard_has_key(current, key))
            ps5_physical_send_key(state, key, 0, current->modifiers);
    }
    for (size_t index = 0; index < sizeof(current->keys) / sizeof(current->keys[0]); ++index)
    {
        const uint16_t key = current->keys[index];

        if (key != 0 && (key < 224 || key > 231) && !ps5_keyboard_has_key(previous, key))
            ps5_physical_send_key(state, key, 1, current->modifiers);
    }
    *previous = *current;
}

static void ps5_physical_release_mouse_buttons(ps5_physical_input_state_t *state,
                                               uint32_t *pressed_buttons)
{
    static const int moonlight_buttons[] = {BUTTON_LEFT, BUTTON_RIGHT, BUTTON_MIDDLE, BUTTON_X1,
                                            BUTTON_X2};

    for (uint32_t bit = 1, index = 0; index < 5; bit <<= 1, ++index)
    {
        if ((*pressed_buttons & bit) == 0)
            continue;
        if (LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, moonlight_buttons[index]) != 0)
            ++state->mouse_send_errors;
        else
            ++state->mouse_button_events;
    }
    *pressed_buttons = 0;
}

static void ps5_physical_process_mouse(ps5_physical_input_state_t *state,
                                       uint32_t *previous_buttons, const ps5_mouse_data_t *sample)
{
    static const int moonlight_buttons[] = {BUTTON_LEFT, BUTTON_RIGHT, BUTTON_MIDDLE, BUTTON_X1,
                                            BUTTON_X2};
    const int usable = sample->connected && (sample->buttons & UINT32_C(0x80000000)) == 0;
    const uint32_t current_buttons = usable ? sample->buttons & UINT32_C(0x1f) : 0;

    if (!usable)
    {
        ps5_physical_release_mouse_buttons(state, previous_buttons);
        return;
    }
    if (sample->x_axis != 0 || sample->y_axis != 0)
    {
        if (LiSendMouseMoveEvent(prosperolight::physical_input::ClampMotion(sample->x_axis),
                                 prosperolight::physical_input::ClampMotion(sample->y_axis)) != 0)
            ++state->mouse_send_errors;
        else
            ++state->mouse_motion_events;
    }
    const uint32_t changed_buttons = *previous_buttons ^ current_buttons;
    for (uint32_t bit = 1, index = 0; index < 5; bit <<= 1, ++index)
    {
        if ((changed_buttons & bit) == 0)
            continue;
        if (LiSendMouseButtonEvent((current_buttons & bit) ? BUTTON_ACTION_PRESS
                                                           : BUTTON_ACTION_RELEASE,
                                   moonlight_buttons[index]) != 0)
            ++state->mouse_send_errors;
        else
            ++state->mouse_button_events;
    }
    *previous_buttons = current_buttons;
    if (sample->wheel != 0)
    {
        if (LiSendScrollEvent(prosperolight::physical_input::ClampScroll(sample->wheel)) != 0)
            ++state->mouse_send_errors;
        else
            ++state->mouse_scroll_events;
    }
    if (sample->tilt != 0)
    {
        if (LiSendHScrollEvent(prosperolight::physical_input::ClampScroll(sample->tilt)) != 0)
            ++state->mouse_send_errors;
        else
            ++state->mouse_scroll_events;
    }
}

static int ps5_physical_input_init(ps5_physical_input_state_t *state, int32_t user_id)
{
    ps5_mouse_open_param_t mouse_parameter = {};
    uint64_t keyboard_parameter = 0;

    memset(state, 0, sizeof(*state));
    state->initialization_attempted = 1;
    state->keyboard_module_result = -1;
    state->keyboard_unload_result = -1;
    state->keyboard_init_result = -1;
    state->keyboard_open_result = -1;
    state->keyboard_close_result = -1;
    state->mouse_module_result = -1;
    state->mouse_unload_result = -1;
    state->mouse_init_result = -1;
    state->mouse_open_result = -1;
    state->mouse_close_result = -1;
    for (size_t index = 0;
         index < sizeof(state->keyboard_handles) / sizeof(state->keyboard_handles[0]); ++index)
        state->keyboard_handles[index] = -1;
    for (size_t index = 0; index < sizeof(state->mouse_handles) / sizeof(state->mouse_handles[0]);
         ++index)
        state->mouse_handles[index] = -1;

    state->keyboard_module_result = sceSysmoduleLoadModule(UINT32_C(0x0106));
    if (state->keyboard_module_result >= 0)
        state->keyboard_init_result = sceKeyboardInit();
    if (state->keyboard_init_result >= 0)
    {
        for (size_t index = 0;
             index < sizeof(state->keyboard_handles) / sizeof(state->keyboard_handles[0]); ++index)
        {
            const int32_t handle = sceKeyboardOpen(user_id, 0, (int32_t)index, &keyboard_parameter);
            if (handle < 0)
                continue;
            state->keyboard_handles[index] = handle;
            if (state->keyboard_open_result < 0)
                state->keyboard_open_result = handle;
            ++state->keyboard_handle_count;
        }
    }

    state->mouse_module_result = sceSysmoduleLoadModule(UINT32_C(0x00a9));
    if (state->mouse_module_result >= 0)
        state->mouse_init_result = sceMouseInit();
    if (state->mouse_init_result >= 0)
    {
        for (size_t index = 0;
             index < sizeof(state->mouse_handles) / sizeof(state->mouse_handles[0]); ++index)
        {
            const int32_t handle = sceMouseOpen(user_id, 0, (int32_t)index, &mouse_parameter);
            if (handle < 0)
                continue;
            state->mouse_handles[index] = handle;
            if (state->mouse_open_result < 0)
                state->mouse_open_result = handle;
            ++state->mouse_handle_count;
        }
    }
    return state->keyboard_handle_count || state->mouse_handle_count ? 0 : -1;
}

static void ps5_physical_input_poll(ps5_physical_input_state_t *state)
{
    for (size_t slot = 0;
         slot < sizeof(state->keyboard_handles) / sizeof(state->keyboard_handles[0]); ++slot)
    {
        if (state->keyboard_handles[slot] < 0)
            continue;
        int count = sceKeyboardRead(state->keyboard_handles[slot], state->keyboard_samples_batch,
                                    (int32_t)(sizeof(state->keyboard_samples_batch) /
                                              sizeof(state->keyboard_samples_batch[0])));

        ++state->keyboard_polls;
        if (count < 0)
            ++state->keyboard_read_errors;
        if (count <= 0)
            continue;
        if ((size_t)count >
            sizeof(state->keyboard_samples_batch) / sizeof(state->keyboard_samples_batch[0]))
            count = (int)(sizeof(state->keyboard_samples_batch) /
                          sizeof(state->keyboard_samples_batch[0]));
        for (int index = 0; index < count; ++index)
            ps5_physical_process_keyboard(state, &state->keyboards[slot],
                                          &state->keyboard_samples_batch[index]);
    }
    for (size_t slot = 0; slot < sizeof(state->mouse_handles) / sizeof(state->mouse_handles[0]);
         ++slot)
    {
        if (state->mouse_handles[slot] < 0)
            continue;
        int count = sceMouseRead(
            state->mouse_handles[slot], state->mouse_samples_batch,
            (int32_t)(sizeof(state->mouse_samples_batch) / sizeof(state->mouse_samples_batch[0])));

        ++state->mouse_polls;
        if (count < 0)
            ++state->mouse_read_errors;
        else
        {
            if ((size_t)count >
                sizeof(state->mouse_samples_batch) / sizeof(state->mouse_samples_batch[0]))
                count = (int)(sizeof(state->mouse_samples_batch) /
                              sizeof(state->mouse_samples_batch[0]));
            state->mouse_samples += (uint32_t)count;
            for (int index = 0; index < count; ++index)
                ps5_physical_process_mouse(state, &state->mouse_buttons[slot],
                                           &state->mouse_samples_batch[index]);
        }
    }
}

static void ps5_physical_input_stop(ps5_physical_input_state_t *state)
{
    const ps5_keyboard_state_t empty_keyboard = {};

    for (size_t index = 0; index < sizeof(state->keyboards) / sizeof(state->keyboards[0]); ++index)
        ps5_physical_process_keyboard(state, &state->keyboards[index], &empty_keyboard);
    for (size_t index = 0; index < sizeof(state->mouse_buttons) / sizeof(state->mouse_buttons[0]);
         ++index)
        ps5_physical_release_mouse_buttons(state, &state->mouse_buttons[index]);
}

static void ps5_physical_input_shutdown(ps5_physical_input_state_t *state)
{
    if (!state->initialization_attempted)
        return;
    for (size_t index = 0;
         index < sizeof(state->keyboard_handles) / sizeof(state->keyboard_handles[0]); ++index)
    {
        if (state->keyboard_handles[index] < 0)
            continue;
        state->keyboard_close_result = sceKeyboardClose(state->keyboard_handles[index]);
        state->keyboard_handles[index] = -1;
    }
    for (size_t index = 0; index < sizeof(state->mouse_handles) / sizeof(state->mouse_handles[0]);
         ++index)
    {
        if (state->mouse_handles[index] < 0)
            continue;
        state->mouse_close_result = sceMouseClose(state->mouse_handles[index]);
        state->mouse_handles[index] = -1;
    }
    if (state->mouse_module_result == 0)
        state->mouse_unload_result = sceSysmoduleUnloadModule(UINT32_C(0x00a9));
    if (state->keyboard_module_result == 0)
        state->keyboard_unload_result = sceSysmoduleUnloadModule(UINT32_C(0x0106));
    state->initialization_attempted = 0;
}

static ps5_pad_sample_t *ps5_controller_newest_sample(ps5_controller_state_t *state, int count)
{
    ps5_pad_sample_t *newest = &state->sample_batch[0];

    for (int index = 1; index < count; ++index)
    {
        if (state->sample_batch[index].timestamp_us > newest->timestamp_us)
            newest = &state->sample_batch[index];
    }
    return newest;
}

static controller_event_t ps5_controller_map_sample(const ps5_pad_sample_t *sample, int neutral)
{
    controller_event_t event = {};

    if (neutral)
        return event;
    if (sample->buttons & PS5_PAD_BUTTON_UP)
        event.buttons |= UP_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_DOWN)
        event.buttons |= DOWN_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_LEFT)
        event.buttons |= LEFT_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_RIGHT)
        event.buttons |= RIGHT_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_CROSS)
        event.buttons |= A_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_CIRCLE)
        event.buttons |= B_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_SQUARE)
        event.buttons |= X_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_TRIANGLE)
        event.buttons |= Y_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_L1)
        event.buttons |= LB_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_R1)
        event.buttons |= RB_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_L3)
        event.buttons |= LS_CLK_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_R3)
        event.buttons |= RS_CLK_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_OPTIONS)
        event.buttons |= PLAY_FLAG;
    if (sample->buttons & PS5_PAD_BUTTON_TOUCH_PAD)
        event.buttons |= TOUCHPAD_FLAG;
    event.left_trigger = sample->left_trigger;
    event.right_trigger = sample->right_trigger;
    event.left_x = controller_axis(sample->left_x, 0);
    event.left_y = controller_axis(sample->left_y, 1);
    event.right_x = controller_axis(sample->right_x, 0);
    event.right_y = controller_axis(sample->right_y, 1);
    return event;
}

static void ps5_controller_send(ps5_controller_state_t *state, const controller_event_t *event)
{
    static const uint32_t supported_buttons =
        UP_FLAG | DOWN_FLAG | LEFT_FLAG | RIGHT_FLAG | A_FLAG | B_FLAG | X_FLAG | Y_FLAG | LB_FLAG |
        RB_FLAG | PLAY_FLAG | LS_CLK_FLAG | RS_CLK_FLAG | TOUCHPAD_FLAG;
    uint64_t now;
    int result;

    if (!state->announced)
    {
        result = LiSendControllerArrivalEvent(0, 1, LI_CTYPE_PS, supported_buttons,
                                              LI_CCAP_ANALOG_TRIGGERS);
        state->arrival_result = result;
        if (result != 0)
        {
            ++state->send_errors;
            return;
        }
        state->announced = 1;
    }

    now = monotonic_us();
    if (state->last_event_us != 0 && !memcmp(event, &state->last_event, sizeof(*event)) &&
        now - state->last_event_us < CONTROLLER_KEEPALIVE_US)
        return;
    result =
        LiSendMultiControllerEvent(0, 1, event->buttons, event->left_trigger, event->right_trigger,
                                   event->left_x, event->left_y, event->right_x, event->right_y);
    if (result != 0)
    {
        ++state->send_errors;
        return;
    }
    state->last_event = *event;
    state->last_event_us = now;
    ++state->events;
}

typedef struct mouse_button_mapping
{
    uint32_t pad_button;
    int mouse_button;
} mouse_button_mapping_t;

static const mouse_button_mapping_t mouse_button_mappings[] = {
    {PS5_PAD_BUTTON_CROSS, BUTTON_LEFT},    {PS5_PAD_BUTTON_CIRCLE, BUTTON_RIGHT},
    {PS5_PAD_BUTTON_SQUARE, BUTTON_MIDDLE}, {PS5_PAD_BUTTON_L1, BUTTON_X1},
    {PS5_PAD_BUTTON_R1, BUTTON_X2},
};

static void ps5_controller_mouse_buttons(ps5_controller_state_t *state, uint32_t previous,
                                         uint32_t current)
{
    for (size_t index = 0; index < sizeof(mouse_button_mappings) / sizeof(mouse_button_mappings[0]);
         ++index)
    {
        const mouse_button_mapping_t *mapping = &mouse_button_mappings[index];
        const int was_down = (previous & mapping->pad_button) != 0;
        const int is_down = (current & mapping->pad_button) != 0;
        int result;

        if (was_down == is_down)
            continue;
        result = LiSendMouseButtonEvent(is_down ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE,
                                        mapping->mouse_button);
        if (result != 0)
            ++state->mouse_errors;
        else
            ++state->mouse_button_events;
        if (is_down)
            state->mouse_buttons_down |= mapping->pad_button;
        else
            state->mouse_buttons_down &= ~mapping->pad_button;
    }
}

static void ps5_controller_release_mouse_buttons(ps5_controller_state_t *state)
{
    ps5_controller_mouse_buttons(state, state->mouse_buttons_down, 0);
    state->last_mouse_buttons = 0;
}

static void ps5_controller_mouse_scroll(ps5_controller_state_t *state, uint32_t previous,
                                        uint32_t current)
{
    int result = 0;

    if ((current & PS5_PAD_BUTTON_UP) != 0 && (previous & PS5_PAD_BUTTON_UP) == 0)
        result = LiSendScrollEvent(1);
    else if ((current & PS5_PAD_BUTTON_DOWN) != 0 && (previous & PS5_PAD_BUTTON_DOWN) == 0)
        result = LiSendScrollEvent(-1);
    else if ((current & PS5_PAD_BUTTON_RIGHT) != 0 && (previous & PS5_PAD_BUTTON_RIGHT) == 0)
        result = LiSendHScrollEvent(1);
    else if ((current & PS5_PAD_BUTTON_LEFT) != 0 && (previous & PS5_PAD_BUTTON_LEFT) == 0)
        result = LiSendHScrollEvent(-1);
    else
        return;
    if (result != 0)
        ++state->mouse_errors;
    else
        ++state->mouse_scroll_events;
}

static void ps5_controller_mouse_motion(ps5_controller_state_t *state)
{
    const controller_event_t *event = &state->mouse_event;
    int32_t left_strength, right_strength;
    int16_t raw_x, raw_y, delta_x, delta_y;
    uint64_t now = monotonic_us();

    if (now < state->next_mouse_motion_us)
        return;
    state->next_mouse_motion_us = now + MOONLIGHT_MOUSE_EMULATION_POLL_US;
    left_strength = abs(event->left_x) + abs(event->left_y);
    right_strength = abs(event->right_x) + abs(event->right_y);
    if (left_strength > right_strength)
    {
        raw_x = event->left_x;
        raw_y = (int16_t)-event->left_y;
    }
    else
    {
        raw_x = event->right_x;
        raw_y = (int16_t)-event->right_y;
    }
    delta_x = moonlight_stream_mouse_axis_delta(raw_x);
    delta_y = moonlight_stream_mouse_axis_delta(raw_y);
    if (delta_x == 0 && delta_y == 0)
        return;
    if (LiSendMouseMoveEvent(delta_x, delta_y) != 0)
        ++state->mouse_errors;
    else
        ++state->mouse_motion_events;
}

static void ps5_controller_set_mouse_mode(ps5_controller_state_t *state, int enabled)
{
    const controller_event_t neutral_event = {};

    state->mouse_mode = enabled != 0;
    ++state->mouse_toggles;
    if (state->mouse_mode)
    {
        /* Keep Sunshine's virtual controller alive and release any gamepad
         * state before local mouse emulation takes ownership. */
        ps5_controller_send(state, &neutral_event);
        state->next_mouse_motion_us = monotonic_us() + MOONLIGHT_MOUSE_EMULATION_POLL_US;
        state->mouse_buttons_down = 0;
    }
    else
    {
        ps5_controller_release_mouse_buttons(state);
        memset(&state->mouse_event, 0, sizeof(state->mouse_event));
        /* Force the first restored gamepad sample through the deduplicator. */
        state->last_event_us = 0;
    }
    snprintf(notification.message, sizeof(notification.message),
             "ProsperoLight: %s mode enabled. Select + Square switches to %s.",
             state->mouse_mode ? "Mouse" : "Controller",
             state->mouse_mode ? "controller" : "mouse");
    (void)sceKernelSendNotificationRequest(0, &notification, sizeof(notification), 0);
    (void)lan_http_report_text(notification.message);
}

static int ps5_send_windows_key(uint16_t virtual_key, int shifted)
{
    const short key = (short)(0x8000u | virtual_key);
    const char modifiers = shifted ? MODIFIER_SHIFT : 0;
    int result = LiSendKeyboardEvent(key, KEY_ACTION_DOWN, modifiers);

    if (result == 0)
        result = LiSendKeyboardEvent(key, KEY_ACTION_UP, modifiers);
    return result;
}

static void ps5_controller_set_keyboard_mode(ps5_controller_state_t *state, int enabled)
{
    static const controller_event_t neutral_event = {};

    state->keyboard_mode = enabled != 0;
    if (state->keyboard_mode)
    {
        if (state->mouse_mode)
            ps5_controller_set_mouse_mode(state, 0);
        ps5_controller_send(state, &neutral_event);
    }
    else
        state->last_event_us = 0;
    native_agc_set_keyboard_state(state->keyboard_mode, state->keyboard_selected,
                                  state->keyboard_shifted);
}

static void ps5_controller_activate_keyboard_key(ps5_controller_state_t *state)
{
    const moonlight_keyboard_key &key = moonlight_keyboard_keys[state->keyboard_selected];
    int result = 0;

    switch (key.action)
    {
    case moonlight_keyboard_action::shift:
        state->keyboard_shifted = !state->keyboard_shifted;
        break;
    case moonlight_keyboard_action::space:
    case moonlight_keyboard_action::backspace:
        result = ps5_send_windows_key(key.virtual_key, 0);
        break;
    case moonlight_keyboard_action::enter:
        result = ps5_send_windows_key(key.virtual_key, 0);
        ps5_controller_set_keyboard_mode(state, 0);
        return;
    case moonlight_keyboard_action::close:
        ps5_controller_set_keyboard_mode(state, 0);
        return;
    case moonlight_keyboard_action::key:
        result = ps5_send_windows_key(key.virtual_key, state->keyboard_shifted);
        break;
    }
    if (result != 0)
    {
        snprintf(notification.message, sizeof(notification.message),
                 "ProsperoLight: Could not send keyboard input.");
        (void)sceKernelSendNotificationRequest(0, &notification, sizeof(notification), 0);
        (void)lan_http_report_text(notification.message);
    }
}

static int pressed_edge(uint32_t current, uint32_t previous, uint32_t button)
{
    return (current & button) != 0 && (previous & button) == 0;
}

static void ps5_controller_keyboard_input(ps5_controller_state_t *state, uint32_t current,
                                          uint32_t previous)
{
    uint32_t previous_selected = state->keyboard_selected;
    int previous_shifted = state->keyboard_shifted;

    if (current & PS5_PAD_BUTTON_TOUCH_PAD)
        return;
    if (pressed_edge(current, previous, PS5_PAD_BUTTON_LEFT))
        state->keyboard_selected = moonlight_keyboard_move(state->keyboard_selected, -1, 0);
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_RIGHT))
        state->keyboard_selected = moonlight_keyboard_move(state->keyboard_selected, 1, 0);
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_UP))
        state->keyboard_selected = moonlight_keyboard_move(state->keyboard_selected, 0, -1);
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_DOWN))
        state->keyboard_selected = moonlight_keyboard_move(state->keyboard_selected, 0, 1);
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_CROSS))
        ps5_controller_activate_keyboard_key(state);
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_SQUARE))
        (void)ps5_send_windows_key(0x08, 0);
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_TRIANGLE))
        state->keyboard_shifted = !state->keyboard_shifted;
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_OPTIONS))
    {
        (void)ps5_send_windows_key(0x0d, 0);
        ps5_controller_set_keyboard_mode(state, 0);
        return;
    }
    else if (pressed_edge(current, previous, PS5_PAD_BUTTON_CIRCLE))
    {
        ps5_controller_set_keyboard_mode(state, 0);
        return;
    }
    if (state->keyboard_mode && (state->keyboard_selected != previous_selected ||
                                 state->keyboard_shifted != previous_shifted))
        native_agc_set_keyboard_state(1, state->keyboard_selected, state->keyboard_shifted);
}

static void ps5_controller_poll(ps5_controller_state_t *state)
{
    static const uint32_t hud_chord = PS5_PAD_BUTTON_TOUCH_PAD | PS5_PAD_BUTTON_R1;
    static const uint32_t keyboard_chord = PS5_PAD_BUTTON_TOUCH_PAD | PS5_PAD_BUTTON_TRIANGLE;
    static const uint32_t mouse_chord = PS5_PAD_BUTTON_TOUCH_PAD | PS5_PAD_BUTTON_SQUARE;
    static const controller_event_t neutral_event = {};
    int count;

    if (state->handle < 0)
        return;
    ++state->polls;
    count = scePadRead(state->handle, state->sample_batch, PS5_PAD_SAMPLE_CAPACITY);
    if (count < 0)
    {
        ++state->read_errors;
        return;
    }
    if (count == 0)
    {
        ++state->empty_reads;
        if (state->mouse_mode)
        {
            ps5_controller_mouse_motion(state);
            ps5_controller_send(state, &neutral_event);
        }
        else
            ps5_controller_send(state, &state->last_event);
        return;
    }
    state->samples += (uint32_t)count;
    if ((uint32_t)count > state->max_batch)
        state->max_batch = (uint32_t)count;

    {
        ps5_pad_sample_t *sample = ps5_controller_newest_sample(state, count);
        controller_event_t event;
        uint32_t raw_buttons = sample->buttons;
        uint32_t mouse_buttons;
        int mouse_toggle;
        int keyboard_toggle;
        int intercepted = (raw_buttons & PS5_PAD_BUTTON_INTERCEPTED) != 0;
        int neutral = !sample->connected || intercepted;

        if (!sample->connected)
            ++state->disconnected_samples;
        if (intercepted)
            ++state->intercepted_samples;
        if (!state->connected_count_valid || sample->connected_count != state->connected_count)
        {
            state->connected_count = sample->connected_count;
            state->connected_count_valid = 1;
            state->last_raw_buttons = 0;
            ps5_controller_release_mouse_buttons(state);
        }
        if (neutral)
            raw_buttons = 0;
        mouse_toggle = moonlight_stream_mouse_toggle_requested(raw_buttons) &&
                       !moonlight_stream_mouse_toggle_requested(state->last_raw_buttons);
        keyboard_toggle = moonlight_stream_keyboard_requested(raw_buttons) &&
                          !moonlight_stream_keyboard_requested(state->last_raw_buttons);

        if (moonlight_stream_disconnect_requested(raw_buttons))
        {
            state->requested_stop = 1;
            return;
        }
        if (moonlight_stream_hud_toggle_requested(raw_buttons) &&
            !moonlight_stream_hud_toggle_requested(state->last_raw_buttons))
            native_agc_set_hud_enabled(!native_agc_hud_enabled());
        if (moonlight_stream_hud_toggle_requested(raw_buttons))
            sample->buttons &= ~hud_chord;
        if (keyboard_toggle)
            ps5_controller_set_keyboard_mode(state, !state->keyboard_mode);
        if (moonlight_stream_keyboard_requested(raw_buttons))
            sample->buttons &= ~keyboard_chord;
        if (moonlight_stream_mouse_toggle_requested(raw_buttons))
            sample->buttons &= ~mouse_chord;

        if (state->keyboard_mode)
        {
            if (!keyboard_toggle)
                ps5_controller_keyboard_input(state, raw_buttons, state->last_raw_buttons);
            state->last_raw_buttons = raw_buttons;
            ps5_controller_send(state, &neutral_event);
            return;
        }

        event = ps5_controller_map_sample(sample, neutral);
        mouse_buttons = neutral ? 0 : sample->buttons;
        state->last_raw_buttons = raw_buttons;
        state->observed_raw_buttons |= raw_buttons;
        state->observed_moonlight_buttons |= (uint32_t)event.buttons;
        if (event.buttons || event.left_trigger || event.right_trigger || event.left_x ||
            event.left_y || event.right_x || event.right_y)
            ++state->nonneutral_samples;
        if (state->mouse_mode)
        {
            if (mouse_toggle)
            {
                ps5_controller_set_mouse_mode(state, 0);
                ps5_controller_send(state, &event);
            }
            else
            {
                ps5_controller_mouse_buttons(state, state->last_mouse_buttons, mouse_buttons);
                ps5_controller_mouse_scroll(state, state->last_mouse_buttons, mouse_buttons);
                state->last_mouse_buttons = mouse_buttons;
                state->mouse_event = event;
                ps5_controller_mouse_motion(state);
                ps5_controller_send(state, &neutral_event);
            }
        }
        else
        {
            ps5_controller_send(state, &event);
            if (mouse_toggle)
            {
                state->last_mouse_buttons = mouse_buttons;
                state->mouse_event = event;
                ps5_controller_set_mouse_mode(state, 1);
            }
        }
    }
}

static void ps5_controller_stop(ps5_controller_state_t *state)
{
    ps5_controller_release_mouse_buttons(state);
    if (!state->announced)
        return;
    state->removal_result = LiSendMultiControllerEvent(0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (state->removal_result != 0)
        ++state->send_errors;
    state->announced = 0;
}

static void ps5_controller_shutdown(ps5_controller_state_t *state)
{
    native_agc_set_keyboard_state(0, 0, 0);
    if (state->handle >= 0)
    {
        (void)scePadClose(state->handle);
        state->handle = -1;
    }
    if (state->user_service_result == 0)
        (void)sceUserServiceTerminate();
}

static void connection_stage_starting(int stage)
{
    connection_loading_state_t *loading =
        std::atomic_load_explicit(&active_connection_loading, std::memory_order_acquire);
    char receipt[256];

    if (loading)
        std::atomic_store_explicit(&loading->connection_pending, 1, std::memory_order_release);
    snprintf(receipt, sizeof(receipt), "Moonlight connection stage %d: %s", stage,
             LiGetStageName(stage));
    (void)lan_http_report_text(receipt);
}

static void connection_stage_complete(int stage)
{
    char receipt[256];

    snprintf(receipt, sizeof(receipt), "Moonlight connection stage complete %d: %s", stage,
             LiGetStageName(stage));
    (void)lan_http_report_text(receipt);
}

static void connection_log(const char *format, ...)
{
    char message[320];
    char receipt[352];
    va_list arguments;
    size_t length;

    va_start(arguments, format);
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    length = strlen(message);
    while (length && (message[length - 1] == '\n' || message[length - 1] == '\r'))
        message[--length] = '\0';
    snprintf(receipt, sizeof(receipt), "Moonlight[C] %s", message);
    (void)lan_http_report_text(receipt);
}

static void connection_stage_failed(int stage, int error)
{
    char receipt[256];
    connection_error = error;
    connection_terminated = 1;
    snprintf(receipt, sizeof(receipt), "Moonlight connection failed: stage=%d %s error=%08x", stage,
             LiGetStageName(stage), (uint32_t)error);
    (void)lan_http_report_text(receipt);
}

static void connection_started(void)
{
    (void)lan_http_report_text("Moonlight connection started");
}

static void connection_ended(int error)
{
    char receipt[256];
    connection_error = error;
    connection_terminated = 1;
    snprintf(receipt, sizeof(receipt), "Moonlight connection terminated: error=%08x",
             (uint32_t)error);
    (void)lan_http_report_text(receipt);
}

static void connection_set_hdr_mode(bool enabled)
{
    SS_HDR_METADATA metadata = {};
    char receipt[384];
    int metadata_valid = 0;

    std::atomic_store_explicit(&host_hdr_active, enabled ? 1u : 0u, std::memory_order_relaxed);
    std::atomic_fetch_add_explicit(&host_hdr_transitions, 1u, std::memory_order_relaxed);
    if (enabled)
        metadata_valid = LiGetHdrMetadata(&metadata) ? 1 : 0;
    snprintf(receipt, sizeof(receipt),
             "Moonlight HDR mode: active=%u transitions=%u metadata=%u max_nits=%u min_1e4_nits=%u "
             "max_cll=%u max_fall=%u",
             enabled ? 1u : 0u,
             std::atomic_load_explicit(&host_hdr_transitions, std::memory_order_relaxed),
             metadata_valid ? 1u : 0u, metadata.maxDisplayLuminance, metadata.minDisplayLuminance,
             metadata.maxContentLightLevel, metadata.maxFrameAverageLightLevel);
    (void)lan_http_report_text(receipt);
}

static CONNECTION_LISTENER_CALLBACKS moonlight_connection_callbacks = {
    .stageStarting = connection_stage_starting,
    .stageComplete = connection_stage_complete,
    .stageFailed = connection_stage_failed,
    .connectionStarted = connection_started,
    .connectionTerminated = connection_ended,
    .logMessage = connection_log,
    .rumble = nullptr,
    .connectionStatusUpdate = nullptr,
    .setHdrMode = connection_set_hdr_mode,
    .rumbleTriggers = nullptr,
    .setMotionEventState = nullptr,
    .setControllerLED = nullptr,
    .setAdaptiveTriggers = nullptr,
};

static void nvhttp_log_sink(const char *message)
{
    (void)lan_http_report_text(message);
}

static int prepare_native_session(client_identity_t *identity, gs_server_t *server,
                                  STREAM_CONFIGURATION *configuration,
                                  const native_video_mode_t *mode, int gamepad_mask,
                                  const char *host, const char *app_name, int requested_app_id)
{
    app_entry_t *apps = NULL;
    app_entry_t *app;
    char target_name[64] = {};
    int target_id = 0;
    int app_count = 0;
    int resume_requested = 0;
    int result;

    gs_log_set_sink(nvhttp_log_sink);
    result = identity_init(identity, MOONLIGHT_IDENTITY_DIRECTORY);
    if (result != GS_OK)
        return result;
    http_init(identity, 1);
    result = gs_init(server, identity, host, 47989);
    snprintf(notification.message, sizeof(notification.message),
             "Native NVHTTP serverinfo: rc=%08x paired=%u app=%s https=%u codec=%08x error=%s",
             (uint32_t)result, server->paired, server->app_version, server->https_port,
             (uint32_t)server->server_codec_mode_support, gs_error ? gs_error : "");
    (void)lan_http_report_text(notification.message);
    if (result != GS_OK)
        return result;

    if (!server->paired)
    {
        gs_error = "Pair this client from the launcher";
        return GS_WRONG_STATE;
    }
    if (!mode ||
        (mode->video_format == VIDEO_FORMAT_H265_MAIN10 &&
         !(server->server_codec_mode_support & SCM_HEVC_MAIN10)) ||
        (mode->video_format == VIDEO_FORMAT_H265 &&
         !(server->server_codec_mode_support & SCM_HEVC)) ||
        (mode->video_format == VIDEO_FORMAT_H264 &&
         !(server->server_codec_mode_support & SCM_MASK_H264)))
    {
        gs_error = "Selected video codec is not supported by this Sunshine PC";
        return GS_NOT_SUPPORTED_MODE;
    }

    result = gs_applist(server, &apps);
    if (result != GS_OK)
        return result;
    for (app = apps; app; app = app->next)
    {
        ++app_count;
        if ((requested_app_id > 0 && app->id == requested_app_id) ||
            (requested_app_id <= 0 && app->name && !strcmp(app->name, app_name)))
        {
            target_id = app->id;
            snprintf(target_name, sizeof(target_name), "%s", app->name ? app->name : app_name);
        }
    }
    snprintf(notification.message, sizeof(notification.message),
             "Native NVHTTP applist: rc=00000000 apps=%d target=%s id=%d", app_count,
             target_name[0] ? target_name : app_name, target_id);
    (void)lan_http_report_text(notification.message);
    xml_applist_free(apps);
    if (!target_id)
    {
        gs_error = "Requested Sunshine app was not found";
        return GS_INVALID;
    }
    if (server->current_game && server->current_game != target_id)
    {
        result = gs_quit_app(server);
        if (result != GS_OK)
            return result;
    }
    resume_requested = server->current_game == target_id;
    result = gs_start_app(server, configuration, target_id, true, false, gamepad_mask);
    snprintf(notification.message, sizeof(notification.message),
             "Native NVHTTP launch: rc=%08x action=%s target=%s id=%d gamepads=%x rtsp=%s error=%s",
             (uint32_t)result, resume_requested ? "resume" : "launch",
             target_name[0] ? target_name : app_name, target_id, gamepad_mask,
             server->rtsp_session_url, gs_error ? gs_error : "");
    (void)lan_http_report_text(notification.message);
    if (result != GS_OK || !server->rtsp_session_url[0])
    {
        if (result == GS_OK)
        {
            gs_error = "Sunshine launch omitted the RTSP session URL";
            result = GS_INVALID;
        }
        return result;
    }
    return GS_OK;
}

int moonlight_stream_run(const moonlight_stream_options_t *options,
                         moonlight_stream_metrics_t *metrics)
{
    videodec2_decoder_config_t config;
    videodec2_decoder_memory_t memory = {};
    videodec2_compute_config_t compute_config = {};
    videodec2_compute_memory_t compute_memory = {};
    native_renderer_state_t renderer = {};
    connection_loading_state_t loading = {};
    ps5_controller_state_t controller{};
    ps5_physical_input_state_t physical_input{};
    client_identity_t client_identity;
    gs_server_t gs_server;
    STREAM_CONFIGURATION stream_config;
    void *decoder = NULL;
    void *compute_queue = NULL;
    void *input_memory = NULL;
    void *frame_memory = NULL;
    int64_t compute_start = -1;
    int64_t gpu_start = -1;
    int64_t cpu_gpu_start = -1;
    int64_t input_start = -1;
    int64_t frame_start = -1;
    int64_t direct_memory_limit;
    size_t cpu_mapping_size = 0;
    size_t flexible_available = 0;
    uint8_t cpu_mapping_info[0x48] = {};
    size_t compute_size = 0;
    size_t gpu_size = 0;
    size_t cpu_gpu_size = 0;
    size_t input_size = 0;
    size_t frame_size = 0;
    size_t input_pool_size = 0;
    size_t frame_pool_size = 0;
    int32_t result;
    int32_t present_cleanup_result = -1;
    int32_t delete_result = -1;
    int32_t release_compute_result = -1;
    int32_t unload_result = -1;
    int sysmodule_loaded = 0;
    uint64_t live_elapsed_us = 0;
    uint64_t first_frame_wait_start_us = 0;
    int connection_result = -1;
    int connection_active = 0;
    int identity_initialized = 0;
    int session_started = 0;
    int controller_ready = 0;
    int physical_input_ready = 0;
    int first_frame_timed_out = 0;
    int terminated = 0;
    int reported_error = 0;
    int user_stop = 0;
    uint32_t presented = 0;
    char stream_error[192] = {};
    uint32_t synthetic_motion_events = 0;
    uint32_t synthetic_motion_errors = 0;
    uint64_t synthetic_motion_next_us = 0;
    const char *host = options && options->host && options->host[0] ? options->host : "";
    const char *app_name =
        options && options->app_name && options->app_name[0] ? options->app_name : "Desktop";
    const int app_id = options ? options->app_id : 0;
    const uint32_t bitrate_kbps = options && options->bitrate_kbps ? options->bitrate_kbps : 20000u;
    const native_video_mode_t *mode =
        find_video_mode(options ? options->video_codec : MOONLIGHT_VIDEO_CODEC_H264,
                        options ? options->stream_resolution : MOONLIGHT_STREAM_RESOLUTION_1080P,
                        options ? options->hdr_enabled : 0u);

    controller.user_service_result = -1;
    controller.user_result = -1;
    controller.pad_init_result = -1;
    controller.user_id = -1;
    controller.handle = -1;
    controller.arrival_result = -1;
    controller.removal_result = -1;
    physical_input.keyboard_module_result = -1;
    physical_input.keyboard_unload_result = -1;
    physical_input.keyboard_init_result = -1;
    physical_input.keyboard_open_result = -1;
    physical_input.mouse_module_result = -1;
    physical_input.mouse_unload_result = -1;
    physical_input.mouse_init_result = -1;
    physical_input.mouse_open_result = -1;
    physical_input.keyboard_close_result = -1;
    physical_input.mouse_close_result = -1;
    native_agc_set_tv_safe_area(!options ||
                                options->display_area == MOONLIGHT_DISPLAY_AREA_TV_SAFE);

    if (metrics)
        memset(metrics, 0, sizeof(*metrics));

    if (!host[0] || !mode)
        return -1;
    lan_http_report_set_host(host);

    result = start_connection_loading(&loading, NULL, 0, mode->hdr, mode->visible_width,
                                      mode->visible_height, NULL);
    if (result != 0)
        goto done;

    snprintf(notification.message, sizeof(notification.message),
             "Native zero-copy stage 1: mode=%s bitrate=%u kbps input_slot=%x", mode->name,
             bitrate_kbps, INPUT_SLOT_BYTES);
    (void)lan_http_report_text(notification.message);
    sceSystemServiceHideSplashScreen();
    result = sceSysmoduleLoadModule(207);
    snprintf(notification.message, sizeof(notification.message),
             "Native zero-copy stage 2: sysmodule207=%08x", (uint32_t)result);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;
    sysmodule_loaded = 1;

    direct_memory_limit = sceKernelGetDirectMemorySize();
    compute_memory.size = sizeof(compute_memory);
    result = sceVideodec2QueryComputeMemoryInfo(&compute_memory);
    compute_size = align_16k((size_t)compute_memory.cpu_gpu_size);
    snprintf(notification.message, sizeof(notification.message),
             "Native compute query: rc=%08x shared=%llx mapped=%zx", (uint32_t)result,
             (unsigned long long)compute_memory.cpu_gpu_size, compute_size);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    result = allocate_direct(compute_size, 0x33, direct_memory_limit, &compute_start,
                             &compute_memory.cpu_gpu);
    if (result == 0)
    {
        compute_memory.cpu_gpu_size = compute_size;
        compute_config.size = sizeof(compute_config);
        compute_config.pipe_id = 0;
        compute_config.queue_id = 0;
        result = sceVideodec2AllocateComputeQueue(&compute_config, &compute_memory, &compute_queue);
    }
    snprintf(notification.message, sizeof(notification.message),
             "Native compute queue: rc=%08x memory=%p/%zx queue=%p", (uint32_t)result,
             compute_memory.cpu_gpu, compute_size, compute_queue);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    memset(&config, 0, sizeof(config));
    config.size = sizeof(config);
    config.resource_type = 1;
    config.codec_type = mode->codec_type;
    config.profile = mode->profile;
    config.max_level = mode->max_level;
    config.max_width = (int32_t)mode->max_width;
    config.max_height = (int32_t)mode->max_height;
    config.max_dpb_frames = 4;
    config.pipeline_depth = 1;
    config.compute_queue = (uint64_t)compute_queue;
    config.cpu_affinity = 0x3f;
    config.cpu_priority = 700;
    config.optimize_progressive = 1;
    memset(&memory, 0, sizeof(memory));
    memory.size = sizeof(memory);
    result = sceVideodec2QueryDecoderMemoryInfo(&config, &memory);
    if (result != 0)
        goto done;

    snprintf(notification.message, sizeof(notification.message),
             "Native embedded query: cpu=%llx gpu=%llx shared=%llx frame=%llx align=%x",
             (unsigned long long)memory.cpu_size, (unsigned long long)memory.gpu_size,
             (unsigned long long)memory.cpu_gpu_size, (unsigned long long)memory.max_frame_size,
             memory.frame_alignment);
    (void)lan_http_report_text(notification.message);

    cpu_mapping_size = align_16k((size_t)memory.cpu_size);
    result = sceKernelAvailableFlexibleMemorySize(&flexible_available);
    if (result == 0)
        result = sceKernelMapNamedFlexibleMemory(&memory.cpu, cpu_mapping_size, 0x03, 0,
                                                 "MoonlightVdecCpu");
    snprintf(
        notification.message, sizeof(notification.message),
        "Native flexible CPU workspace: rc=%08x available=%zx requested=%llx mapped=%zx ptr=%p",
        (uint32_t)result, flexible_available, (unsigned long long)memory.cpu_size, cpu_mapping_size,
        memory.cpu);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    result = sceKernelVirtualQuery(memory.cpu, 0, cpu_mapping_info, sizeof(cpu_mapping_info));
    snprintf(notification.message, sizeof(notification.message),
             "Native flexible CPU query: rc=%08x protection=%x type=%x flags=%x", (uint32_t)result,
             *(const uint32_t *)(cpu_mapping_info + 0x18),
             *(const uint32_t *)(cpu_mapping_info + 0x1c),
             *(const uint32_t *)(cpu_mapping_info + 0x20));
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    gpu_size = align_16k((size_t)memory.gpu_size);
    cpu_gpu_size = align_16k((size_t)memory.cpu_gpu_size);
    input_size = INPUT_SLOT_BYTES;
    frame_size = align_16k((size_t)memory.max_frame_size);
    input_pool_size = input_size * PIPELINE_BUFFER_COUNT;
    frame_pool_size = frame_size * PIPELINE_BUFFER_COUNT;
    memory.gpu_size = gpu_size;
    if (cpu_gpu_size != 0)
        memory.cpu_gpu_size = cpu_gpu_size;
    result = allocate_direct(gpu_size, 0x32, direct_memory_limit, &gpu_start, &memory.gpu);
    if (result == 0)
        if (cpu_gpu_size != 0)
            result = allocate_direct(cpu_gpu_size, 0x33, direct_memory_limit, &cpu_gpu_start,
                                     &memory.cpu_gpu);
    if (result == 0)
        result = allocate_direct(input_pool_size, 0x32, direct_memory_limit, &input_start,
                                 &input_memory);
    if (result == 0)
        result = allocate_direct(frame_pool_size, 0x32, direct_memory_limit, &frame_start,
                                 &frame_memory);
    snprintf(notification.message, sizeof(notification.message),
             "Native zero-copy stage 3: alloc=%08x gpu=%p/%zx shared=%p/%zx input_pool=%p/%zx "
             "slots=%u/%zx frame_pool=%p/%zx slots=%u/%zx",
             (uint32_t)result, memory.gpu, gpu_size, memory.cpu_gpu, cpu_gpu_size, input_memory,
             input_pool_size, PIPELINE_BUFFER_COUNT, input_size, frame_memory, frame_pool_size,
             PIPELINE_BUFFER_COUNT, frame_size);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    result = sceVideodec2CreateDecoder(&config, &memory, &decoder);
    snprintf(notification.message, sizeof(notification.message),
             "Native zero-copy stage 4: create=%08x decoder=%p", (uint32_t)result, decoder);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    result = sceVideodec2Reset(decoder);
    snprintf(notification.message, sizeof(notification.message),
             "Native compute zero-copy stage 5: reset=%08x direct_maps=skipped", (uint32_t)result);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;

    renderer.decoder = decoder;
    renderer.mode = mode;
    renderer.input_memory = input_memory;
    renderer.frame_memory = frame_memory;
    renderer.input_size = input_size;
    renderer.frame_size = frame_size;

    if (std::atomic_load_explicit(&loading.cancel_requested, std::memory_order_relaxed))
    {
        if (std::atomic_load_explicit(&loading.timed_out, std::memory_order_relaxed))
        {
            gs_error = "Decoder setup timed out";
            result = GS_IO_ERROR;
        }
        else
            result = 0;
        goto done;
    }
    stop_connection_loading();

    LiInitializeStreamConfiguration(&stream_config);
    stream_config.width = (int)mode->visible_width;
    stream_config.height = (int)mode->visible_height;
    stream_config.fps = 60;
    stream_config.bitrate = (int)bitrate_kbps;
    stream_config.packetSize = 1392;
    stream_config.streamingRemotely = STREAM_CFG_LOCAL;
    stream_config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    stream_config.supportedVideoFormats = mode->video_format;
    stream_config.clientRefreshRateX100 = 6000;
    stream_config.colorSpace = mode->hdr ? COLORSPACE_REC_2020 : COLORSPACE_REC_709;
    stream_config.colorRange = COLOR_RANGE_LIMITED;
    stream_config.encryptionFlags = ENCFLG_NONE;
    controller_ready = ps5_controller_init(&controller) == 0;
    if (controller.user_id >= 0)
        physical_input_ready = ps5_physical_input_init(&physical_input, controller.user_id) == 0;
    snprintf(notification.message, sizeof(notification.message),
             "Moonlight controller init: ready=%d user_service=%08x user=%08x pad_init=%08x "
             "handle=%08x launch_mask=%x",
             controller_ready, (uint32_t)controller.user_service_result,
             (uint32_t)controller.user_result, (uint32_t)controller.pad_init_result,
             (uint32_t)controller.handle, controller_ready ? 1 : 0);
    (void)lan_http_report_text(notification.message);
    snprintf(notification.message, sizeof(notification.message),
             "Moonlight physical input init: ready=%d keyboard_module=%08x keyboard_init=%08x "
             "keyboard_open=%08x handles=%u mouse_module=%08x mouse_init=%08x mouse_open=%08x "
             "handles=%u",
             physical_input_ready, (uint32_t)physical_input.keyboard_module_result,
             (uint32_t)physical_input.keyboard_init_result,
             (uint32_t)physical_input.keyboard_open_result, physical_input.keyboard_handle_count,
             (uint32_t)physical_input.mouse_module_result,
             (uint32_t)physical_input.mouse_init_result, (uint32_t)physical_input.mouse_open_result,
             physical_input.mouse_handle_count);
    (void)lan_http_report_text(notification.message);
    result =
        start_connection_loading(&loading, frame_memory, frame_size, mode->hdr, mode->visible_width,
                                 mode->visible_height, controller_ready ? &controller : NULL);
    snprintf(notification.message, sizeof(notification.message),
             "Native connecting animation: present=%08x thread=%08x hdr=%u", (uint32_t)result,
             (uint32_t)loading.create_result, mode->hdr ? 1u : 0u);
    (void)lan_http_report_text(notification.message);
    if (result != 0)
        goto done;
    identity_initialized = 1;
    result = prepare_native_session(&client_identity, &gs_server, &stream_config, mode,
                                    controller_ready ? 1 : 0, host, app_name, app_id);
    if (result != GS_OK)
        goto done;
    session_started = 1;
    if (std::atomic_load_explicit(&loading.cancel_requested, std::memory_order_relaxed))
    {
        if (std::atomic_load_explicit(&loading.timed_out, std::memory_order_relaxed))
        {
            gs_error = "Sunshine connection setup timed out";
            result = GS_IO_ERROR;
        }
        else
            result = 0;
        goto done;
    }

    connection_terminated = 0;
    connection_error = 0;
    std::atomic_store_explicit(&host_hdr_active, 0u, std::memory_order_relaxed);
    std::atomic_store_explicit(&host_hdr_transitions, 0u, std::memory_order_relaxed);
    std::atomic_store_explicit(&loading.connection_pending, 1, std::memory_order_release);
    connection_result = LiStartConnection(
        &gs_server.server_info, &stream_config, &moonlight_connection_callbacks,
        &moonlight_video_callbacks, &moonlight_audio_callbacks, &renderer, 0, NULL, 0);
    std::atomic_store_explicit(&loading.connection_pending, 0, std::memory_order_release);
    stop_connection_loading();
    if (connection_result != 0)
    {
        if (controller.requested_stop)
            result = 0;
        else if (std::atomic_load_explicit(&loading.timed_out, std::memory_order_relaxed))
        {
            gs_error = "Sunshine connection setup timed out";
            result = GS_IO_ERROR;
        }
        else
            result = connection_result;
        goto done;
    }
    connection_active = 1;
    first_frame_wait_start_us = monotonic_us();
    if (options && options->synthetic_motion)
        synthetic_motion_next_us = monotonic_us();

    while (!connection_terminated && !controller.requested_stop)
    {
        if (options && options->synthetic_motion)
        {
            const uint64_t now = monotonic_us();
            if (now >= synthetic_motion_next_us)
            {
                const short x = (short)((synthetic_motion_events * 32u) % mode->visible_width);
                const short y = (short)(mode->visible_height / 2u);
                if (LiSendMousePositionEvent(x, y, (short)mode->visible_width,
                                             (short)mode->visible_height) != 0)
                    ++synthetic_motion_errors;
                ++synthetic_motion_events;
                synthetic_motion_next_us += UINT64_C(16667);
                if (now > synthetic_motion_next_us + UINT64_C(16667))
                    synthetic_motion_next_us = now + UINT64_C(16667);
            }
        }
        if (controller_ready)
            ps5_controller_poll(&controller);
        if (physical_input_ready)
            ps5_physical_input_poll(&physical_input);
        if (std::atomic_load_explicit(&renderer.presented, std::memory_order_relaxed) == 0 &&
            monotonic_us() - first_frame_wait_start_us >= FIRST_VIDEO_FRAME_TIMEOUT_US)
        {
            first_frame_timed_out = 1;
            gs_error = "Sunshine connected, but no video frame arrived";
            break;
        }
        sceKernelUsleep(4000);
    }

    live_elapsed_us = renderer.last_present_us > renderer.first_present_us
                          ? renderer.last_present_us - renderer.first_present_us
                          : 0;
    terminated = std::atomic_load_explicit(&connection_terminated, std::memory_order_relaxed);
    reported_error = std::atomic_load_explicit(&connection_error, std::memory_order_relaxed);
    user_stop = std::atomic_load_explicit(&controller.requested_stop, std::memory_order_relaxed);
    presented = std::atomic_load_explicit(&renderer.presented, std::memory_order_relaxed);
    result = first_frame_timed_out               ? GS_IO_ERROR
             : terminated && reported_error != 0 ? reported_error
                                                 : 0;

    if (controller_ready)
        ps5_controller_stop(&controller);
    if (physical_input_ready)
        ps5_physical_input_stop(&physical_input);
    LiStopConnection();
    connection_active = 0;
    snprintf(
        notification.message, sizeof(notification.message),
        "Moonlight live result: rc=%08x connection=%08x terminated=%d user_stop=%d error=%08x "
        "access_units=%u presented=%u fragments=%u bytes=%zx frame_span_us=%llu fps_x100=%llu "
        "source=%p",
        (uint32_t)result, (uint32_t)connection_result, terminated, user_stop,
        (uint32_t)reported_error, renderer.access_units, presented, renderer.fragments,
        renderer.stream_bytes, (unsigned long long)live_elapsed_us,
        (unsigned long long)(live_elapsed_us && presented > 1
                                 ? (uint64_t)(presented - 1) * UINT64_C(100000000) / live_elapsed_us
                                 : 0),
        frame_memory);
    (void)lan_http_report_text(notification.message);
    snprintf(notification.message, sizeof(notification.message),
             "Moonlight synthetic motion: enabled=%u events=%u errors=%u",
             options && options->synthetic_motion ? 1u : 0u, synthetic_motion_events,
             synthetic_motion_errors);
    (void)lan_http_report_text(notification.message);
    snprintf(
        notification.message, sizeof(notification.message),
        "Moonlight live timing: copy_calls=%u copy_avg_us=%llu copy_max_us=%llu decode_calls=%u "
        "decode_avg_us=%llu decode_max_us=%llu flush_calls=%u flush_avg_us=%llu flush_max_us=%llu "
        "present_calls=%u present_avg_us=%llu present_max_us=%llu",
        renderer.access_units,
        (unsigned long long)(renderer.access_units ? renderer.copy_total_us / renderer.access_units
                                                   : 0),
        (unsigned long long)renderer.copy_max_us, renderer.decode_calls,
        (unsigned long long)(renderer.decode_calls
                                 ? renderer.decode_total_us / renderer.decode_calls
                                 : 0),
        (unsigned long long)renderer.decode_max_us, renderer.flush_calls,
        (unsigned long long)(renderer.flush_calls ? renderer.flush_total_us / renderer.flush_calls
                                                  : 0),
        (unsigned long long)renderer.flush_max_us, presented,
        (unsigned long long)(presented ? renderer.present_total_us / presented : 0),
        (unsigned long long)renderer.present_max_us);
    (void)lan_http_report_text(notification.message);
    snprintf(notification.message, sizeof(notification.message),
             "Moonlight live latency: calls=%u callback_to_decode_avg_us=%llu min_us=%llu "
             "max_us=%llu callback_to_flip_avg_us=%llu min_us=%llu max_us=%llu pending=%u",
             renderer.latency_calls,
             (unsigned long long)(renderer.latency_calls ? renderer.callback_to_decode_total_us /
                                                               renderer.latency_calls
                                                         : 0),
             (unsigned long long)renderer.callback_to_decode_min_us,
             (unsigned long long)renderer.callback_to_decode_max_us,
             (unsigned long long)(renderer.latency_calls
                                      ? renderer.callback_to_flip_total_us / renderer.latency_calls
                                      : 0),
             (unsigned long long)renderer.callback_to_flip_min_us,
             (unsigned long long)renderer.callback_to_flip_max_us, renderer.submission_count);
    (void)lan_http_report_text(notification.message);

done:
    if (result != 0 && !controller.requested_stop)
    {
        if (std::atomic_load_explicit(&loading.timed_out, std::memory_order_relaxed))
            snprintf(stream_error, sizeof(stream_error),
                     "Connection timed out before streaming started. Returned safely; refresh the "
                     "PC and retry.");
        else if (first_frame_timed_out)
            snprintf(stream_error, sizeof(stream_error),
                     "Sunshine connected but no video arrived. The session was closed; retry or "
                     "choose another codec.");
        else
            snprintf(stream_error, sizeof(stream_error), "Stream failed: %s",
                     gs_error && gs_error[0] ? gs_error
                                             : "Sunshine did not complete the connection");
    }
    if (connection_active)
    {
        if (controller_ready)
            ps5_controller_stop(&controller);
        if (physical_input_ready)
            ps5_physical_input_stop(&physical_input);
        LiStopConnection();
    }
    stop_connection_loading();
    http_clear_interrupt();
    snprintf(
        notification.message, sizeof(notification.message),
        "Moonlight audio result: init=%08x open=%08x opus=%d channels=%d frame_samples=%d "
        "packets=%u plc=%u packet_samples=%u-%u mismatches=%u callback_span_us=%llu "
        "callback_hz_x100=%llu interval_avg_us=%llu interval_min_us=%llu interval_max_us=%llu "
        "decode_errors=%u decoded_frames=%llu nonzero_samples=%llu peak=%u ring=%u overruns=%u "
        "dropped=%llu output_calls=%u output_errors=%u decode_avg_us=%llu decode_max_us=%llu "
        "output_avg_us=%llu output_max_us=%llu rtp_audio=%u rtp_fec=%u recovered=%u failed=%u "
        "oos=%u invalid=%u fec_invalid=%u drain=%08x close=%08x",
        (uint32_t)audio_state.init_result, (uint32_t)audio_state.open_result,
        audio_state.opus_error, audio_state.channels, audio_state.samples_per_frame,
        audio_state.packets, audio_state.plc_packets, audio_state.packet_samples_min,
        audio_state.packet_samples_max, audio_state.packet_sample_mismatches,
        (unsigned long long)(audio_state.last_packet_us > audio_state.first_packet_us
                                 ? audio_state.last_packet_us - audio_state.first_packet_us
                                 : 0),
        (unsigned long long)(audio_state.last_packet_us > audio_state.first_packet_us &&
                                     audio_state.packets > 1
                                 ? (uint64_t)(audio_state.packets - 1) * UINT64_C(100000000) /
                                       (audio_state.last_packet_us - audio_state.first_packet_us)
                                 : 0),
        (unsigned long long)(audio_state.packets > 1
                                 ? audio_state.interval_total_us / (audio_state.packets - 1)
                                 : 0),
        (unsigned long long)audio_state.interval_min_us,
        (unsigned long long)audio_state.interval_max_us, audio_state.decode_errors,
        (unsigned long long)audio_state.decoded_frames,
        (unsigned long long)audio_state.nonzero_samples, audio_state.peak_sample,
        audio_state.ring_count, audio_state.overruns,
        (unsigned long long)audio_state.dropped_frames, audio_state.output_calls,
        audio_state.output_errors,
        (unsigned long long)(audio_state.packets ? audio_state.decode_total_us / audio_state.packets
                                                 : 0),
        (unsigned long long)audio_state.decode_max_us,
        (unsigned long long)(audio_state.output_calls
                                 ? audio_state.output_total_us / audio_state.output_calls
                                 : 0),
        (unsigned long long)audio_state.output_max_us, audio_state.rtp.packetCountAudio,
        audio_state.rtp.packetCountFec, audio_state.rtp.packetCountFecRecovered,
        audio_state.rtp.packetCountFecFailed, audio_state.rtp.packetCountOOS,
        audio_state.rtp.packetCountInvalid, audio_state.rtp.packetCountFecInvalid,
        (uint32_t)audio_state.drain_result, (uint32_t)audio_state.close_result);
    (void)lan_http_report_text(notification.message);
    snprintf(
        notification.message, sizeof(notification.message),
        "Moonlight controller result: ready=%d user_service=%08x user=%08x pad_init=%08x "
        "handle=%08x arrival=%08x removal=%08x polls=%u samples=%u empty=%u max_batch=%u "
        "generation=%u events=%u read_errors=%u send_errors=%u disconnected=%u intercepted=%u "
        "nonneutral=%u raw=%08x mapped=%08x last=%08x triggers=%u,%u axes=%d,%d,%d,%d "
        "mouse_mode=%d toggles=%u motion=%u buttons=%u scroll=%u mouse_errors=%u",
        controller_ready, (uint32_t)controller.user_service_result,
        (uint32_t)controller.user_result, (uint32_t)controller.pad_init_result,
        (uint32_t)controller.handle, (uint32_t)controller.arrival_result,
        (uint32_t)controller.removal_result, controller.polls, controller.samples,
        controller.empty_reads, controller.max_batch, controller.connected_count, controller.events,
        controller.read_errors, controller.send_errors, controller.disconnected_samples,
        controller.intercepted_samples, controller.nonneutral_samples,
        controller.observed_raw_buttons, controller.observed_moonlight_buttons,
        controller.last_raw_buttons, controller.last_event.left_trigger,
        controller.last_event.right_trigger, controller.last_event.left_x,
        controller.last_event.left_y, controller.last_event.right_x, controller.last_event.right_y,
        controller.mouse_mode, controller.mouse_toggles, controller.mouse_motion_events,
        controller.mouse_button_events, controller.mouse_scroll_events, controller.mouse_errors);
    (void)lan_http_report_text(notification.message);
    ps5_physical_input_shutdown(&physical_input);
    snprintf(notification.message, sizeof(notification.message),
             "Moonlight physical input result: ready=%d keyboard_module=%08x open=%08x handles=%u "
             "polls=%u events=%u read_errors=%u send_errors=%u mouse_module=%08x open=%08x "
             "handles=%u polls=%u samples=%u motion=%u buttons=%u scroll=%u read_errors=%u "
             "send_errors=%u keyboard_close=%08x mouse_close=%08x keyboard_unload=%08x "
             "mouse_unload=%08x",
             physical_input_ready, (uint32_t)physical_input.keyboard_module_result,
             (uint32_t)physical_input.keyboard_open_result, physical_input.keyboard_handle_count,
             physical_input.keyboard_polls, physical_input.keyboard_events,
             physical_input.keyboard_read_errors, physical_input.keyboard_send_errors,
             (uint32_t)physical_input.mouse_module_result,
             (uint32_t)physical_input.mouse_open_result, physical_input.mouse_handle_count,
             physical_input.mouse_polls, physical_input.mouse_samples,
             physical_input.mouse_motion_events, physical_input.mouse_button_events,
             physical_input.mouse_scroll_events, physical_input.mouse_read_errors,
             physical_input.mouse_send_errors, (uint32_t)physical_input.keyboard_close_result,
             (uint32_t)physical_input.mouse_close_result,
             (uint32_t)physical_input.keyboard_unload_result,
             (uint32_t)physical_input.mouse_unload_result);
    (void)lan_http_report_text(notification.message);
    ps5_controller_shutdown(&controller);
    if (session_started && !controller.requested_stop)
    {
        http_set_timeout_ms(2000);
        int quit_result = gs_quit_app(&gs_server);
        snprintf(notification.message, sizeof(notification.message),
                 "Native NVHTTP cancel: rc=%08x error=%s", (uint32_t)quit_result,
                 gs_error ? gs_error : "");
        (void)lan_http_report_text(notification.message);
    }
    if (identity_initialized)
    {
        identity_free(&client_identity);
        gs_log_set_sink(NULL);
    }
    if (active_renderer)
    {
        moonlight_video_callbacks.stop();
        moonlight_video_callbacks.cleanup();
    }
    present_cleanup_result = native_agc_present_shutdown();
    if (decoder)
        delete_result = sceVideodec2DeleteDecoder(decoder);
    release_direct(frame_memory, frame_start, frame_pool_size);
    release_direct(input_memory, input_start, input_pool_size);
    release_direct(memory.cpu_gpu, cpu_gpu_start, cpu_gpu_size);
    release_direct(memory.gpu, gpu_start, gpu_size);
    if (memory.cpu)
    {
        (void)sceKernelReleaseFlexibleMemory(memory.cpu, cpu_mapping_size);
        (void)sceKernelMunmap(memory.cpu, cpu_mapping_size);
    }
    if (compute_queue)
        release_compute_result = sceVideodec2ReleaseComputeQueue(compute_queue);
    release_direct(compute_memory.cpu_gpu, compute_start, compute_size);
    if (sysmodule_loaded)
        unload_result = sceSysmoduleUnloadModule(207);
    snprintf(
        notification.message, sizeof(notification.message),
        "Native zero-copy cleanup: rc=%08x present=%08x delete=%08x compute=%08x unload=%08x done",
        (uint32_t)result, (uint32_t)present_cleanup_result, (uint32_t)delete_result,
        (uint32_t)release_compute_result, (uint32_t)unload_result);
    (void)lan_http_report_text(notification.message);
    if (stream_error[0])
    {
        snprintf(notification.message, sizeof(notification.message), "ProsperoLight: %s",
                 stream_error);
        (void)sceKernelSendNotificationRequest(0, &notification, sizeof(notification), 0);
    }
    if (metrics)
    {
        metrics->result = result;
        metrics->presented_frames = renderer.presented;
        metrics->access_units = renderer.access_units;
        metrics->pending_frames = renderer.submission_count;
        metrics->audio_packets = audio_state.packets;
        metrics->audio_overruns = audio_state.overruns;
        metrics->controller_polls = controller.polls;
        metrics->controller_errors = controller.read_errors + controller.send_errors;
        metrics->hdr_active =
            std::atomic_load_explicit(&host_hdr_active, std::memory_order_relaxed);
        metrics->hdr_transitions =
            std::atomic_load_explicit(&host_hdr_transitions, std::memory_order_relaxed);
        metrics->callback_to_flip_average_us =
            renderer.latency_calls ? renderer.callback_to_flip_total_us / renderer.latency_calls
                                   : 0;
        snprintf(metrics->error, sizeof(metrics->error), "%s", stream_error);
    }
    return result;
}

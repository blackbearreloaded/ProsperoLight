/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOONLIGHT_STREAM_INPUT_H
#define MOONLIGHT_STREAM_INPUT_H

#include <stdint.h>

#define MOONLIGHT_PS5_PAD_L1        UINT32_C(0x000400)
#define MOONLIGHT_PS5_PAD_R1        UINT32_C(0x000800)
#define MOONLIGHT_PS5_PAD_OPTIONS   UINT32_C(0x000008)
#define MOONLIGHT_PS5_PAD_SELECT    UINT32_C(0x100000)

#define MOONLIGHT_MOUSE_EMULATION_LONG_PRESS_US UINT64_C(750000)
#define MOONLIGHT_MOUSE_EMULATION_POLL_US       UINT64_C(50000)

static inline int moonlight_stream_disconnect_requested(uint32_t buttons)
{
    const uint32_t mask = MOONLIGHT_PS5_PAD_SELECT |
                          MOONLIGHT_PS5_PAD_L1;
    return (buttons & mask) == mask;
}

static inline int moonlight_stream_hud_toggle_requested(uint32_t buttons)
{
    const uint32_t mask = MOONLIGHT_PS5_PAD_SELECT |
                          MOONLIGHT_PS5_PAD_R1;
    return (buttons & mask) == mask;
}

static inline int moonlight_stream_mouse_toggle_released(
    uint32_t previous_buttons, uint32_t buttons, uint64_t pressed_at_us,
    uint64_t now_us)
{
    return (previous_buttons & MOONLIGHT_PS5_PAD_OPTIONS) != 0 &&
           (buttons & MOONLIGHT_PS5_PAD_OPTIONS) == 0 &&
           now_us > pressed_at_us &&
           now_us - pressed_at_us >
               MOONLIGHT_MOUSE_EMULATION_LONG_PRESS_US;
}

static inline int16_t moonlight_stream_mouse_axis_delta(int16_t axis)
{
    float scaled = (float)axis / 32766.0f * 4.0f;
    float delta = scaled * scaled * scaled;

    if (delta > 2.0f)
        delta -= 2.0f;
    else if (delta < -2.0f)
        delta -= 2.0f;
    else
        delta = 0.0f;
    return (int16_t)delta;
}

#endif

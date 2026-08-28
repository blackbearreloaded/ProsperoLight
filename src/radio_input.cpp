/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "radio_input.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INPUT_QUEUE_SIZE 64U
#define PAD_SAMPLE_SIZE 120U
#define PAD_SAMPLE_CAPACITY 64
#define PAD_BUTTON_INTERCEPTED UINT32_C(0x80000000)
#define STICK_LOW 64U
#define STICK_HIGH 192U
#define STICK_REPEAT_DELAY_MS UINT64_C(350)
#define STICK_REPEAT_MS UINT64_C(110)

typedef struct
{
    uint32_t button;
    radio_input_key_t key;
} button_map_t;

extern "C"
{
    int scePadInit(void);
    extern int scePadOpen(int32_t user_id, int32_t port_type, int32_t index, const void *param);
    extern int scePadClose(int32_t handle);
    extern int scePadRead(int32_t handle, void *data, int32_t num);
    extern int sceUserServiceInitialize(void *init_params);
    extern int sceUserServiceGetInitialUser(int32_t *user_id);
    extern int sceUserServiceTerminate(void);
    uint64_t SDL_GetTicks64(void);
}

static const button_map_t buttons[] = {
    {UINT32_C(0x00004000), RADIO_INPUT_CROSS},   {UINT32_C(0x00002000), RADIO_INPUT_CIRCLE},
    {UINT32_C(0x00008000), RADIO_INPUT_SQUARE},  {UINT32_C(0x00001000), RADIO_INPUT_TRIANGLE},
    {UINT32_C(0x00000008), RADIO_INPUT_OPTIONS}, {UINT32_C(0x00000400), RADIO_INPUT_L1},
    {UINT32_C(0x00000800), RADIO_INPUT_R1},      {UINT32_C(0x00000010), RADIO_INPUT_UP},
    {UINT32_C(0x00000040), RADIO_INPUT_DOWN},    {UINT32_C(0x00000080), RADIO_INPUT_LEFT},
    {UINT32_C(0x00000020), RADIO_INPUT_RIGHT},
};

static radio_input_event_t queue[INPUT_QUEUE_SIZE];
static unsigned char samples[PAD_SAMPLE_CAPACITY][PAD_SAMPLE_SIZE];
static unsigned queue_read;
static unsigned queue_write;
static uint32_t button_state;
static int analog_key = -1;
static uint64_t analog_repeat_at;
static int32_t pad_handle = -1;
static bool owns_user_service;

static uint64_t monotonic_milliseconds(void)
{
    return SDL_GetTicks64();
}

static int stick_direction(uint8_t x, uint8_t y)
{
    const int horizontal = (int)x - 128;
    const int vertical = (int)y - 128;
    const int horizontal_size = horizontal < 0 ? -horizontal : horizontal;
    const int vertical_size = vertical < 0 ? -vertical : vertical;
    if (horizontal_size < 64 && vertical_size < 64)
        return -1;
    if (horizontal_size > vertical_size)
        return x < STICK_LOW ? RADIO_INPUT_LEFT : x > STICK_HIGH ? RADIO_INPUT_RIGHT : -1;
    return y < STICK_LOW ? RADIO_INPUT_UP : y > STICK_HIGH ? RADIO_INPUT_DOWN : -1;
}

static void queue_push(radio_input_key_t key, bool pressed)
{
    const unsigned next = (queue_write + 1U) % INPUT_QUEUE_SIZE;
    if (next == queue_read)
    {
        /* ponytail: bounded input; discard the oldest event instead of allocating. */
        queue_read = (queue_read + 1U) % INPUT_QUEUE_SIZE;
    }
    queue[queue_write] = (radio_input_event_t){key, pressed};
    queue_write = next;
}

static void process_sample(const unsigned char *sample)
{
    uint32_t current;
    memcpy(&current, sample, sizeof(current));
    const bool neutral = sample[76] == 0 || (current & PAD_BUTTON_INTERCEPTED) != 0;
    if (neutral)
        current = 0;

    const uint32_t changed = button_state ^ current;
    for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        if ((changed & buttons[i].button) != 0)
        {
            queue_push(buttons[i].key, (current & buttons[i].button) != 0);
        }
    }
    button_state = current;

    int current_analog = neutral ? -1 : stick_direction(sample[4], sample[5]);
    if (current_analog != analog_key)
    {
        if (analog_key >= 0)
            queue_push((radio_input_key_t)analog_key, false);
        analog_key = current_analog;
        if (analog_key >= 0)
        {
            queue_push((radio_input_key_t)analog_key, true);
            analog_repeat_at = monotonic_milliseconds() + STICK_REPEAT_DELAY_MS;
        }
    }
}

bool radio_input_init(void)
{
    const int user_init = sceUserServiceInitialize(NULL);
    owns_user_service = user_init == 0;

    int32_t user_id = -1;
    if (sceUserServiceGetInitialUser(&user_id) < 0 || scePadInit() < 0)
    {
        radio_input_shutdown();
        return false;
    }
    pad_handle = scePadOpen(user_id, 0, 0, NULL);
    if (pad_handle < 0)
    {
        radio_input_shutdown();
        return false;
    }
    queue_read = queue_write = 0;
    button_state = 0;
    analog_key = -1;
    analog_repeat_at = 0;
    return true;
}

void radio_input_poll(void)
{
    if (pad_handle < 0)
        return;
    const int count = scePadRead(pad_handle, samples, PAD_SAMPLE_CAPACITY);
    for (int i = 0; i < count; ++i)
        process_sample(samples[i]);
    if (analog_key >= 0)
    {
        const uint64_t now = monotonic_milliseconds();
        if (now >= analog_repeat_at)
        {
            queue_push((radio_input_key_t)analog_key, true);
            analog_repeat_at = now + STICK_REPEAT_MS;
        }
    }
}

bool radio_input_next(radio_input_event_t *event)
{
    if (event == NULL || queue_read == queue_write)
        return false;
    *event = queue[queue_read];
    queue_read = (queue_read + 1U) % INPUT_QUEUE_SIZE;
    return true;
}

bool radio_input_pressed(radio_input_key_t key)
{
    if (key < 0 || key >= RADIO_INPUT_COUNT)
        return false;
    for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        if (buttons[i].key == key)
            return (button_state & buttons[i].button) != 0;
    }
    return false;
}

void radio_input_shutdown(void)
{
    if (pad_handle >= 0)
    {
        scePadClose(pad_handle);
        pad_handle = -1;
    }
    if (owns_user_service)
    {
        sceUserServiceTerminate();
        owns_user_service = false;
    }
    queue_read = queue_write = 0;
    button_state = 0;
    analog_key = -1;
    analog_repeat_at = 0;
}

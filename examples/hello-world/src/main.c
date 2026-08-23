/*
 * ps5-native-app-boilerplate - Native CPU-rendered Hello World.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Renders bitmap text and geometric figures into two tiled VideoOut buffers.
 * The process stays alive until the shell closes it.
 */

#include <stddef.h>
#include <stdint.h>

#define FRAME_WIDTH 1920u
#define FRAME_HEIGHT 1080u
#define FRAME_BYTES 0x1000000u
#define MEMORY_BYTES (FRAME_BYTES * 2u)
#define MEMORY_ALIGNMENT 0x200000u
#define MEMORY_TYPE_WC_GARLIC 3
#define MAP_PROTECTION 0x33
#define PIXEL_FORMAT_RGBA8_SRGB UINT64_C(0x8000000022000000)

#define COLOR_BACKGROUND UINT32_C(0xff190d0a)
#define COLOR_PANEL UINT32_C(0xff301f17)
#define COLOR_WHITE UINT32_C(0xffffffff)
#define COLOR_CYAN UINT32_C(0xffffff00)
#define COLOR_MAGENTA UINT32_C(0xffff00ff)
#define COLOR_YELLOW UINT32_C(0xff00ffff)

typedef struct video_buffer {
    void *data;
    void *metadata;
    void *reserved0;
    void *reserved1;
} video_buffer_t;

typedef struct video_attribute {
    uint8_t reserved[80];
} video_attribute_t;

typedef struct notification_request {
    uint8_t reserved[45];
    char message[3075];
} notification_request_t;

typedef struct glyph {
    char character;
    uint8_t rows[7];
} glyph_t;

size_t sceKernelGetDirectMemorySize(void);
int sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end,
                                  size_t length, size_t alignment,
                                  int memory_type, int64_t *physical_address);
int sceKernelMapDirectMemory(void **address, size_t length, int protection,
                             int flags, int64_t physical_address,
                             size_t alignment);
int sceKernelSendNotificationRequest(uint32_t device, void *request,
                                     size_t size, int blocking);
int sceKernelUsleep(uint32_t microseconds);
int sceSystemServiceHideSplashScreen(void);
int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index,
                    const void *param);
int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
void sceVideoOutSetBufferAttribute2(video_attribute_t *attribute,
                                    uint64_t pixel_format,
                                    uint32_t tiling_mode, uint32_t width,
                                    uint32_t height, uint64_t option,
                                    uint32_t dcc_control,
                                    uint64_t dcc_clear_color);
int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set_index,
                                int32_t buffer_index_start,
                                video_buffer_t *buffers, int32_t buffer_count,
                                video_attribute_t *attribute, int32_t category,
                                void *option);
int sceVideoOutSubmitFlip(int32_t handle, int32_t buffer_index,
                          uint32_t flip_mode, int64_t flip_argument);
int sceVideoOutWaitVblank(int32_t handle);

static notification_request_t notification;

static const glyph_t glyphs[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 14}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {31, 4, 4, 4, 4, 4, 31}},
    {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
};

static void copy_message(char *destination, size_t capacity,
                         const char *source)
{
    size_t index = 0;

    if (capacity == 0)
        return;
    while (source[index] != '\0' && index + 1 < capacity) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void notify(const char *message)
{
    copy_message(notification.message, sizeof(notification.message), message);
    sceKernelSendNotificationRequest(0, &notification,
                                     sizeof(notification), 0);
}

static _Noreturn void halt(const char *message)
{
    notify(message);
    for (;;)
        sceKernelUsleep(1000000);
}

static const uint8_t *glyph_rows(char character)
{
    size_t count = sizeof(glyphs) / sizeof(glyphs[0]);

    for (size_t index = 0; index < count; ++index) {
        if (glyphs[index].character == character)
            return glyphs[index].rows;
    }
    return glyphs[0].rows;
}

static size_t tiled_byte_offset(unsigned x, unsigned y)
{
    uint32_t offset = ((y << 4) & 0x70u) ^
                      ((y << 5) & 0xf00u) ^
                      ((y << 9) & 0x1000u) ^
                      ((y << 8) & 0x4000u) ^
                      ((x << 2) & 0xcu) ^
                      ((x << 5) & 0x380u) ^
                      ((x << 4) & 0x400u) ^
                      ((x << 6) & 0x800u) ^
                      ((x << 9) & 0xa000u);
    uint32_t blocks_per_row = (FRAME_WIDTH + 127u) >> 7;
    uint32_t block_index = (y >> 7) * blocks_per_row + (x >> 7);

    return ((size_t)block_index << 16) + offset;
}

static void put_pixel(uint32_t *pixels, unsigned x, unsigned y,
                      uint32_t color)
{
    size_t offset = tiled_byte_offset(x, y);
    *(uint32_t *)((uint8_t *)pixels + offset) = color;
}

static void fill_rect(uint32_t *pixels, unsigned x, unsigned y,
                      unsigned width, unsigned height, uint32_t color)
{
    for (unsigned row = y; row < y + height; ++row) {
        for (unsigned column = x; column < x + width; ++column)
            put_pixel(pixels, column, row, color);
    }
}

static void fill_circle(uint32_t *pixels, unsigned center_x,
                        unsigned center_y, unsigned radius, uint32_t color)
{
    int signed_radius = (int)radius;

    for (int y = -signed_radius; y <= signed_radius; ++y) {
        for (int x = -signed_radius; x <= signed_radius; ++x) {
            if (x * x + y * y <= signed_radius * signed_radius) {
                put_pixel(pixels, center_x + (unsigned)x,
                          center_y + (unsigned)y, color);
            }
        }
    }
}

static void fill_triangle(uint32_t *pixels, unsigned center_x, unsigned top,
                          unsigned half_width, unsigned height,
                          uint32_t color)
{
    for (unsigned row = 0; row < height; ++row) {
        unsigned half = row * half_width / height;
        fill_rect(pixels, center_x - half, top + row, half * 2 + 1, 1,
                  color);
    }
}

static void draw_text(uint32_t *pixels, unsigned x, unsigned y,
                      const char *text, unsigned scale, uint32_t color)
{
    for (; *text != '\0'; ++text, x += 6 * scale) {
        const uint8_t *rows = glyph_rows(*text);
        for (unsigned row = 0; row < 7; ++row) {
            for (unsigned column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4 - column))) != 0) {
                    fill_rect(pixels, x + column * scale,
                              y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

static void flush_range(void *address, size_t length)
{
    uint8_t *at = address;
    uint8_t *end = at + length;

    for (; at < end; at += 64)
        __asm__ volatile("clflush (%0)" : : "r"(at) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}

static void render_frame(uint32_t *pixels)
{
    fill_rect(pixels, 0, 0, FRAME_WIDTH, FRAME_HEIGHT, COLOR_BACKGROUND);
    fill_rect(pixels, 120, 430, 500, 470, COLOR_PANEL);
    fill_rect(pixels, 710, 430, 500, 470, COLOR_PANEL);
    fill_rect(pixels, 1300, 430, 500, 470, COLOR_PANEL);
    fill_rect(pixels, 120, 375, 1680, 8, COLOR_WHITE);

    draw_text(pixels, 120, 90, "HELLO WORLD", 14, COLOR_WHITE);
    draw_text(pixels, 120, 245, "NATIVE PS5 CPU VIDEOOUT", 6, COLOR_WHITE);

    fill_circle(pixels, 370, 665, 130, COLOR_CYAN);
    fill_rect(pixels, 840, 535, 240, 240, COLOR_YELLOW);
    fill_triangle(pixels, 1550, 520, 170, 285, COLOR_MAGENTA);

    draw_text(pixels, 250, 830, "CIRCLE", 5, COLOR_WHITE);
    draw_text(pixels, 870, 830, "SQUARE", 5, COLOR_WHITE);
    draw_text(pixels, 1420, 830, "TRIANGLE", 5, COLOR_WHITE);
}

int main(void)
{
    sceSystemServiceHideSplashScreen();

    int video = sceVideoOutOpen(0xff, 0, 0, NULL);
    if (video < 0)
        halt("Hello World: sceVideoOutOpen failed");

    size_t pool_size = sceKernelGetDirectMemorySize();
    if (pool_size < MEMORY_BYTES)
        halt("Hello World: insufficient direct memory");

    int64_t physical_address = 0;
    int result = sceKernelAllocateDirectMemory(
        0, (int64_t)pool_size, MEMORY_BYTES, MEMORY_ALIGNMENT,
        MEMORY_TYPE_WC_GARLIC, &physical_address);
    if (result < 0)
        halt("Hello World: direct-memory allocation failed");

    void *mapped = NULL;
    result = sceKernelMapDirectMemory(&mapped, MEMORY_BYTES, MAP_PROTECTION, 0,
                                      physical_address, MEMORY_ALIGNMENT);
    if (result < 0)
        halt("Hello World: direct-memory mapping failed");

    render_frame(mapped);
    render_frame((uint32_t *)((uint8_t *)mapped + FRAME_BYTES));
    flush_range(mapped, MEMORY_BYTES);

    video_buffer_t buffers[2] = {
        {mapped, NULL, NULL, NULL},
        {(uint8_t *)mapped + FRAME_BYTES, NULL, NULL, NULL},
    };
    video_attribute_t attribute = {0};
    sceVideoOutSetFlipRate(video, 0);
    sceVideoOutSetBufferAttribute2(
        &attribute, PIXEL_FORMAT_RGBA8_SRGB, 0, FRAME_WIDTH, FRAME_HEIGHT,
        0, 0, 0);

    result = sceVideoOutRegisterBuffers2(video, 0, 0, buffers, 2,
                                         &attribute, 0, NULL);
    if (result < 0)
        halt("Hello World: buffer registration failed");

    result = sceVideoOutSubmitFlip(video, 0, 1, 1);
    if (result < 0)
        halt("Hello World: initial flip failed");

    sceVideoOutWaitVblank(video);
    notify("Hello World: CPU frame submitted");

    /* Returning from main or calling exit crashes this launch context. */
    for (;;)
        sceKernelUsleep(1000000);
}

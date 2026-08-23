/*
 * ps5-native-app-boilerplate - Minimal native application example.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Displays a startup notification and keeps the process alive for normal
 * shell-mediated closure.
 */

#include <stddef.h>
#include <stdint.h>

int sceKernelSendNotificationRequest(uint32_t device, void *request,
                                     size_t size, int blocking);
int sceKernelUsleep(uint32_t microseconds);
int sceSystemServiceHideSplashScreen(void);

typedef struct notification_request {
    uint8_t reserved[45];
    char message[3075];
} notification_request_t;

static notification_request_t notification;

static void copy_message(char *destination, size_t capacity,
                         const char *source)
{
    size_t i = 0;

    if (capacity == 0)
        return;

    while (source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        ++i;
    }
    destination[i] = '\0';
}

int main(void)
{
    sceSystemServiceHideSplashScreen();
    copy_message(notification.message, sizeof(notification.message),
                 "Hello from ps5-native-app-boilerplate");
    sceKernelSendNotificationRequest(0, &notification,
                                     sizeof(notification), 0);

    /* Returning from main or calling exit crashes this launch context. */
    for (;;)
        sceKernelUsleep(1000000);
}

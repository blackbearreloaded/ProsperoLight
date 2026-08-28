/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <fcntl.h>
#include <pthread.h>

/*
 * The generic FreeBSD path in moonlight-common-c renames every new thread.
 * Calling the PS5 pthread_rename_np export from a newly started thread hangs
 * before its entry point runs, so thread names are intentionally best-effort.
 */
static inline int ps5_pthread_setname_noop(pthread_t thread, const char *name) {
    (void)thread;
    (void)name;
    return 0;
}
#define pthread_setname_np ps5_pthread_setname_noop

/* nanors has a portable path; CPU feature globals are unavailable here. */
#define __builtin_cpu_init() ((void)0)
#define __builtin_cpu_supports(feature) 0

/* PS5 network handles must be closed/configured through libSceNet. */
#ifndef PS5_SOCKET_ADAPTER_IMPLEMENTATION
#define close ps5_socket_close
#define fcntl ps5_socket_fcntl
#define ioctl ps5_socket_ioctl
#define poll ps5_socket_poll
#define select ps5_socket_select
int ps5_socket_close(int socket);
int ps5_socket_fcntl(int socket, int command, ...);
int ps5_socket_ioctl(int socket, unsigned long request, ...);
#endif

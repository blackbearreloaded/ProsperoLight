/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <fcntl.h>
#include <pthread.h>

/* The PS5 linker maps this POSIX spelling to pthread_rename_np. */
int pthread_setname_np(pthread_t thread, const char *name);

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

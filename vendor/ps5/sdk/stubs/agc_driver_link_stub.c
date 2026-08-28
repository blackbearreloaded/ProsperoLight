/*
 * ps5-native-app-boilerplate / ProsperoLight - AGC driver link stub.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host-link declarations for system libSceAgcDriver; never included in the app.
 */

#include <stdint.h>

int32_t sceAgcDriverSubmitDcb(void *description) { (void)description; return -1; }
uint32_t sceAgcDriverGetWaitRenderingPacketSizeInDwords(void) { return 0; }
uint32_t sceAgcDriverWaitUntilSafeForRendering(uint32_t **command, uint32_t packet_size, uint32_t reserved, uint32_t handle, int buffer) { (void)command; (void)packet_size; (void)reserved; (void)handle; (void)buffer; return 0; }

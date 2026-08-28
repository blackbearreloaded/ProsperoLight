/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAN_HTTP_REPORT_HPP
#define LAN_HTTP_REPORT_HPP

#ifdef __cplusplus
extern "C" {
#endif

int lan_http_report_text(const char *message);
void lan_http_report_set_host(const char *host);

#ifdef __cplusplus
}
#endif

#endif

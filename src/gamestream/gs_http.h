/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stddef.h>

#include "certgen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char *body;
        size_t length;
    } http_response_t;

    void http_init(client_identity_t *identity, int verbose);
    void http_set_timeout_ms(unsigned milliseconds);
    void http_interrupt(void);
    void http_clear_interrupt(void);
    int http_get(const char *host, unsigned short port, int use_tls, const char *path,
                 http_response_t *response);
    void http_response_free(http_response_t *response);

#ifdef __cplusplus
}
#endif

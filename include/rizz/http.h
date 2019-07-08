//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
// NOTE: Api is not multi-threaded. But I will make it thread-safe if it's needed
#pragma once

#include "types.h"

typedef enum rizz_http_status {
    RIZZ_HTTP_PENDING = 0,
    RIZZ_HTTP_COMPLETED,
    RIZZ_HTTP_FAILED
} rizz_http_status;

typedef struct rizz_http_state {
    rizz_http_status status;
    int status_code;
    char const* reason_phrase;
    char const* content_type;
    size_t response_size;
    void* response_data;
} rizz_http_state;

typedef void(rizz_http_cb)(const rizz_http_state* http, void* user);

typedef struct rizz_api_http {
    // normal requests: returns immediately (async sockets)
    // check `status_code` for retrieved http object to determine if it's finished or failed
    // call `free` if http data is not needed any more. if not freed by the user, it will be freed
    // when engine exits and throws a warning
    rizz_http (*get)(const char* url);
    rizz_http (*post)(const char* url, const void* data, size_t size);
    void (*free)(rizz_http handle);

    // return can be NULL (if it is not even started)
    const rizz_http_state* (*state)(rizz_http handle);

    // callback requests: triggers the callback when get/post is complete
    // NOTE: do not call `free` in the callback functions, the request will be freed automatically
    void (*get_cb)(const char* url, rizz_http_cb* callback, void* user);
    void (*post_cb)(const char* url, const void* data, size_t size, rizz_http_cb* callback,
                    void* user);
} rizz_api_http;

#ifdef RIZZ_INTERNAL_API
typedef struct sx_alloc sx_alloc;

RIZZ_API rizz_api_http the__http;

bool rizz__http_init(const sx_alloc* alloc);
void rizz__http_release();
void rizz__http_update();
#endif    // RIZZ_INTERNAL_API

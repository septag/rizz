//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "internal.h"

#include "sx/array.h"
#include "sx/handle.h"
#include "sx/string.h"

#define HTTP_IMPLEMENTATION
#define HTTP_MALLOC(ctx, size) (sx_malloc((const sx_alloc*)ctx, size))
#define HTTP_FREE(ctx, ptr) (sx_free((const sx_alloc*)ctx, ptr))
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-variable")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wsign-compare")    // clang/win: comparison of different signs
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)
#if SX_PLATFORM_ANDROID
#    include <linux/in.h>
#endif
#include "mattias/http.h"
SX_PRAGMA_DIAGNOSTIC_POP();

typedef enum { HTTP_MODE_NONE, HTTP_MODE_GET, HTTP_MODE_POST } rizz__http_mode;

typedef struct {
    char* url;
    rizz__http_mode mode;
    void* data;
    size_t size;
} rizz__http_request_params;

typedef struct {
    http_t* h;
    rizz__http_request_params* params;    // optional: only allocated for inactive objects
    rizz_http_cb* callback;
    void* callback_user;
} rizz__http;

typedef struct {
    const sx_alloc* alloc;
    sx_handle_pool* http_handles;    // rizz__http
    rizz__http* https;               // sx_array
    rizz_http pending[RIZZ_CONFIG_MAX_HTTP_REQUESTS];
    int num_pending;
} rizz__http_context;

static rizz__http_context g_http;

bool rizz__http_init(const sx_alloc* alloc)
{
    g_http.alloc = alloc;
    g_http.http_handles = sx_handle_create_pool(alloc, RIZZ_CONFIG_MAX_HTTP_REQUESTS);
    if (!g_http.http_handles)
        return false;

    return true;
}

void rizz__http_release()
{
    if (g_http.alloc) {
        // remove remaining http requests
        if (g_http.http_handles) {
            for (int i = 0; i < g_http.http_handles->count; i++) {
                sx_handle_t handle = sx_handle_at(g_http.http_handles, i);
                rizz__http* http = &g_http.https[sx_handle_index(handle)];
                if (http->h) {
                    rizz__log_warn("un-freed active http request: 0x%p", http->h);
                    http_release(http->h);
                } else if (http->params) {
                    rizz__log_warn("un-freed http request: %s", http->params->url);
                    sx_free(g_http.alloc, http->params);
                }
            }

            sx_handle_destroy_pool(g_http.http_handles, g_http.alloc);
        }

        sx_array_free(g_http.alloc, g_http.https);
        sx_memset(&g_http, 0x0, sizeof(g_http));
    }
}

void rizz__http_update()
{
    int remove_list[RIZZ_CONFIG_MAX_HTTP_REQUESTS];
    int num_removes = 0;

    // process pending
    // for callback requests, call the callback and release the request automatically
    for (int i = 0, c = g_http.num_pending; i < c; i++) {
        rizz_http handle = g_http.pending[i];
        rizz__http* http = &g_http.https[sx_handle_index(handle.id)];
        sx_assert(http->h);
        if (http->h->status != HTTP_STATUS_PENDING)
            continue;

        http_status_t status = http_process(http->h);
        if (status != HTTP_STATUS_PENDING && http->callback) {
            http->callback((const rizz_http_state*)http->h, http->callback_user);
            sx_assertf(http->h, "must not `free` inside callback");
            http_release(http->h);
            http->h = NULL;

            remove_list[num_removes++] = i;
        }
    }

    // remove finished requests
    // this applies to callbacks only, because non-callbacks should be freed by user manually
    for (int i = 0; i < num_removes; i++) {
        int index = remove_list[i];
        sx_handle_del(g_http.http_handles, g_http.pending[index].id);
        sx_swap(g_http.pending[index], g_http.pending[g_http.num_pending - 1], rizz_http);
        --g_http.num_pending;
    }

    // check if we have new requests to start
    while (g_http.num_pending < RIZZ_CONFIG_MAX_HTTP_REQUESTS) {
        // find an inactive handle in order to start new request
        rizz_http inactive_handle = { 0 };
        for (int i = 0, c = g_http.http_handles->count; i < c; i++) {
            rizz_http handle = (rizz_http){ .id = sx_handle_at(g_http.http_handles, i) };
            if (g_http.https[sx_handle_index(handle.id)].params) {
                inactive_handle = handle;
                break;
            }
        }

        if (!inactive_handle.id)
            break;

        rizz__http* http = &g_http.https[sx_handle_index(inactive_handle.id)];
        const rizz__http_request_params* params = http->params;
        switch (params->mode) {
        case HTTP_MODE_GET:
            http->h = http_get(params->url, (void*)g_http.alloc);
            break;

        case HTTP_MODE_POST:
            http->h = http_post(params->url, params->data, params->size, (void*)g_http.alloc);
            break;

        default:
            sx_assert(0);
            break;
        }
        sx_free(g_http.alloc, http->params);
        http->params = NULL;
        g_http.pending[g_http.num_pending++] = inactive_handle;
    }
}

static rizz__http_request_params* rizz__http_alloc_params(const char* url, rizz__http_mode mode,
                                                          const void* data, size_t size)
{
    int url_size = sx_strlen(url) + 1;
    size_t total_sz = sizeof(rizz__http_request_params) + url_size + size;
    uint8_t* buff = sx_malloc(g_http.alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return NULL;
    }

    rizz__http_request_params* p = (rizz__http_request_params*)buff;
    buff += sizeof(rizz__http_request_params);

    p->url = (char*)buff;
    buff += url_size;
    sx_memcpy(p->url, url, url_size);

    p->mode = mode;
    p->size = size;
    if (data) {
        p->data = buff;
        sx_memcpy(p->data, data, size);
    }

    return p;
}

static rizz_http rizz__http_get(const char* url)
{
    sx_assert(g_http.alloc);

    rizz_http handle =
        (rizz_http){ .id = sx_handle_new_and_grow(g_http.http_handles, g_http.alloc) };
    sx_assert(handle.id);
    rizz__http _http = (rizz__http){ 0 };

    if (g_http.num_pending < RIZZ_CONFIG_MAX_HTTP_REQUESTS) {
        _http.h = http_get(url, (void*)g_http.alloc);
        g_http.pending[g_http.num_pending++] = handle;
    } else {
        _http.params = rizz__http_alloc_params(url, HTTP_MODE_GET, NULL, 0);
    }

    int index = sx_handle_index(handle.id);
    if (index >= sx_array_count(g_http.https))
        sx_array_push(g_http.alloc, g_http.https, _http);
    else
        g_http.https[index] = _http;

    return handle;
}

static rizz_http rizz__http_post(const char* url, const void* data, size_t size)
{
    sx_assert(g_http.alloc);

    rizz_http handle =
        (rizz_http){ .id = sx_handle_new_and_grow(g_http.http_handles, g_http.alloc) };
    sx_assert(handle.id);
    rizz__http _http = (rizz__http){ 0 };

    if (g_http.num_pending < RIZZ_CONFIG_MAX_HTTP_REQUESTS) {
        _http.h = http_post(url, data, size, (void*)g_http.alloc);
        g_http.pending[g_http.num_pending++] = handle;
    } else {
        _http.params = rizz__http_alloc_params(url, HTTP_MODE_POST, data, size);
    }

    int index = sx_handle_index(handle.id);
    if (index >= sx_array_count(g_http.https))
        sx_array_push(g_http.alloc, g_http.https, _http);
    else
        g_http.https[index] = _http;

    return handle;
}

static void rizz__http_free(rizz_http handle)
{
    sx_assert(g_http.alloc);

    sx_assert(handle.id);
    rizz__http* http = &g_http.https[sx_handle_index(handle.id)];
    sx_assertf(http->h, "double free?");
    http_release(http->h);
    http->h = NULL;

    if (http->params) {
        sx_free(g_http.alloc, http->params);
        http->params = NULL;
    }

    sx_handle_del(g_http.http_handles, handle.id);

    // remove from pending
    for (int i = 0, c = g_http.num_pending; i < c; i++) {
        if (g_http.pending[i].id == handle.id) {
            sx_swap(g_http.pending[i], g_http.pending[c - 1], rizz_http);
            --g_http.num_pending;
            break;
        }
    }
}

static const rizz_http_state* rizz__http_state(rizz_http handle)
{
    sx_assert(g_http.alloc);
    sx_assert(handle.id);

    return (const rizz_http_state*)g_http.https[sx_handle_index(handle.id)].h;
}

static void rizz__http_get_cb(const char* url, rizz_http_cb* callback, void* user)
{
    sx_assert(g_http.alloc);

    rizz_http handle =
        (rizz_http){ .id = sx_handle_new_and_grow(g_http.http_handles, g_http.alloc) };
    sx_assert(handle.id);
    rizz__http _http = (rizz__http){ .callback = callback, .callback_user = user };

    if (g_http.num_pending < RIZZ_CONFIG_MAX_HTTP_REQUESTS) {
        _http.h = http_get(url, (void*)g_http.alloc);
        g_http.pending[g_http.num_pending++] = handle;
    } else {
        _http.params = rizz__http_alloc_params(url, HTTP_MODE_GET, NULL, 0);
    }

    int index = sx_handle_index(handle.id);
    if (index >= sx_array_count(g_http.https))
        sx_array_push(g_http.alloc, g_http.https, _http);
    else
        g_http.https[index] = _http;
}

static void rizz__http_post_cb(const char* url, const void* data, size_t size,
                               rizz_http_cb* callback, void* user)
{
    sx_assert(g_http.alloc);

    rizz_http handle =
        (rizz_http){ .id = sx_handle_new_and_grow(g_http.http_handles, g_http.alloc) };
    sx_assert(handle.id);
    rizz__http _http = (rizz__http){ .callback = callback, .callback_user = user };

    if (g_http.num_pending < RIZZ_CONFIG_MAX_HTTP_REQUESTS) {
        _http.h = http_post(url, data, size, (void*)g_http.alloc);
        g_http.pending[g_http.num_pending++] = handle;
    } else {
        _http.params = rizz__http_alloc_params(url, HTTP_MODE_POST, data, size);
    }

    int index = sx_handle_index(handle.id);
    if (index >= sx_array_count(g_http.https))
        sx_array_push(g_http.alloc, g_http.https, _http);
    else
        g_http.https[index] = _http;
}

rizz_api_http the__http = { .get = rizz__http_get,
                            .post = rizz__http_post,
                            .free = rizz__http_free,
                            .state = rizz__http_state,
                            .get_cb = rizz__http_get_cb,
                            .post_cb = rizz__http_post_cb };

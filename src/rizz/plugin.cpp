//
// Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "config.h"
#include "rizz/plugin.h"
#include "rizz/core.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/os.h"
#include "sx/string.h"

#ifndef RIZZ_BUNDLE
#    define CR_MAIN_FUNC "rizz_plugin_main"
#    define CR_EVENT_FUNC "rizz_plugin_event_handler"
#    if CR_DEBUG
#        define CR_LOG(...) the__core.print_debug(__VA_ARGS__)
#    else
#        define CR_LOG(...)
#    endif
#    define CR_ERROR(...) the__core.print_error(__VA_ARGS__)
#    define CR_HOST CR_SAFEST

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#    include "../../3rdparty/cr/cr.h"
SX_PRAGMA_DIAGNOSTIC_POP()
#else
#    include "plugin_bundle.h"
#    include "plugin_bundle_native.h"

typedef rizz_plugin cr_plugin;
#    define CR_OTHER RIZZ_PLUGIN_CRASH_OTHER
#endif

// Keep the list of all APIs (and their headers! ouch)
#include "rizz/app.h"
#include "rizz/asset.h"
#include "rizz/camera.h"
#include "rizz/graphics.h"
#include "rizz/http.h"
#include "rizz/reflect.h"
#include "rizz/vfs.h"
static void* g_native_apis[_RIZZ_API_NATIVE_COUNT] = { &the__core,  &the__plugin, &the__app,
                                                       &the__gfx,   &the__refl,   &the__vfs,
                                                       &the__asset, &the__camera, &the__http };

struct rizz__plugin_injected_api {
    rizz_api_type type;
    uint32_t      version;
    void*         api;
};

struct rizz__plugin_item {
    cr_plugin        p;
    rizz_plugin_info info;
    int              order;
    char             filename[32];
    float            update_tm;
    bool             manual_reload;
};

struct rizz__plugin_mgr {
    const sx_alloc*            alloc = nullptr;
    rizz__plugin_item*         plugins = nullptr;
    int*                       plugin_update_order = nullptr;    // indices to 'plugins' for updates
    char                       plugin_path[256] = { 0 };
    rizz__plugin_injected_api* injected = nullptr;
};

static rizz__plugin_mgr g_plugin{};

#define SORT_NAME rizz__plugin
#define SORT_TYPE int
#define SORT_CMP(x, y) (g_plugin.plugins[x].order - g_plugin.plugins[y].order)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

bool rizz__plugin_init(const sx_alloc* alloc, const char* plugin_path) {
    static_assert(RIZZ_PLUGIN_CRASH_OTHER == (rizz_plugin_crash)CR_OTHER, "crash enum mismatch");
    sx_assert(alloc);
    g_plugin.alloc = alloc;

    if (plugin_path && plugin_path[0]) {
        sx_strcpy(g_plugin.plugin_path, sizeof(g_plugin.plugin_path), plugin_path);
        sx_os_path_normpath(g_plugin.plugin_path, sizeof(g_plugin.plugin_path), plugin_path);
        if (!sx_os_path_isdir(g_plugin.plugin_path)) {
            rizz_log_error("Plugin path '%s' is incorrect", g_plugin.plugin_path);
            return false;
        }
    } else {
#if SX_PLATFORM_LINUX
        sx_strcpy(g_plugin.plugin_path, sizeof(g_plugin.plugin_path), "./");
#endif
    }

#if RIZZ_BUNDLE
    rizz__plugin_bundle_native();
    rizz__plugin_bundle();
#endif

    return true;
}

void* rizz__plugin_get_api(rizz_api_type api, uint32_t version) {
    sx_unused(version);

    if (api < _RIZZ_API_NATIVE_COUNT) {
        return g_native_apis[api];
    } else {
        sx_assert(api > _RIZZ_API_NATIVE_COUNT && api < _RIZZ_API_COUNT);
        for (int i = 0, c = sx_array_count(g_plugin.injected); i < c; i++) {
            if (g_plugin.injected[i].type == api)
                return g_plugin.injected[i].api;
        }
        return nullptr;    // plugin not found/injected
    }
}

void rizz__plugin_release() {
    if (!g_plugin.alloc)
        return;

    if (g_plugin.plugins) {
#ifndef RIZZ_BUNDLE
        for (int i = 0; i < sx_array_count(g_plugin.plugins); i++)
            cr_plugin_close(g_plugin.plugins[i].p);
#else
        for (int i = 0; i < sx_array_count(g_plugin.plugins); i++) {
            sx_assert(g_plugin.plugins[i].info.main_cb);
            g_plugin.plugins[i].info.main_cb((rizz_plugin*)&g_plugin.plugins[i].p,
                                             RIZZ_PLUGIN_EVENT_SHUTDOWN);
        }
#endif
        sx_array_free(g_plugin.alloc, g_plugin.plugins);
    }

    sx_array_free(g_plugin.alloc, g_plugin.injected);
    sx_array_free(g_plugin.alloc, g_plugin.plugin_update_order);

    sx_memset(&g_plugin, 0x0, sizeof(g_plugin));
}

#ifdef RIZZ_BUNDLE
static void rizz__plugin_register(const rizz_plugin_info* info) {
    rizz__plugin_item item;
    sx_memset(&item, 0x0, sizeof(item));
    item.p.api = &the__plugin;
    item.p.iteration = 1;
    item.manual_reload = true;
    sx_memcpy(&item.info, info, sizeof(*info));
    sx_strcpy(item.filename, sizeof(item.filename), info->name);

    sx_array_push(g_plugin.alloc, g_plugin.plugins, item);
    sx_array_push(g_plugin.alloc, g_plugin.plugin_update_order,
                  sx_array_count(g_plugin.plugins) - 1);
}

static bool rizz__plugin_load(const char* plugin_name, rizz_plugin_flags plugin_flags) {
    // find the plugin and save it's flags, the list is already populated
    int plugin_id = -1;
    for (int i = 0; i < sx_array_count(g_plugin.plugins); i++) {
        if (sx_strequal(g_plugin.plugins[i].filename, plugin_name)) {
            plugin_id = i;
            break;
        }
    }

    if (plugin_id != -1) {
        rizz__plugin_item* item = &g_plugin.plugins[plugin_id];

        // load the plugin
        int r;
        if ((r = item->info.main_cb((rizz_plugin*)&item->p, RIZZ_PLUGIN_EVENT_INIT)) != 0) {
            rizz_log_error("plugin load failed: %s, returned: %d", item->info.name, r);
            return false;
        }

        if (plugin_flags & RIZZ_PLUGIN_FLAG_UPDATE_FIRST)
            item->order = -1;
        else if (plugin_flags & RIZZ_PLUGIN_FLAG_UPDATE_LAST)
            item->order = 1;
        rizz__plugin_binary_insertion_sort(g_plugin.plugin_update_order,
                                           sx_array_count(g_plugin.plugin_update_order));

        int version = item->info.version;
        rizz_log_info("(init) plugin: %s - %s - v%d.%d.%d", item->info.name, item->info.desc,
                      (version / 100), (version / 10) % 10, version % 100);

        item->p._p = (void*)0x1;    // set it to something that indicates plugin is loaded
        return true;
    } else {
        rizz_log_error("plugin load failed: %s", plugin_name);
        return false;
    }
}

bool rizz__plugin_load_abs(const char* filepath, rizz_plugin_flags plugin_flags) {
    sx_unused(plugin_flags);
    sx_assert(sx_strstr(filepath, GAME_PLUGIN_NAME) &&
              "only game plugin should be loaded by 'load_abs' in BUNDLE build");
    return rizz__plugin_load(GAME_PLUGIN_NAME, 0);
}

static void rizz__plugin_unload(const char* plugin_name) {
    int index = -1;
    for (int i = 0, c = sx_array_count(g_plugin.plugins); i < c; i++) {
        if (sx_strequal(g_plugin.plugins[i].info.name, plugin_name)) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        rizz__plugin_item* item = &g_plugin.plugins[index];
        sx_assert(item->info.main_cb);
        item->info.main_cb((rizz_plugin*)&item->p, RIZZ_PLUGIN_EVENT_SHUTDOWN);
        item->p._p = NULL;
    }
}

void rizz__plugin_update(float dt) {
    sx_unused(dt);
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        rizz__plugin_item* item = &g_plugin.plugins[i];
        if (item->p._p == (void*)0x1) {
            sx_assert(item->info.main_cb);
            item->info.main_cb((rizz_plugin*)&item->p, RIZZ_PLUGIN_EVENT_STEP);
        }
    }
}

void rizz__plugin_broadcast_event(const rizz_app_event* e) {
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        rizz__plugin_item* item = &g_plugin.plugins[i];
        if (item->p._p == (void*)0x1 && item->info.flags & RIZZ_PLUGIN_INFO_EVENT_HANDLER) {
            sx_assert(item->info.event_cb);
            item->info.event_cb(e);
        }
    }
}
#else    // RIZZ_BUNDLE

static void rizz__plugin_reload_handler(cr_plugin* plugin, const char* filename, const void** ptrs,
                                        const void** new_ptrs, int num_ptrs) {
    sx_unused(filename);
    sx_unused(plugin);

    for (int i = 0; i < num_ptrs; i++) {
        rizz__core_fix_callback_ptrs(ptrs, new_ptrs, num_ptrs);
    }
}

static bool rizz__plugin_load(const char* plugin_name, rizz_plugin_flags plugin_flags) {
    // construct full filepath, by joining to root plugin path and adding extension
    char filepath[256];
#    if SX_PLATFORM_LINUX || SX_PLATFORM_OSX
    sx_os_path_join(filepath, sizeof(filepath), g_plugin.plugin_path, "lib");
    sx_strcat(filepath, sizeof(filepath), plugin_name);
#    else
    sx_os_path_join(filepath, sizeof(filepath), g_plugin.plugin_path, plugin_name);
#    endif
    sx_strcat(filepath, sizeof(filepath), SX_DLL_EXT);
    return rizz__plugin_load_abs(filepath, plugin_flags);
}

bool rizz__plugin_load_abs(const char* filepath, rizz_plugin_flags plugin_flags) {
    rizz__plugin_item item;
    sx_memset(&item, 0x0, sizeof(item));
    item.p.userdata = &the__plugin;

    char plugin_file[128];
    sx_os_path_basename(plugin_file, sizeof(plugin_file), filepath);

    // We got the info, the plugin seems to be valid, now load and run the plugin
    item.manual_reload =
        sx_cppbool(!RIZZ_CONFIG_HOT_LOADING || (plugin_flags & RIZZ_PLUGIN_FLAG_MANUAL_RELOAD));
    if (!cr_plugin_load(item.p, filepath, rizz__plugin_reload_handler)) {
        rizz_log_error("plugin load failed: %s", filepath);
        return false;
    }

    rizz_plugin_get_info_cb* get_info =
        (rizz_plugin_get_info_cb*)cr_plugin_symbol(item.p, "rizz_plugin_get_info");
    if (!get_info) {
        rizz_log_warn("plugin missing rizz_plugin_get_info symbol: %s", filepath);
        char ext[32];
        sx_os_path_splitext(ext, sizeof(ext), item.info.name, sizeof(item.info.name), plugin_file);
    } else {
        get_info(&item.info);
    }

    sx_strcpy(item.filename, sizeof(item.filename), plugin_file);
    if (plugin_flags & RIZZ_PLUGIN_FLAG_UPDATE_FIRST)
        item.order = -1;
    else if (plugin_flags & RIZZ_PLUGIN_FLAG_UPDATE_LAST)
        item.order = 1;

    sx_array_push(g_plugin.alloc, g_plugin.plugins, item);
    sx_array_push(g_plugin.alloc, g_plugin.plugin_update_order,
                  sx_array_count(g_plugin.plugins) - 1);
    rizz__plugin_binary_insertion_sort(g_plugin.plugin_update_order,
                                       sx_array_count(g_plugin.plugin_update_order));

    int version = item.info.version;
    rizz_log_info("(init) plugin: %s (%s) - %s - v%d.%d.%d", item.info.name, item.filename,
                  item.info.desc, rizz_version_major(version), rizz_version_minor(version),
                  rizz_version_bugfix(version));
    return true;
}

static void rizz__plugin_unload(const char* plugin_name) {
    int index = -1;
    for (int i = 0, c = sx_array_count(g_plugin.plugins); i < c; i++) {
        if (sx_strequal(g_plugin.plugins[i].info.name, plugin_name)) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        cr_plugin_close(g_plugin.plugins[index].p);
        sx_array_pop(g_plugin.plugins, index);
        // reorder
        sx_array_pop_last(g_plugin.plugin_update_order);
        for (int i = 0; i < sx_array_count(g_plugin.plugin_update_order); i++) {
            g_plugin.plugin_update_order[i] = i;
        }
        rizz__plugin_binary_insertion_sort(g_plugin.plugin_update_order,
                                           sx_array_count(g_plugin.plugin_update_order));
    }
}

void rizz__plugin_update(float dt) {
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        rizz__plugin_item* plugin = &g_plugin.plugins[g_plugin.plugin_update_order[i]];
        bool               check_reload = false;
        if (!plugin->manual_reload) {
            plugin->update_tm += dt;
            if (plugin->update_tm >= RIZZ_CONFIG_PLUGIN_UPDATE_INTERVAL) {
                check_reload = true;
                plugin->update_tm = 0;
            }
        }

        int r = cr_plugin_update(plugin->p, check_reload);
        if (r == -2) {
            rizz_log_error("plugin '%s' failed to reload", g_plugin.plugins[i].info.name);
        } else if (r < -1) {
            if (plugin->p.failure == CR_USER) {
                rizz_log_error("plugin '%s' failed (main ret = -1)", g_plugin.plugins[i].info.name);
            } else {
                rizz_log_error("plugin '%s' crashed", g_plugin.plugins[i].info.name);
            }
        }
    }
}

void rizz__plugin_broadcast_event(const rizz_app_event* e) {
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        if (g_plugin.plugins[i].info.flags & RIZZ_PLUGIN_INFO_EVENT_HANDLER)
            cr_plugin_event(g_plugin.plugins[g_plugin.plugin_update_order[i]].p, e);
    }
}
#endif    // RIZZ_BUNDLE

void rizz__plugin_inject_api(rizz_api_type type, uint32_t version, void* api) {
    // the plugin must not exist with the same attributes
    for (int i = 0, c = sx_array_count(g_plugin.injected); i < c; i++) {
        if (g_plugin.injected[i].type == type && g_plugin.injected[i].version == version) {
            rizz_log_warn("plugin (type_id='%d', version=%d) already exists", type, version);
            sx_assert(0);
            return;
        }
    }

    rizz__plugin_injected_api item = { type, version, api };
    sx_array_push(g_plugin.alloc, g_plugin.injected, item);
}

void rizz__plugin_remove_api(rizz_api_type type, uint32_t version) {
    for (int i = 0, c = sx_array_count(g_plugin.injected); i < c; i++) {
        if (g_plugin.injected[i].type == type && g_plugin.injected[i].version == version) {
            sx_array_pop(g_plugin.injected, i);
            return;
        }
    }
    rizz_log_warn("plugin (type_id='%d', version=%d) not found", type, version);
}

const char* rizz__plugin_crash_reason(rizz_plugin_crash crash) {
    // clang-format off
    switch (crash) {
    case RIZZ_PLUGIN_CRASH_NONE:             return "None";
    case RIZZ_PLUGIN_CRASH_SEGFAULT:         return "EXCEPTION_ACCESS_VIOLATION";
    case RIZZ_PLUGIN_CRASH_ILLEGAL:          return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case RIZZ_PLUGIN_CRASH_ABORT:            return "abort()";
    case RIZZ_PLUGIN_CRASH_MISALIGN:         return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case RIZZ_PLUGIN_CRASH_BOUNDS:           return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case RIZZ_PLUGIN_CRASH_STACKOVERFLOW:    return "EXCEPTION_STACK_OVERFLOW";
    case RIZZ_PLUGIN_CRASH_STATE_INVALIDATED:return "Global data safety error";
    case RIZZ_PLUGIN_CRASH_USER:             return "Returned -1";
    case RIZZ_PLUGIN_CRASH_OTHER:            return "Other";
    default:                                return "Unknown";
    }
    // clang-format on
}

rizz_api_plugin the__plugin = { rizz__plugin_load,       rizz__plugin_unload,
                                rizz__plugin_inject_api, rizz__plugin_remove_api,
                                rizz__plugin_get_api,    rizz__plugin_crash_reason };

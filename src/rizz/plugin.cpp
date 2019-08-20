//
// Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "config.h"

#include "rizz/core.h"
#include "rizz/plugin.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/os.h"
#include "sx/string.h"

#ifndef RIZZ_BUNDLE
#    define CR_MAIN_FUNC "rizz_plugin_main"
#    define CR_EVENT_FUNC "rizz_plugin_event_handler"
//#define CR_DEBUG 1
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
#    define PLUGIN_SOURCE
#    include "plugin_bundle.h"

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
static void* g_native_apis[_RIZZ_API_COUNT] = { &the__core,  &the__plugin, &the__app,
                                                &the__gfx,   &the__refl,   &the__vfs,
                                                &the__asset, &the__camera, &the__http };

struct rizz__plugin_injected_api {
    char name[32];
    uint32_t version;
    void* api;
};

struct rizz__plugin_dependency {
    char name[32];
};

struct rizz__plugin_item {
    cr_plugin p;
    rizz_plugin_info info;
    int order;
    char filepath[RIZZ_MAX_PATH];
    float update_tm;
    rizz__plugin_dependency* deps;
    int num_deps;
};

struct rizz__plugin_mgr {
    const sx_alloc* alloc = nullptr;
    rizz__plugin_item* plugins = nullptr;
    int* plugin_update_order = nullptr;    // indices to 'plugins' array
    char plugin_path[256] = { 0 };
    rizz__plugin_injected_api* injected = nullptr;
    bool loaded;
};

static rizz__plugin_mgr g_plugin;

#define SORT_NAME rizz__plugin
#define SORT_TYPE int
#define SORT_CMP(x, y) (g_plugin.plugins[x].order - g_plugin.plugins[y].order)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#include "sort/sort.h"

bool rizz__plugin_init(const sx_alloc* alloc, const char* plugin_path)
{
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
#if SX_PLATFORM_LINUX || SX_PLATFORM_RPI
        sx_strcpy(g_plugin.plugin_path, sizeof(g_plugin.plugin_path), "./");
#endif
    }

#if RIZZ_BUNDLE
    rizz__plugin_bundle();
#endif

    return true;
}

void* rizz__plugin_get_api(rizz_api_type api, uint32_t version)
{
    sx_unused(version);
    sx_assert(api < _RIZZ_API_COUNT);

    return g_native_apis[api];
}

void* rizz__plugin_get_api_byname(const char* name, uint32_t version)
{
    sx_unused(version);
    for (int i = 0, c = sx_array_count(g_plugin.injected); i < c; i++) {
        if (sx_strequal(name, g_plugin.injected[i].name) &&
            g_plugin.injected[i].version == version) {
            return g_plugin.injected[i].api;
        }
    }

    rizz_log_warn("API '%s' version '%d' not found", name, version);
    return NULL;
}

void rizz__plugin_release()
{
    if (!g_plugin.alloc)
        return;

    if (g_plugin.plugins) {
        // unload plugins in reverse order of loading
        // so game module will be unloaded first
#ifndef RIZZ_BUNDLE
        for (int i = sx_array_count(g_plugin.plugin_update_order) - 1; i >= 0; i--) {
            int index = g_plugin.plugin_update_order[i];
            cr_plugin_close(g_plugin.plugins[index].p);
        }
#else
        for (int i = sx_array_count(g_plugin.plugin_update_order) - 1; i >= 0; i--) {
            int index = g_plugin.plugin_update_order[i];
            sx_assert(g_plugin.plugins[index].info.main_cb);
            g_plugin.plugins[index].info.main_cb((rizz_plugin*)&g_plugin.plugins[index].p,
                                                 RIZZ_PLUGIN_EVENT_SHUTDOWN);
        }
#endif

        for (int i = 0; i < sx_array_count(g_plugin.plugins); i++) {
            sx_free(g_plugin.alloc, g_plugin.plugins[i].deps);
        }
        sx_array_free(g_plugin.alloc, g_plugin.plugins);
    }

    sx_array_free(g_plugin.alloc, g_plugin.injected);
    sx_array_free(g_plugin.alloc, g_plugin.plugin_update_order);

    sx_memset(&g_plugin, 0x0, sizeof(g_plugin));
}

static bool rizz__plugin_order_dependencies()
{
    int num_plugins = sx_array_count(g_plugin.plugins);
    if (num_plugins == 0)
        return true;

    int level = 0;
    int count = 0;
    while (count < num_plugins) {
        int init_count = count;
        for (int i = 0; i < num_plugins; i++) {
            rizz__plugin_item* item = &g_plugin.plugins[i];
            if (item->order == -1) {
                if (item->num_deps > 0) {
                    int num_deps_met = 0;
                    for (int d = 0; d < item->num_deps; d++) {
                        for (int j = 0; j < num_plugins; j++) {
                            rizz__plugin_item* parent_item = &g_plugin.plugins[j];
                            if (i != j && parent_item->order != -1 &&
                                parent_item->order <= (level - 1) &&
                                sx_strequal(parent_item->info.name, item->deps[d].name)) {
                                num_deps_met++;
                                break;
                            }
                        }
                    }    // foreach dep
                    if (num_deps_met == item->num_deps) {
                        item->order = level;
                        count++;
                    }
                } else {
                    item->order = 0;
                    count++;
                }
            }
        }    // foreach: plugin

        if (init_count == count) {
            break;    // count is not changed, so nothing could be resolved. quit the loop
        }

        level++;
    }

    // check for errors
    if (count != num_plugins) {
        rizz_log_error("the following plugins' dependencies didn't met:");
        for (int i = 0; i < num_plugins; i++) {
            rizz__plugin_item* item = &g_plugin.plugins[i];
            if (item->order == -1) {
                char dep_str[512];
                sx_strcpy(dep_str, sizeof(dep_str), "[");
                for (int d = 0; d < item->num_deps; d++) {
                    if (d != item->num_deps - 1)
                        sx_strcat(dep_str, sizeof(dep_str), item->deps[d].name);
                    else
                        sx_strcat(dep_str, sizeof(dep_str), "]");
                }
                rizz_log_error("\t%s -(depends)-> %s",
                               item->info.name[0] ? item->info.name : "[entry]", dep_str);
            }
        }

        return false;
    }

    // sort them by their order and load
    rizz__plugin_tim_sort(g_plugin.plugin_update_order,
                          sx_array_count(g_plugin.plugin_update_order));

    return true;
}

#ifdef RIZZ_BUNDLE
static void rizz__plugin_register(const rizz_plugin_info* info)
{
    rizz__plugin_item item;
    sx_memset(&item, 0x0, sizeof(item));
    item.p.api = &the__plugin;
    item.p.iteration = 1;
    sx_memcpy(&item.info, info, sizeof(*info));
    sx_strcpy(item.filepath, sizeof(item.filepath), info->name);
    item.order = -1;

    sx_array_push(g_plugin.alloc, g_plugin.plugins, item);
    sx_array_push(g_plugin.alloc, g_plugin.plugin_update_order,
                  sx_array_count(g_plugin.plugins) - 1);
}

static bool rizz__plugin_load(const char* name)
{
    sx_assert(!g_plugin.loaded && "cannot load anymore plugins after `init_plugins` is called");

    return rizz__plugin_load_abs(name, false, NULL, 0);
}

bool rizz__plugin_load_abs(const char* name, bool entry, const char** edeps, int enum_deps)
{
    // find the plugin and save it's flags, the list is already populated
    int plugin_id = -1;
    for (int i = 0; i < sx_array_count(g_plugin.plugins); i++) {
        if (sx_strequal(g_plugin.plugins[i].filepath, name)) {
            plugin_id = i;
            break;
        }
    }

    if (plugin_id != -1) {
        rizz__plugin_item* item = &g_plugin.plugins[plugin_id];

        // We got the info, the plugin seems to be valid
        int num_deps = entry ? enum_deps : item->info.num_deps;
        const char** deps = entry ? edeps : item->info.deps;
        if (num_deps > 0 && deps) {
            item->deps = (rizz__plugin_dependency*)sx_malloc(
                g_plugin.alloc, sizeof(rizz__plugin_dependency) * num_deps);
            if (!item->deps) {
                sx_out_of_memory();
                return false;
            }
            item->num_deps = num_deps;
            for (int i = 0; i < num_deps; i++)
                sx_strcpy(item->deps[i].name, sizeof(item->deps[i].name), deps[i]);
        }

        return true;
    } else {
        rizz_log_error("plugin load failed: %s", name);
        return false;
    }
}

bool rizz__plugin_init_plugins()
{
    if (!rizz__plugin_order_dependencies()) {
        return false;
    }

    for (int i = 0; i < sx_array_count(g_plugin.plugin_update_order); i++) {
        int index = g_plugin.plugin_update_order[i];
        rizz__plugin_item* item = &g_plugin.plugins[index];

        // load the plugin
        int r;
        if ((r = item->info.main_cb((rizz_plugin*)&item->p, RIZZ_PLUGIN_EVENT_INIT)) != 0) {
            rizz_log_error("plugin load failed: %s, returned: %d", item->info.name, r);
            return false;
        }

        item->p._p = (void*)0x1;    // set it to something that indicates plugin is loaded

        if (item->info.name[0]) {
            int version = item->info.version;
            char filename[32];
            sx_os_path_basename(filename, sizeof(filename), item->filepath);
            rizz_log_info("(init) plugin: %s (%s) - %s - v%d.%d.%d", item->info.name, filename,
                          item->info.desc, rizz_version_major(version), rizz_version_minor(version),
                          rizz_version_bugfix(version));
        }
    }


    g_plugin.loaded = true;
    return true;

    return true;
}

void rizz__plugin_update(float dt)
{
    sx_unused(dt);
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        int index = g_plugin.plugin_update_order[i];
        rizz__plugin_item* item = &g_plugin.plugins[index];
        if (item->p._p == (void*)0x1) {
            sx_assert(item->info.main_cb);
            item->info.main_cb((rizz_plugin*)&item->p, RIZZ_PLUGIN_EVENT_STEP);
        }
    }
}

void rizz__plugin_broadcast_event(const rizz_app_event* e)
{
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        int index = g_plugin.plugin_update_order[i];
        rizz__plugin_item* item = &g_plugin.plugins[index];
        if (item->p._p == (void*)0x1) {
            if (item->info.event_cb)
                item->info.event_cb(e);
        }
    }
}
#else    // RIZZ_BUNDLE

static void rizz__plugin_reload_handler(cr_plugin* plugin, const char* filename, const void** ptrs,
                                        const void** new_ptrs, int num_ptrs)
{
    sx_unused(filename);
    sx_unused(plugin);

    for (int i = 0; i < num_ptrs; i++) {
        rizz__core_fix_callback_ptrs(ptrs, new_ptrs, num_ptrs);
    }
}

static bool rizz__plugin_load(const char* name)
{
    sx_assert(!g_plugin.loaded && "cannot load anymore plugins after `init_plugins` is called");

    // construct full filepath, by joining to root plugin path and adding extension
    char filepath[256];
#if SX_PLATFORM_LINUX || SX_PLATFORM_OSX || SX_PLATFORM_RPI
    sx_os_path_join(filepath, sizeof(filepath), g_plugin.plugin_path, "lib");
    sx_strcat(filepath, sizeof(filepath), name);
#else
    sx_os_path_join(filepath, sizeof(filepath), g_plugin.plugin_path, name);
#endif
    sx_strcat(filepath, sizeof(filepath), SX_DLL_EXT);
    return rizz__plugin_load_abs(filepath, false, NULL, 0);
}

bool rizz__plugin_load_abs(const char* filepath, bool entry, const char** edeps, int enum_deps)
{
    rizz__plugin_item item;
    sx_memset(&item, 0x0, sizeof(item));
    item.p.userdata = &the__plugin;

    // get info from the plugin
    void* dll = NULL;
    if (!entry) {
        dll = sx_os_dlopen(filepath);
        if (!dll) {
            rizz_log_error("plugin load failed: %s: %s", filepath, sx_os_dlerr());
            return false;
        }

        rizz_plugin_get_info_cb* get_info =
            (rizz_plugin_get_info_cb*)sx_os_dlsym(dll, "rizz_plugin_get_info");
        if (!get_info) {
            rizz_log_error("plugin missing rizz_plugin_get_info symbol: %s", filepath);
            return false;
        }

        get_info(&item.info);
    }

    sx_strcpy(item.filepath, sizeof(item.filepath), filepath);

    // We got the info, the plugin seems to be valid
    int num_deps = entry ? enum_deps : item.info.num_deps;
    const char** deps = entry ? edeps : item.info.deps;
    if (num_deps > 0 && deps) {
        item.deps = (rizz__plugin_dependency*)sx_malloc(g_plugin.alloc,
                                                        sizeof(rizz__plugin_dependency) * num_deps);
        if (!item.deps) {
            sx_out_of_memory();
            return false;
        }
        item.num_deps = num_deps;
        for (int i = 0; i < num_deps; i++)
            sx_strcpy(item.deps[i].name, sizeof(item.deps[i].name), deps[i]);
    }

    item.order = -1;

    if (dll)
        sx_os_dlclose(dll);

    sx_array_push(g_plugin.alloc, g_plugin.plugins, item);
    sx_array_push(g_plugin.alloc, g_plugin.plugin_update_order,
                  sx_array_count(g_plugin.plugins) - 1);

    return true;
}

bool rizz__plugin_init_plugins()
{
    if (!rizz__plugin_order_dependencies())
        return false;

    for (int i = 0; i < sx_array_count(g_plugin.plugin_update_order); i++) {
        int index = g_plugin.plugin_update_order[i];
        rizz__plugin_item* item = &g_plugin.plugins[index];

        if (!cr_plugin_load(item->p, item->filepath, rizz__plugin_reload_handler)) {
            rizz_log_error("plugin init failed: %s", item->filepath);
            return false;
        }

        if (item->info.name[0]) {
            int version = item->info.version;
            char filename[32];
            sx_os_path_basename(filename, sizeof(filename), item->filepath);
            rizz_log_info("(init) plugin: %s (%s) - %s - v%d.%d.%d", item->info.name, filename,
                          item->info.desc, rizz_version_major(version), rizz_version_minor(version),
                          rizz_version_bugfix(version));
        }
    }


    g_plugin.loaded = true;
    return true;
}

void rizz__plugin_update(float dt)
{
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        rizz__plugin_item* plugin = &g_plugin.plugins[g_plugin.plugin_update_order[i]];
        bool check_reload = false;
        plugin->update_tm += dt;
        if (plugin->update_tm >= RIZZ_CONFIG_PLUGIN_UPDATE_INTERVAL) {
            check_reload = true;
            plugin->update_tm = 0;
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

void rizz__plugin_broadcast_event(const rizz_app_event* e)
{
    for (int i = 0, c = sx_array_count(g_plugin.plugin_update_order); i < c; i++) {
        cr_plugin_event(g_plugin.plugins[g_plugin.plugin_update_order[i]].p, e);
    }
}
#endif    // RIZZ_BUNDLE

void rizz__plugin_inject_api(const char* name, uint32_t version, void* api)
{
    int api_idx = -1;
    for (int i = 0, c = sx_array_count(g_plugin.injected); i < c; i++) {
        if (sx_strequal(g_plugin.injected[i].name, name) &&
            g_plugin.injected[i].version == version) {
            api_idx = i;
            break;
        }
    }

    if (api_idx == -1) {
        rizz__plugin_injected_api item = { { 0 }, version, api };
        sx_strcpy(item.name, sizeof(item.name), name);
        sx_array_push(g_plugin.alloc, g_plugin.injected, item);
    } else {
        g_plugin.injected[api_idx].api = api;

        // broatcast API change event
        rizz_app_event e = { RIZZ_APP_EVENTTYPE_UPDATE_APIS };
        rizz__plugin_broadcast_event(&e);
    }
}

void rizz__plugin_remove_api(const char* name, uint32_t version)
{
    for (int i = 0, c = sx_array_count(g_plugin.injected); i < c; i++) {
        if (sx_strequal(g_plugin.injected[i].name, name) &&
            g_plugin.injected[i].version == version) {
            sx_array_pop(g_plugin.injected, i);
            return;
        }
    }
    rizz_log_warn("API (name='%s', version=%d) not found", name, version);
}

const char* rizz__plugin_crash_reason(rizz_plugin_crash crash)
{
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

rizz_api_plugin the__plugin = { rizz__plugin_load,           rizz__plugin_inject_api,
                                rizz__plugin_remove_api,     rizz__plugin_get_api,
                                rizz__plugin_get_api_byname, rizz__plugin_crash_reason };

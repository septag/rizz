#include "rizz/input.h"

#include "rizz/app.h"
#include "rizz/core.h"
#include "rizz/imgui.h"
#include "rizz/plugin.h"

#include "sx/allocator.h"
#include "sx/string.h"
#include "sx/timer.h"

#if SX_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
#include "gainput/gainput.h"
SX_PRAGMA_DIAGNOSTIC_POP()

using namespace gainput;

#ifndef INPUT_CONFIG_FORCE_MOUSE
#    define INPUT_CONFIG_FORCE_MOUSE 0
#endif

#ifndef INPUT_CONFIG_FORCE_TOUCH
#    define INPUT_CONFIG_FORCE_TOUCH 0
#endif

#ifndef INPUT_CONFIG_FORCE_TOUCH
#    define INPUT_CONFIG_FORCE_TOUCH 0
#endif

#define INPUT_ENABLE_MOUSE \
    (SX_PLATFORM_OSX | SX_PLATFORM_WINDOWS | SX_PLATFORM_LINUX | INPUT_CONFIG_FORCE_MOUSE)
#define INPUT_ENABLE_TOUCH (SX_PLATFORM_IOS | SX_PLATFORM_ANDROID | INPUT_CONFIG_FORCE_TOUCH)

RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_core*   the_core;
RIZZ_STATE static rizz_api_app*    the_app;
RIZZ_STATE static rizz_api_imgui*  the_imgui;

RIZZ_STATE static const sx_alloc* g_input_alloc;

class ga_allocator : public Allocator {
    void* Allocate(size_t size, size_t align) override {
        sx_assert(g_input_alloc);
        if (!size) {
            return nullptr;
        }

        void* p = sx_malloc(g_input_alloc, size);
        if (!p) {
            sx_out_of_memory();
            return nullptr;
        }
        return p;
    }

    void Deallocate(void* ptr) override {
        sx_assert(g_input_alloc);
        sx_free(g_input_alloc, ptr);
    }
};

struct input__context {
    ga_allocator  ga_alloc;
    InputManager* mgr;
    InputMap*     mapper;
};

static input__context g_input;

static bool input__init() {
    g_input_alloc = the_core->alloc(RIZZ_MEMID_INPUT);

    g_input.mgr =
        new (sx_malloc(g_input_alloc, sizeof(InputManager))) InputManager(false, g_input.ga_alloc);
    if (!g_input.mgr) {
        sx_out_of_memory();
        return false;
    }
    g_input.mgr->SetDisplaySize(the_app->width(), the_app->height());

    g_input.mapper = new (sx_malloc(g_input_alloc, sizeof(InputMap)))
        InputMap(*g_input.mgr, "rizz_input_map", g_input.ga_alloc);
    if (!g_input.mapper) {
        sx_out_of_memory();
        return false;
    }

    return true;
}

static void input__release() {
    if (g_input.mapper) {
        g_input.mapper->~InputMap();
        sx_free(g_input_alloc, g_input.mapper);
    }

    if (g_input.mgr) {
        g_input.mgr->~InputManager();
        sx_free(g_input_alloc, g_input.mgr);
    }
}

static rizz_input_device input__create_device(rizz_input_device_type type) {
    DeviceId    device_id = 0;
    const char* device_name = "";

    switch (type) {
    case RIZZ_INPUT_DEVICETYPE_MOUSE:
        device_id = g_input.mgr->CreateDevice<InputDeviceMouse>();
        device_name = "mouse";
        break;

    case RIZZ_INPUT_DEVICETYPE_KEYBOARD:
        device_id = g_input.mgr->CreateDevice<InputDeviceKeyboard>();
        device_name = "keyboard";
        break;

    case RIZZ_INPUT_DEVICETYPE_PAD:
        device_id = g_input.mgr->CreateDevice<InputDevicePad>();
        device_name = "gamepad";
        break;

    case RIZZ_INPUT_DEVICETYPE_TOUCH:
        device_id = g_input.mgr->CreateDevice<InputDeviceTouch>();
        device_name = "touch";
        break;

    case RIZZ_INPUT_DEVICETYPE_BUILTIN:
        device_id = g_input.mgr->CreateDevice<InputDeviceBuiltIn>();
        device_name = "builtin";
        break;
    }

    return { rizz_to_id(device_id) };
}

static void input__map_bool(rizz_input_device device, int device_key, rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    sx_assert(device.id);

    g_input.mapper->MapBool((UserButtonId)key, (DeviceId)rizz_to_index(device.id),
                            (DeviceButtonId)device_key);
}

static void input__map_float(rizz_input_device device, int device_key, rizz_input_userkey key,
                             float min_val, float max_val,
                             float (*filter_cb)(float const value, void* user),
                             void* filter_user_ptr) {
    sx_assert(g_input.mapper);
    sx_assert(device.id);

    g_input.mapper->MapFloat((UserButtonId)key, (DeviceId)rizz_to_index(device.id),
                             (DeviceButtonId)device_key, min_val, max_val, filter_cb,
                             filter_user_ptr);
}

static void input__unmap(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    g_input.mapper->Unmap((UserButtonId)key);
}

static bool input__get_bool(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBool((UserButtonId)key);
}

static bool input__get_bool_pressed(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBoolIsNew((UserButtonId)key);
}

static bool input__get_bool_released(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBoolWasDown((UserButtonId)key);
}

static bool input__get_bool_previous(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBoolPrevious((UserButtonId)key);
}

static float input__get_float(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetFloat((UserButtonId)key);
}

static float input__get_float_previous(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetFloatPrevious((UserButtonId)key);
}

static float input__get_float_delta(rizz_input_userkey key) {
    sx_assert(g_input.mapper);
    return g_input.mapper->GetFloatDelta((UserButtonId)key);
}

static void input__clear_mappings() {
    sx_assert(g_input.mapper);
    g_input.mapper->Clear();
}

static void input__set_dead_zone(rizz_input_userkey key, float zone) {
    sx_assert(g_input.mapper);
    g_input.mapper->SetDeadZone(key, zone);
}

static void input__set_userkey_policy(rizz_input_userkey key, rizz_input_userkey_policy policy) {
    sx_assert(g_input.mapper);
    g_input.mapper->SetUserButtonPolicy(key, (InputMap::UserButtonPolicy)policy);
}

static bool input__device_avail(rizz_input_device device) {
    sx_assert(g_input.mapper);
    sx_assert(device.id);
    return g_input.mgr->GetDevice(rizz_to_index(device.id))->IsAvailable();
}

static rizz_api_input the__input = { input__create_device,     input__map_bool,
                                     input__map_float,         input__unmap,
                                     input__clear_mappings,    input__device_avail,
                                     input__get_bool,          input__get_bool_pressed,
                                     input__get_bool_released, input__get_bool_previous,
                                     input__get_float,         input__get_float_previous,
                                     input__get_float_delta,   input__set_dead_zone,
                                     input__set_userkey_policy };

rizz_plugin_decl_main(input, plugin, e) {
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        g_input.mgr->Update((uint64_t)sx_tm_ms(the_core->delta_tick()));
        break;
    }
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = (rizz_api_core*)the_plugin->get_api(RIZZ_API_CORE, 0);
        the_app = (rizz_api_app*)the_plugin->get_api(RIZZ_API_APP, 0);
        the_imgui = (rizz_api_imgui*)the_plugin->get_api_byname("imgui", 0);

        if (!input__init()) {
            return -1;
        }

        the_plugin->inject_api("input", 0, &the__input);
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("input", 0, &the__input);
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("input", 0);
        input__release();
        break;
    }

    return 0;
}

static void input__dispatch_event(const rizz_app_event* e) {
#if SX_PLATFORM_WINDOWS
    MSG* msg = (MSG*)e->native_event;
    g_input.mgr->HandleMessage(*msg);
#endif
}

rizz_plugin_decl_event_handler(input, e) {
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_RESIZED:
        g_input.mgr->SetDisplaySize(e->window_width, e->window_height);
        break;

    case RIZZ_APP_EVENTTYPE_UPDATE_APIS:
        break;

    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
    case RIZZ_APP_EVENTTYPE_MOUSE_SCROLL:
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        input__dispatch_event(e);
        break;
    default:
        break;
    }
}

static const char* input__deps[] = { "imgui" };
rizz_plugin_implement_info(input, 1000, "input plugin", input__deps, 1);
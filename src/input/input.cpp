#include "rizz/input.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/string.h"
#include "sx/timer.h"

#include <float.h>

#if SX_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
SX_PRAGMA_DIAGNOSTIC_IGNORED_GCC("-Wunused-parameter")
#if SX_COMPILER_GCC >= 70000
SX_PRAGMA_DIAGNOSTIC_IGNORED_GCC("-Wimplicit-fallthrough")
#endif
#include "gainput/gainput.h"
#include "gainput/GainputDebugRenderer.h"
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
RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;

RIZZ_STATE static const sx_alloc* g_input_alloc;

class ga_allocator : public Allocator
{
    void* Allocate(size_t size, size_t align) override
    {
        sx_unused(align);
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

    void Deallocate(void* ptr) override
    {
        sx_assert(g_input_alloc);
        sx_free(g_input_alloc, ptr);
    }
};

struct input__device_info {
    rizz_input_device id;
    rizz_input_device_type type;
    const char* name;
};

class input__gainput_listener : public gainput::InputListener {
  private:
    rizz_input_callbacks m_callbacks;
    void* m_user;
    int m_priority;

  public:
    gainput::ListenerId m_id = 0;

    input__gainput_listener() = delete;
    input__gainput_listener(const input__gainput_listener&) = delete;

    explicit input__gainput_listener(const rizz_input_callbacks* callbacks, void* user, int priority)
    {
        m_callbacks = *callbacks;
        m_user = user;
        m_priority = priority;
    }

    bool OnDeviceButtonBool(DeviceId device, DeviceButtonId deviceButton, bool oldValue, bool newValue) override
    {
        return m_callbacks.on_bool({rizz_to_id(device)}, (int)deviceButton, oldValue, newValue, m_user);
    }

	bool OnDeviceButtonFloat(DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue) override
    {
        return m_callbacks.on_float({rizz_to_id(device)}, (int)deviceButton, oldValue, newValue, m_user);
    }

    int GetPriority() const override
    {
        return m_priority;
    }
}; 

struct input__debug_item {
    enum Type { Circle, Line, Text };

    Type type;
    union Params {
        Params() : x(0), y(0) {}
        Params(const Params& p) { sx_memcpy(this, &p, sizeof(Params)); }
        Params& operator=(const Params& p)
        {
            sx_memcpy(this, &p, sizeof(Params));
            return *this;
        }

        struct {
            float x1, y1, x2, y2;
        };

        struct {
            float xcenter, ycenter, radius;
        };

        struct {
            float x, y;
            char text[64];
        };
    } params;

    input__debug_item() = default;
    input__debug_item(const input__debug_item&) = default;
    input__debug_item& operator=(const input__debug_item&) = default;
};

struct input__context {
    ga_allocator ga_alloc;
    InputManager* mgr;
    InputMap* mapper;
    input__device_info* devices;           // sx_array
    input__debug_item* debug_items;        // sx_array
    input__gainput_listener* listeners;    // sx_array
    bool debugger;
};

RIZZ_STATE static input__context g_input;

class input__gainput_debugger : public gainput::DebugRenderer
{
    void DrawCircle(float x, float y, float radius) override
    {
        if (!g_input.debugger)
            return;

        input__debug_item item;
        item.type = input__debug_item::Circle;
        item.params.xcenter = x;
        item.params.ycenter = y;
        item.params.radius = radius;
        sx_array_push(g_input_alloc, g_input.debug_items, item);
    }

    void DrawLine(float x1, float y1, float x2, float y2) override
    {
        if (!g_input.debugger)
            return;

        input__debug_item item;
        item.type = input__debug_item::Line;
        item.params.x1 = x1;
        item.params.y1 = y1;
        item.params.x2 = x2;
        item.params.y2 = y2;
        sx_array_push(g_input_alloc, g_input.debug_items, item);
    }


    void DrawText(float x, float y, const char* const text) override
    {
        if (!g_input.debugger)
            return;

        input__debug_item item;
        item.type = input__debug_item::Text;
        item.params.x = x;
        item.params.y = y;
        sx_strcpy(item.params.text, sizeof(item.params.text), text);
        sx_array_push(g_input_alloc, g_input.debug_items, item);
    }
};

static bool input__init()
{
    g_input_alloc = the_core->alloc(RIZZ_MEMID_INPUT);

    g_input.mgr = sx_new(g_input_alloc, InputManager)(false, g_input.ga_alloc);
    if (!g_input.mgr) {
        sx_out_of_memory();
        return false;
    }
    g_input.mgr->SetDisplaySize(the_app->width(), the_app->height());

    g_input.mapper = sx_new(g_input_alloc, InputMap)(*g_input.mgr, "rizz_input_map", g_input.ga_alloc);
    if (!g_input.mapper) {
        sx_out_of_memory();
        return false;
    }

    return true;
}

static void input__release()
{
    sx_array_free(g_input_alloc, g_input.listeners);
    sx_delete(g_input_alloc, InputMap, g_input.mapper);
    sx_delete(g_input_alloc, InputManager, g_input.mgr);
    sx_array_free(g_input_alloc, g_input.devices);
    sx_array_free(g_input_alloc, g_input.debug_items);
}

static rizz_input_device input__create_device(rizz_input_device_type type)
{
    DeviceId device_id = 0;
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

    // Add to devices
    rizz_input_device dev = { rizz_to_id(device_id) };
    input__device_info info = { dev, type, device_name };
    sx_array_push(g_input_alloc, g_input.devices, info);

    return dev;
}

static void input__map_bool(rizz_input_device device, int device_key, rizz_input_userkey key)
{
    g_input.mapper->MapBool((UserButtonId)key, (DeviceId)rizz_to_index(device.id), (DeviceButtonId)device_key);
}

static void input__map_float(rizz_input_device device, int device_key, rizz_input_userkey key,
                             float min_val, float max_val,
                             float (*filter_cb)(float const value, void* user),
                             void* filter_user_ptr)
{
    g_input.mapper->MapFloat((UserButtonId)key, (DeviceId)rizz_to_index(device.id),
                             (DeviceButtonId)device_key, min_val, max_val, filter_cb,
                             filter_user_ptr);
}

static void input__unmap(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    g_input.mapper->Unmap((UserButtonId)key);
}

static bool input__get_bool(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBool((UserButtonId)key);
}

static bool input__get_bool_pressed(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBoolIsNew((UserButtonId)key);
}

static bool input__get_bool_released(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBoolWasDown((UserButtonId)key);
}

static bool input__get_bool_previous(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetBoolPrevious((UserButtonId)key);
}

static bool input__get_bool_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    return input_device->GetBool((DeviceButtonId)device_key);
}

static bool input__get_bool_pressed_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    bool prev = input_device->GetBoolPrevious((DeviceButtonId)device_key);
    bool curr = input_device->GetBool((DeviceButtonId)device_key);
    return !prev && curr;
}

static bool input__get_bool_released_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    bool prev = input_device->GetBoolPrevious((DeviceButtonId)device_key);
    bool curr = input_device->GetBool((DeviceButtonId)device_key);
    return prev && !curr;
}

static bool input__get_bool_previous_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    return input_device->GetBoolPrevious((DeviceButtonId)device_key);
}

static float input__get_float(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetFloat((UserButtonId)key);
}

static float input__get_float_previous(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetFloatPrevious((UserButtonId)key);
}

static float input__get_float_delta(rizz_input_userkey key)
{
    sx_assert(g_input.mapper);
    return g_input.mapper->GetFloatDelta((UserButtonId)key);
}

static float input__get_float_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    return input_device->GetFloat((DeviceButtonId)device_key);
}

static float input__get_float_previous_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    return input_device->GetFloatPrevious((DeviceButtonId)device_key);
}

static float input__get_float_delta_raw(rizz_input_device device, int device_key)
{
    InputDevice* input_device = g_input.mgr->GetDevice((DeviceId)rizz_to_index(device.id));
    sx_assert(input_device);
    return input_device->GetFloat((DeviceButtonId)device_key) - input_device->GetFloatPrevious((DeviceButtonId)device_key);
}

static void input__clear_mappings()
{
    sx_assert(g_input.mapper);
    g_input.mapper->Clear();
}

static void input__set_dead_zone(rizz_input_userkey key, float zone)
{
    sx_assert(g_input.mapper);
    g_input.mapper->SetDeadZone(key, zone);
}

static void input__set_userkey_policy(rizz_input_userkey key, rizz_input_userkey_policy policy)
{
    sx_assert(g_input.mapper);
    g_input.mapper->SetUserButtonPolicy(key, (InputMap::UserButtonPolicy)policy);
}

static bool input__device_avail(rizz_input_device device)
{
    sx_assert(g_input.mapper);
    sx_assert(device.id);
    return g_input.mgr->GetDevice(rizz_to_index(device.id))->IsAvailable();
}

static void input__show_debugger(bool* p_open)
{
    static int selected_input = -1;
    static input__gainput_debugger gainput_debugger;

    if (!the_imgui) {
        return;
    }

    g_input.debugger = p_open ? *p_open : true;
    if (g_input.debugger) {
        g_input.mgr->SetDebugRenderer(&gainput_debugger);
    }

    auto convert_debug_coords = [](sx_vec2 window_size, sx_vec2 window_pos, sx_vec2 pos,
                                   bool invert_y) -> sx_vec2 {
        if (invert_y) {
            pos.y = 1.0f - pos.y;
        }

        return sx_vec2f(window_pos.x + (pos.x * window_size.x),
                        window_pos.y + (pos.y * window_size.y));
    };

    auto draw_debug_items = [convert_debug_coords](const input__debug_item* items, int count,
                                                   sx_vec2 window_size, sx_vec2 window_pos,
                                                   ImDrawList* drawlist, bool invert_y) {
        for (int i = 0; i < count; i++) {
            const input__debug_item& item = items[i];
            switch (item.type) {
            case input__debug_item::Circle: {
                the_imgui->ImDrawList_AddCircle(
                    drawlist,
                    convert_debug_coords(window_size, window_pos,
                                         sx_vec2f(item.params.xcenter, item.params.ycenter),
                                         invert_y),
                    item.params.radius * window_size.x, 0xffffffff, 20, 2.0f);
            } break;
            case input__debug_item::Line:
                the_imgui->ImDrawList_AddLine(
                    drawlist,
                    convert_debug_coords(window_size, window_pos,
                                         sx_vec2f(item.params.x1, item.params.y1), invert_y),
                    convert_debug_coords(window_size, window_pos,
                                         sx_vec2f(item.params.x2, item.params.y2), invert_y),
                    0xff00ffff, 4.0f);
                break;
            case input__debug_item::Text:
                the_imgui->ImDrawList_AddText_Vec2(
                    drawlist,
                    convert_debug_coords(window_size, window_pos,
                                         sx_vec2f(item.params.x, item.params.y), false),
                    0xffffffff, item.params.text, nullptr);
                break;
            }
        }
    };

    the_imgui->SetNextWindowSizeConstraints(sx_vec2f(487.0f, 353.0f), sx_vec2f(FLT_MAX, FLT_MAX),
                                            nullptr, nullptr);
    // the_imgui->SetNextWindowSize(sx_vec2f(450.0f, 450.0f), ImGuiCond_Always);
    if (the_imgui->Begin("Input Debugger", p_open, ImGuiWindowFlags_NoScrollbar)) {
        the_imgui->Columns(2, nullptr, false);
        the_imgui->SetColumnWidth(0, 150.0f);

        the_imgui->BeginChild_Str("input_list_parent", sx_vec2f(0, 0), true, 0);
        the_imgui->Columns(3, nullptr, false);
        the_imgui->SetColumnWidth(0, 30.0f);
        the_imgui->SetColumnWidth(1, 70.0f);
        the_imgui->Text("#");
        the_imgui->NextColumn();
        the_imgui->Text("Name");
        the_imgui->NextColumn();
        the_imgui->Text("State");
        the_imgui->NextColumn();
        the_imgui->Columns(1, nullptr, false);
        the_imgui->Separator();
        the_imgui->BeginChild_Str("input_list",
                              sx_vec2f(the_imgui->GetWindowContentRegionWidth(), 150.0f), false, 0);

        the_imgui->Columns(3, nullptr, false);
        the_imgui->SetColumnWidth(0, 30.0f);
        the_imgui->SetColumnWidth(1, 70.0f);
        ImGuiListClipper clipper;
        int num_devices = sx_array_count(g_input.devices);
        the_imgui->ImGuiListClipper_Begin(&clipper, num_devices, -1.0f);
        char row_str[32];
        while (the_imgui->ImGuiListClipper_Step(&clipper)) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                sx_snprintf(row_str, sizeof(row_str), "%d", i + 1);
                if (the_imgui->Selectable_Bool(row_str, selected_input == i,
                                               ImGuiSelectableFlags_SpanAllColumns, SX_VEC2_ZERO)) {
                    selected_input = i;

                    // turn off debugging for all devices except this one
                    for (int di = 0; di < num_devices; di++) {
                        InputDevice* dev =
                            g_input.mgr->GetDevice(rizz_to_index(g_input.devices[di].id.id));
                        if (di != i) {
                            dev->SetDebugRenderingEnabled(false);
                        } else {
                            dev->SetDebugRenderingEnabled(true);
                        }
                    }
                    sx_array_clear(g_input.debug_items);
                }
                the_imgui->NextColumn();

                sx_assert(g_input.devices);
                the_imgui->Text(g_input.devices[i].name);
                the_imgui->NextColumn();

                InputDevice::DeviceState state =
                    g_input.mgr->GetDevice(rizz_to_index(g_input.devices[i].id.id))->GetState();

                sx_vec4 button_color = SX_VEC4_ZERO;
                switch (state) {
                case InputDevice::DS_OK:
                    button_color = sx_vec4f(0.0f, 0.8f, 0.1f, 1.0f);
                    break;
                case InputDevice::DS_UNAVAILABLE:
                    button_color = sx_vec4f(0.2f, 0.2f, 0.2f, 1.0f);
                    break;

                case InputDevice::DS_LOW_BATTERY:
                    button_color = sx_vec4f(0.8f, 0.8f, 0.0f, 1.0f);
                }

                the_imgui->ColorButton("input_device_state", button_color, 0, sx_vec2f(14.0f, 14.0f));
                the_imgui->NextColumn();
            }
        }    // while(clipper)

        the_imgui->ImGuiListClipper_End(&clipper);
        the_imgui->EndChild();
        the_imgui->EndChild();
        the_imgui->NextColumn();

        if (selected_input != -1) {
            sx_assert(g_input.devices);
            rizz_input_device_type type = g_input.devices[selected_input].type;
            sx_vec2 region;
            the_imgui->GetContentRegionAvail(&region);
            float w = region.x;
            the_imgui->BeginChild_Str("input_debugger_view", sx_vec2f(w, w), true,
                                      ImGuiWindowFlags_NoScrollbar);
            sx_vec2 view_size;
            sx_vec2 view_pos;
            the_imgui->GetWindowSize(&view_size);
            the_imgui->GetWindowPos(&view_pos);
            draw_debug_items(g_input.debug_items, sx_array_count(g_input.debug_items), view_size,
                             view_pos, the_imgui->GetWindowDrawList(),
                             type == RIZZ_INPUT_DEVICETYPE_PAD);
            the_imgui->EndChild();
        }
        the_imgui->NextColumn();
    }
    the_imgui->End();

    sx_array_clear(g_input.debug_items);
}

static rizz_input_listener input__register_listener(const rizz_input_callbacks* callbacks, 
                                                    void* user, int priority)
{
    sx_assert(callbacks);
    sx_assert(g_input.mgr);
    void* ptr = sx_array_add(g_input_alloc, g_input.listeners, 1);
    sx_assert_always(ptr);
    input__gainput_listener* listener = sx_pnew(ptr, input__gainput_listener)(callbacks, user, priority);
    gainput::ListenerId id = {g_input.mgr->AddListener(listener)};
    listener->m_id = id;
    return {rizz_to_id(id)};
}

static void input__unregister_listener(rizz_input_listener listener_id)
{
    sx_assert(listener_id.id);

    gainput::ListenerId _id = rizz_to_index(listener_id.id);
    for (int i = 0, ic = sx_array_count(g_input.listeners); i < ic; i++) {
        if (g_input.listeners[i].m_id == _id) {
            g_input.mgr->RemoveListener(g_input.listeners[i].m_id);
            sx_array_pop(g_input.listeners, i);
            break;
        }
    }
}

static rizz_api_input the__input = { input__create_device,
                                     input__map_bool,
                                     input__map_float,
                                     input__unmap,
                                     input__clear_mappings,
                                     input__device_avail,
                                     input__get_bool,
                                     input__get_bool_pressed,
                                     input__get_bool_released,
                                     input__get_bool_previous,
                                     input__get_bool_raw,
                                     input__get_bool_pressed_raw,
                                     input__get_bool_released_raw,
                                     input__get_bool_previous_raw,
                                     input__get_float,
                                     input__get_float_previous,
                                     input__get_float_delta,
                                     input__get_float_raw,
                                     input__get_float_previous_raw,
                                     input__get_float_delta_raw,
                                     input__set_dead_zone,
                                     input__set_userkey_policy,
                                     input__show_debugger,
                                     input__register_listener,
                                     input__unregister_listener };

rizz_plugin_decl_main(input, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        g_input.mgr->Update((uint64_t)sx_tm_ms(the_core->delta_tick()));
        g_input.debugger = false;
        break;
    }
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = (rizz_api_core*)the_plugin->get_api(RIZZ_API_CORE, 0);
        the_app = (rizz_api_app*)the_plugin->get_api(RIZZ_API_APP, 0);
        the_imgui = (rizz_api_imgui*)the_plugin->get_api_byname("imgui", 0);
        the_imguix = (rizz_api_imgui_extra*)the_plugin->get_api_byname("imgui_extra", 0);

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

static void input__dispatch_event(const rizz_app_event* e)
{
#if SX_PLATFORM_WINDOWS
    MSG* msg = (MSG*)e->native_event;
    g_input.mgr->HandleMessage(*msg);
#elif SX_PLATFORM_LINUX
    XEvent* ev = (XEvent*)e->native_event;
    g_input.mgr->HandleEvent(*ev);
#else
    sx_unused(e);
#endif
}

rizz_plugin_decl_event_handler(input, e)
{
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_RESIZED:
        g_input.mgr->SetDisplaySize(e->window_width, e->window_height);
        break;

    case RIZZ_APP_EVENTTYPE_UPDATE_APIS:
        the_imgui = (rizz_api_imgui*)the_plugin->get_api_byname("imgui", 0);
        the_imguix = (rizz_api_imgui_extra*)the_plugin->get_api_byname("imgui_extra", 0);
        break;

    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
    case RIZZ_APP_EVENTTYPE_MOUSE_SCROLL:
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
    case RIZZ_APP_EVENTTYPE_KEY_DOWN:
    case RIZZ_APP_EVENTTYPE_KEY_UP:
    case RIZZ_APP_EVENTTYPE_CHAR:
        input__dispatch_event(e);
        break;

    default:
        break;
    }
}

static const char* input__deps[] = { "imgui" };
rizz_plugin_implement_info(input, 1000, "input plugin", input__deps, 1);

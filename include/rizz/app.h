//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "sx/math.h"

#include "types.h"

enum {
    RIZZ_APP_MAX_TOUCHPOINTS = 8,
    RIZZ_APP_MAX_MOUSEBUTTONS = 3,
    RIZZ_APP_MAX_KEYCODES = 512,
};

typedef enum rizz_app_event_type {
    RIZZ_APP_EVENTTYPE_INVALID,
    RIZZ_APP_EVENTTYPE_KEY_DOWN,
    RIZZ_APP_EVENTTYPE_KEY_UP,
    RIZZ_APP_EVENTTYPE_CHAR,
    RIZZ_APP_EVENTTYPE_MOUSE_DOWN,
    RIZZ_APP_EVENTTYPE_MOUSE_UP,
    RIZZ_APP_EVENTTYPE_MOUSE_SCROLL,
    RIZZ_APP_EVENTTYPE_MOUSE_MOVE,
    RIZZ_APP_EVENTTYPE_MOUSE_ENTER,
    RIZZ_APP_EVENTTYPE_MOUSE_LEAVE,
    RIZZ_APP_EVENTTYPE_TOUCHES_BEGAN,
    RIZZ_APP_EVENTTYPE_TOUCHES_MOVED,
    RIZZ_APP_EVENTTYPE_TOUCHES_ENDED,
    RIZZ_APP_EVENTTYPE_TOUCHES_CANCELLED,
    RIZZ_APP_EVENTTYPE_RESIZED,
    RIZZ_APP_EVENTTYPE_ICONIFIED,
    RIZZ_APP_EVENTTYPE_RESTORED,
    RIZZ_APP_EVENTTYPE_SUSPENDED,
    RIZZ_APP_EVENTTYPE_RESUMED,
    RIZZ_APP_EVENTTYPE_UPDATE_CURSOR,
    RIZZ_APP_EVENTTYPE_QUIT_REQUESTED,
    _RIZZ_APP_EVENTTYPE_NUM,
    RIZZ_APP_EVENTTYPE_UPDATE_APIS,    // happens when a plugin updates it's API
    _RIZZ_APP_EVENTTYPE_FORCE_U32 = 0x7FFFFFF
} rizz_app_event_type;

/* key codes are the same names and values as GLFW */
typedef enum rizz_keycode {
    RIZZ_APP_KEYCODE_INVALID = 0,
    RIZZ_APP_KEYCODE_SPACE = 32,
    RIZZ_APP_KEYCODE_APOSTROPHE = 39, /* ' */
    RIZZ_APP_KEYCODE_COMMA = 44,      /* , */
    RIZZ_APP_KEYCODE_MINUS = 45,      /* - */
    RIZZ_APP_KEYCODE_PERIOD = 46,     /* . */
    RIZZ_APP_KEYCODE_SLASH = 47,      /* / */
    RIZZ_APP_KEYCODE_0 = 48,
    RIZZ_APP_KEYCODE_1 = 49,
    RIZZ_APP_KEYCODE_2 = 50,
    RIZZ_APP_KEYCODE_3 = 51,
    RIZZ_APP_KEYCODE_4 = 52,
    RIZZ_APP_KEYCODE_5 = 53,
    RIZZ_APP_KEYCODE_6 = 54,
    RIZZ_APP_KEYCODE_7 = 55,
    RIZZ_APP_KEYCODE_8 = 56,
    RIZZ_APP_KEYCODE_9 = 57,
    RIZZ_APP_KEYCODE_SEMICOLON = 59, /* ; */
    RIZZ_APP_KEYCODE_EQUAL = 61,     /* = */
    RIZZ_APP_KEYCODE_A = 65,
    RIZZ_APP_KEYCODE_B = 66,
    RIZZ_APP_KEYCODE_C = 67,
    RIZZ_APP_KEYCODE_D = 68,
    RIZZ_APP_KEYCODE_E = 69,
    RIZZ_APP_KEYCODE_F = 70,
    RIZZ_APP_KEYCODE_G = 71,
    RIZZ_APP_KEYCODE_H = 72,
    RIZZ_APP_KEYCODE_I = 73,
    RIZZ_APP_KEYCODE_J = 74,
    RIZZ_APP_KEYCODE_K = 75,
    RIZZ_APP_KEYCODE_L = 76,
    RIZZ_APP_KEYCODE_M = 77,
    RIZZ_APP_KEYCODE_N = 78,
    RIZZ_APP_KEYCODE_O = 79,
    RIZZ_APP_KEYCODE_P = 80,
    RIZZ_APP_KEYCODE_Q = 81,
    RIZZ_APP_KEYCODE_R = 82,
    RIZZ_APP_KEYCODE_S = 83,
    RIZZ_APP_KEYCODE_T = 84,
    RIZZ_APP_KEYCODE_U = 85,
    RIZZ_APP_KEYCODE_V = 86,
    RIZZ_APP_KEYCODE_W = 87,
    RIZZ_APP_KEYCODE_X = 88,
    RIZZ_APP_KEYCODE_Y = 89,
    RIZZ_APP_KEYCODE_Z = 90,
    RIZZ_APP_KEYCODE_LEFT_BRACKET = 91,  /* [ */
    RIZZ_APP_KEYCODE_BACKSLASH = 92,     /* \ */
    RIZZ_APP_KEYCODE_RIGHT_BRACKET = 93, /* ] */
    RIZZ_APP_KEYCODE_GRAVE_ACCENT = 96,  /* ` */
    RIZZ_APP_KEYCODE_WORLD_1 = 161,      /* non-US #1 */
    RIZZ_APP_KEYCODE_WORLD_2 = 162,      /* non-US #2 */
    RIZZ_APP_KEYCODE_ESCAPE = 256,
    RIZZ_APP_KEYCODE_ENTER = 257,
    RIZZ_APP_KEYCODE_TAB = 258,
    RIZZ_APP_KEYCODE_BACKSPACE = 259,
    RIZZ_APP_KEYCODE_INSERT = 260,
    RIZZ_APP_KEYCODE_DELETE = 261,
    RIZZ_APP_KEYCODE_RIGHT = 262,
    RIZZ_APP_KEYCODE_LEFT = 263,
    RIZZ_APP_KEYCODE_DOWN = 264,
    RIZZ_APP_KEYCODE_UP = 265,
    RIZZ_APP_KEYCODE_PAGE_UP = 266,
    RIZZ_APP_KEYCODE_PAGE_DOWN = 267,
    RIZZ_APP_KEYCODE_HOME = 268,
    RIZZ_APP_KEYCODE_END = 269,
    RIZZ_APP_KEYCODE_CAPS_LOCK = 280,
    RIZZ_APP_KEYCODE_SCROLL_LOCK = 281,
    RIZZ_APP_KEYCODE_NUM_LOCK = 282,
    RIZZ_APP_KEYCODE_PRINT_SCREEN = 283,
    RIZZ_APP_KEYCODE_PAUSE = 284,
    RIZZ_APP_KEYCODE_F1 = 290,
    RIZZ_APP_KEYCODE_F2 = 291,
    RIZZ_APP_KEYCODE_F3 = 292,
    RIZZ_APP_KEYCODE_F4 = 293,
    RIZZ_APP_KEYCODE_F5 = 294,
    RIZZ_APP_KEYCODE_F6 = 295,
    RIZZ_APP_KEYCODE_F7 = 296,
    RIZZ_APP_KEYCODE_F8 = 297,
    RIZZ_APP_KEYCODE_F9 = 298,
    RIZZ_APP_KEYCODE_F10 = 299,
    RIZZ_APP_KEYCODE_F11 = 300,
    RIZZ_APP_KEYCODE_F12 = 301,
    RIZZ_APP_KEYCODE_F13 = 302,
    RIZZ_APP_KEYCODE_F14 = 303,
    RIZZ_APP_KEYCODE_F15 = 304,
    RIZZ_APP_KEYCODE_F16 = 305,
    RIZZ_APP_KEYCODE_F17 = 306,
    RIZZ_APP_KEYCODE_F18 = 307,
    RIZZ_APP_KEYCODE_F19 = 308,
    RIZZ_APP_KEYCODE_F20 = 309,
    RIZZ_APP_KEYCODE_F21 = 310,
    RIZZ_APP_KEYCODE_F22 = 311,
    RIZZ_APP_KEYCODE_F23 = 312,
    RIZZ_APP_KEYCODE_F24 = 313,
    RIZZ_APP_KEYCODE_F25 = 314,
    RIZZ_APP_KEYCODE_KP_0 = 320,
    RIZZ_APP_KEYCODE_KP_1 = 321,
    RIZZ_APP_KEYCODE_KP_2 = 322,
    RIZZ_APP_KEYCODE_KP_3 = 323,
    RIZZ_APP_KEYCODE_KP_4 = 324,
    RIZZ_APP_KEYCODE_KP_5 = 325,
    RIZZ_APP_KEYCODE_KP_6 = 326,
    RIZZ_APP_KEYCODE_KP_7 = 327,
    RIZZ_APP_KEYCODE_KP_8 = 328,
    RIZZ_APP_KEYCODE_KP_9 = 329,
    RIZZ_APP_KEYCODE_KP_DECIMAL = 330,
    RIZZ_APP_KEYCODE_KP_DIVIDE = 331,
    RIZZ_APP_KEYCODE_KP_MULTIPLY = 332,
    RIZZ_APP_KEYCODE_KP_SUBTRACT = 333,
    RIZZ_APP_KEYCODE_KP_ADD = 334,
    RIZZ_APP_KEYCODE_KP_ENTER = 335,
    RIZZ_APP_KEYCODE_KP_EQUAL = 336,
    RIZZ_APP_KEYCODE_LEFT_SHIFT = 340,
    RIZZ_APP_KEYCODE_LEFT_CONTROL = 341,
    RIZZ_APP_KEYCODE_LEFT_ALT = 342,
    RIZZ_APP_KEYCODE_LEFT_SUPER = 343,
    RIZZ_APP_KEYCODE_RIGHT_SHIFT = 344,
    RIZZ_APP_KEYCODE_RIGHT_CONTROL = 345,
    RIZZ_APP_KEYCODE_RIGHT_ALT = 346,
    RIZZ_APP_KEYCODE_RIGHT_SUPER = 347,
    RIZZ_APP_KEYCODE_MENU = 348,
} rizz_keycode;

typedef struct rizz_touch_point {
    uintptr_t identifier;
    float pos_x;
    float pos_y;
    bool changed;
} rizz_touch_point;

typedef enum rizz_mouse_btn {
    RIZZ_APP_MOUSEBUTTON_INVALID = -1,
    RIZZ_APP_MOUSEBUTTON_LEFT = 0,
    RIZZ_APP_MOUSEBUTTON_RIGHT = 1,
    RIZZ_APP_MOUSEBUTTON_MIDDLE = 2,
} rizz_mouse_btn;

enum rizz_modifier_keys_ {
    RIZZ_APP_MODIFIERKEY_SHIFT = (1 << 0),
    RIZZ_APP_MODIFIERKEY_CTRL = (1 << 1),
    RIZZ_APP_MODIFIERKEY_ALT = (1 << 2),
    RIZZ_APP_MODIFIERKEY_SUPER = (1 << 3)
};
typedef uint32_t rizz_modifier_keys;

typedef struct rizz_app_event {
    uint64_t frame_count;
    rizz_app_event_type type;
    rizz_keycode key_code;
    uint32_t char_code;
    bool key_repeat;
    rizz_modifier_keys modkeys;
    rizz_mouse_btn mouse_button;
    float mouse_x;
    float mouse_y;
    float scroll_x;
    float scroll_y;
    int num_touches;
    rizz_touch_point touches[RIZZ_APP_MAX_TOUCHPOINTS];
    int window_width;
    int window_height;
    int framebuffer_width;
    int framebuffer_height;
    void* native_event;
} rizz_app_event;

typedef void(rizz_app_event_cb)(const rizz_app_event*);

#if SX_PLATFORM_ANDROID
typedef struct ANativeActivity ANativeActivity;
#endif

typedef struct rizz_api_app {
    int (*width)();
    int (*height)();
    sx_vec2 (*sizef)();
    bool (*highdpi)();
    float (*dpiscale)();
    void (*show_keyboard)(bool show);
    bool (*keyboard_shown)();
    bool (*key_pressed)(rizz_keycode key);
    void (*quit)();
    void (*request_quit)();
    void (*cancel_quit)();
    const char* (*name)();
} rizz_api_app;

#ifdef RIZZ_INTERNAL_API
RIZZ_API rizz_api_app the__app;

typedef struct sg_desc sg_desc;

void rizz__app_init_gfx_desc(sg_desc* desc);
const void* rizz__app_d3d11_device();
const void* rizz__app_d3d11_device_context();
#endif    // RIZZ_INTERNAL_API

#if SX_PLATFORM_ANDROID
typedef struct ANativeActivity ANativeActivity;
RIZZ_API void ANativeActivity_onCreate_(ANativeActivity*, void*, size_t);

#    define rizz_app_android_decl()                                                \
        __attribute__((visibility("default"))) void ANativeActivity_onCreate(      \
            ANativeActivity* activity, void* saved_state, size_t saved_state_size) \
        {                                                                          \
            ANativeActivity_onCreate_(activity, saved_state, saved_state_size);    \
        }
#else
#    define rizz_app_android_decl()
#endif
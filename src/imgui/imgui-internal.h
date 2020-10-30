#pragma once

typedef struct rizz_api_imguizmo rizz_api_imguizmo;
typedef struct rizz_api_core rizz_api_core;
typedef struct rizz_api_app rizz_api_app;
typedef struct sx_alloc sx_alloc;

// fwd: log-window.c
void imgui__show_log(bool* p_open);
bool imgui__log_init(rizz_api_core* core, rizz_api_app* app, const sx_alloc* alloc, uint32_t buffer_size);
void imgui__log_release(void);
void imgui__log_update(void);

void imgui__get_imguizmo_api(rizz_api_imguizmo* api);
void imgui__imguizmo_begin(void);
void imgui__imguizmo_setrect(float x, float y, float width, float height);


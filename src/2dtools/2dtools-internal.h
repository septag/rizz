#pragma once

#include "rizz/2dtools.h"

// sprite
bool sprite__init(rizz_api_core* core, rizz_api_asset* asset, rizz_api_refl* refl,
                  rizz_api_gfx* gfx);
void sprite__release(void);

void sprite__set_imgui(rizz_api_imgui* imgui);

rizz_sprite sprite__create(const rizz_sprite_desc* desc);
void sprite__destroy(rizz_sprite handle);
rizz_sprite sprite__clone(rizz_sprite src_handle, rizz_sprite_animclip clip_handle);
const rizz_atlas* sprite__atlas_get(rizz_asset atlas_asset);
sx_vec2 sprite__size(rizz_sprite handle);
sx_vec2 sprite__origin(rizz_sprite handle);
sx_color sprite__color(rizz_sprite handle);
const char* sprite__name(rizz_sprite handle);
sx_rect sprite__bounds(rizz_sprite handle);
sx_rect sprite__draw_bounds(rizz_sprite handle);
rizz_sprite_flip sprite__flip(rizz_sprite handle);
void sprite__set_size(rizz_sprite handle, const sx_vec2 size);
void sprite__set_origin(rizz_sprite handle, const sx_vec2 origin);
void sprite__set_color(rizz_sprite handle, const sx_color color);
void sprite__set_flip(rizz_sprite handle, rizz_sprite_flip flip);
rizz_sprite_drawdata* sprite__drawdata_make_batch(const rizz_sprite* sprs, int num_sprites,
                                                  const sx_alloc* alloc);
rizz_sprite_drawdata* sprite__drawdata_make(rizz_sprite spr, const sx_alloc* alloc);
void sprite__drawdata_free(rizz_sprite_drawdata* data, const sx_alloc* alloc);
void sprite__draw_batch(const rizz_sprite* sprs, int num_sprites, const sx_mat4* vp,
                        const sx_mat3* mats, sx_color* tints);
void sprite__draw(rizz_sprite spr, const sx_mat4* vp, const sx_mat3* mat, sx_color tint);
void sprite__draw_wireframe_batch(const rizz_sprite* sprs, int num_sprites, const sx_mat4* vp,
                                  const sx_mat3* mats);
void sprite__draw_wireframe(rizz_sprite spr, const sx_mat4* vp, const sx_mat3* mat);
void sprite__show_debugger(bool* p_open);
void sprite__animclip_restart(rizz_sprite_animclip handle);
rizz_sprite_animclip sprite__animclip_create(const rizz_sprite_animclip_desc* desc);
rizz_sprite_animclip sprite__animclip_clone(rizz_sprite_animclip src_handle);
void sprite__animclip_destroy(rizz_sprite_animclip handle);
void sprite__animclip_update_batch(const rizz_sprite_animclip* handles, int num_clips, float dt);
void sprite__animclip_update(rizz_sprite_animclip clip, float dt);
float sprite__animclip_fps(rizz_sprite_animclip handle);
float sprite__animclip_len(rizz_sprite_animclip handle);
rizz_sprite_flip sprite__animclip_flip(rizz_sprite_animclip handle);
rizz_event_queue* sprite__animclip_events(rizz_sprite_animclip handle);
void sprite__animclip_set_fps(rizz_sprite_animclip handle, float fps);
void sprite__animclip_set_len(rizz_sprite_animclip handle, float length);
rizz_sprite_animctrl sprite__animctrl_create(const rizz_sprite_animctrl_desc* desc);
void sprite__animctrl_restart(rizz_sprite_animctrl handle);
rizz_event_queue* sprite__animctrl_events(rizz_sprite_animctrl handle);
bool sprite__resize_draw_limits(int max_verts, int max_indices);
void sprite__animctrl_destroy(rizz_sprite_animctrl handle);
void sprite__animctrl_update_batch(const rizz_sprite_animctrl* handles, int num_ctrls, float dt);
void sprite__animctrl_update(rizz_sprite_animctrl handle, float dt);
void sprite__animctrl_set_paramb(rizz_sprite_animctrl handle, const char* name, bool b);
void sprite__animctrl_set_parami(rizz_sprite_animctrl handle, const char* name, int i);
void sprite__animctrl_set_paramf(rizz_sprite_animctrl handle, const char* name, float f);
bool sprite__animctrl_param_valueb(rizz_sprite_animctrl handle, const char* name);
float sprite__animctrl_param_valuef(rizz_sprite_animctrl handle, const char* name);
int sprite__animctrl_param_valuei(rizz_sprite_animctrl handle, const char* name);
rizz_sprite_animclip sprite__animctrl_clip(rizz_sprite_animctrl handle);

// font
bool font__init(rizz_api_core* core, rizz_api_asset* asset, rizz_api_refl* refl, rizz_api_gfx* gfx,
                rizz_api_app* app);
void font__set_imgui(rizz_api_imgui* imgui);
void font__release(void);
void font__update(void);
const rizz_font* font__get(rizz_asset font_asset);
void font__draw(const rizz_font* fnt, sx_vec2 pos, const char* text);
void font__drawf(const rizz_font* fnt, sx_vec2 pos, const char* fmt, ...);
void font__push_state(const rizz_font* fnt, const rizz_font_state* state);
void font__pop_state(const rizz_font* fnt);
void font__clear_state(const rizz_font* fnt);
rizz_font_bounds font__bounds(const rizz_font* fnt, sx_vec2 pos, const char* text);
rizz_font_line_bounds font__line_bounds(const rizz_font* fnt, float y);
rizz_font_vert_metrics font__vert_metrics(const rizz_font* fnt);
bool font__resize_draw_limits(int max_verts);
rizz_font_iter font__iter_init(const rizz_font* fnt, sx_vec2 pos, const char* text);
bool font__iter_next(const rizz_font* fnt, rizz_font_iter* iter, rizz_font_quad* quad);
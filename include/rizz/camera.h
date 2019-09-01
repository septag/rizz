//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "sx/math.h"

#include "types.h"

typedef struct rizz_camera {
    sx_vec3 forward;
    sx_vec3 right;
    sx_vec3 up;
    sx_vec3 pos;

    sx_quat quat;
    float ffar;
    float fnear;
    float fov;
    sx_rect viewport;
} rizz_camera;

typedef struct rizz_camera_fps {
    rizz_camera cam;
    float pitch;
    float yaw;
} rizz_camera_fps;

typedef struct rizz_api_camera {
    void (*init)(rizz_camera* cam, float fov_deg, const sx_rect viewport, float fnear, float ffar);
    void (*lookat)(rizz_camera* cam, const sx_vec3 pos, const sx_vec3 target, const sx_vec3 up);
    sx_mat4 (*ortho_mat)(const rizz_camera* cam);
    sx_mat4 (*perspective_mat)(const rizz_camera* cam);
    sx_mat4 (*view_mat)(const rizz_camera* cam);
    void (*calc_frustum_points)(const rizz_camera* cam, sx_vec3 frustum[8]);
    void (*calc_frustum_points_range)(const rizz_camera* cam, sx_vec3 frustum[8], float fnear,
                                      float ffar);

    void (*fps_init)(rizz_camera_fps* cam, float fov_deg, const sx_rect viewport, float fnear,
                     float ffar);
    void (*fps_lookat)(rizz_camera_fps* cam, const sx_vec3 pos, const sx_vec3 target,
                       const sx_vec3 up);
    void (*fps_pitch)(rizz_camera_fps* cam, float pitch);
    void (*fps_pitch_range)(rizz_camera_fps* cam, float pitch, float _min, float _max);
    void (*fps_yaw)(rizz_camera_fps* cam, float yaw);
    void (*fps_forward)(rizz_camera_fps* cam, float forward);
    void (*fps_strafe)(rizz_camera_fps* cam, float strafe);
} rizz_api_camera;

#ifdef RIZZ_INTERNAL_API
RIZZ_API rizz_api_camera the__camera;
#endif    // RIZZ_INTERNAL_API

//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#include "config.h"

#include "rizz/camera.h"
#include "rizz/graphics.h"

static void rizz__cam_init(rizz_camera* cam, float fov_deg, const sx_rect viewport, float fnear,
                           float ffar)
{
    cam->right = SX_VEC3_UNITX;
    cam->up = SX_VEC3_UNITZ;
    cam->forward = SX_VEC3_UNITY;
    cam->pos = SX_VEC3_ZERO;

    cam->quat = sx_quat_ident();
    cam->fov = fov_deg;
    cam->fnear = fnear;
    cam->ffar = ffar;
    cam->viewport = viewport;
}

static void rizz__cam_lookat(rizz_camera* cam, const sx_vec3 pos, const sx_vec3 target,
                             const sx_vec3 up)
{
    // TODO: figure UP vector out, I hacked up vector (in the matrix) to make it work correctly
    cam->forward = sx_vec3_norm(sx_vec3_sub(target, pos));
    cam->right = sx_vec3_norm(sx_vec3_cross(cam->forward, up));
    cam->up = sx_vec3_cross(cam->right, cam->forward);
    cam->pos = pos;

    sx_mat4 m = sx_mat4v(sx_vec4f(cam->right.x, cam->right.y, cam->right.z, 0),
                         sx_vec4f(-cam->up.x, -cam->up.y, -cam->up.z, 0),
                         sx_vec4f(cam->forward.x, cam->forward.y, cam->forward.z, 0), SX_VEC4_ZERO);
    cam->quat = sx_mat4_quat(&m);
}

static sx_mat4 rizz__cam_perspective_mat(const rizz_camera* cam)
{
    float w = cam->viewport.xmax - cam->viewport.xmin;
    float h = cam->viewport.ymax - cam->viewport.ymin;
    return sx_mat4_perspectiveFOV(sx_torad(cam->fov), w / h, cam->fnear, cam->ffar,
                                  the__gfx.GL_family());
}

static sx_mat4 rizz__cam_ortho_mat(const rizz_camera* cam)
{
    return sx_mat4_ortho(cam->viewport.xmax - cam->viewport.xmin,
                         cam->viewport.ymax - cam->viewport.ymin, cam->fnear, cam->ffar, 0,
                         the__gfx.GL_family());
}

static sx_mat4 rizz__cam_view_mat(const rizz_camera* cam)
{
    sx_vec3 zaxis = cam->forward;
    sx_vec3 xaxis = cam->right; // sx_vec3_norm(sx_vec3_cross(zaxis, up));
    sx_vec3 yaxis = cam->up;    // sx_vec3_cross(xaxis, zaxis);

    // clang-format off
    return sx_mat4f(xaxis.x, xaxis.y, xaxis.z, -sx_vec3_dot(xaxis, cam->pos), 
                    yaxis.x, yaxis.y, yaxis.z, -sx_vec3_dot(yaxis, cam->pos), 
                    -zaxis.x, -zaxis.y, -zaxis.z, sx_vec3_dot(zaxis, cam->pos), 
                    0.0f, 0.0f, 0.0f, 1.0f);
    // clang-format on
}

static void rizz__calc_frustum_points_range(const rizz_camera* cam, sx_vec3 frustum[8], float fnear,
                                            float ffar)
{
    const float fov = sx_torad(cam->fov);
    const float w = cam->viewport.xmax - cam->viewport.xmin;
    const float h = cam->viewport.ymax - cam->viewport.ymin;
    const float aspect = w / h;

    sx_vec3 xaxis = cam->right;
    sx_vec3 yaxis = cam->up;
    sx_vec3 zaxis = cam->forward;
    sx_vec3 pos = cam->pos;

    float near_plane_h = sx_tan(fov * 0.5f) * fnear;
    float near_plane_w = near_plane_h * aspect;

    float far_plane_h = sx_tan(fov * 0.5f) * ffar;
    float far_plane_w = far_plane_h * aspect;

    sx_vec3 center_near = sx_vec3_add(sx_vec3_mulf(zaxis, fnear), pos);
    sx_vec3 center_far = sx_vec3_add(sx_vec3_mulf(zaxis, ffar), pos);

    // scaled axises
    sx_vec3 xnear_scaled = sx_vec3_mulf(xaxis, near_plane_w);
    sx_vec3 xfar_scaled = sx_vec3_mulf(xaxis, far_plane_w);
    sx_vec3 ynear_scaled = sx_vec3_mulf(yaxis, near_plane_h);
    sx_vec3 yfar_scaled = sx_vec3_mulf(yaxis, far_plane_h);

    // near quad (normal inwards)
    frustum[0] = sx_vec3_sub(center_near, sx_vec3_add(xnear_scaled, ynear_scaled));
    frustum[1] = sx_vec3_add(center_near, sx_vec3_sub(xnear_scaled, ynear_scaled));
    frustum[2] = sx_vec3_add(center_near, sx_vec3_add(xnear_scaled, ynear_scaled));
    frustum[3] = sx_vec3_sub(center_near, sx_vec3_sub(xnear_scaled, ynear_scaled));

    // far quad (normal inwards)
    frustum[4] = sx_vec3_sub(center_far, sx_vec3_add(xfar_scaled, yfar_scaled));
    frustum[5] = sx_vec3_sub(center_far, sx_vec3_sub(xfar_scaled, yfar_scaled));
    frustum[6] = sx_vec3_add(center_far, sx_vec3_sub(xfar_scaled, yfar_scaled));
    frustum[7] = sx_vec3_add(center_far, sx_vec3_add(xfar_scaled, yfar_scaled));
}

static void rizz__calc_frustum_points(const rizz_camera* cam, sx_vec3 frustum[8])
{
    rizz__calc_frustum_points_range(cam, frustum, cam->fnear, cam->ffar);
}

static void rizz__cam_fps_init(rizz_camera_fps* cam, float fov_deg, const sx_rect viewport,
                               float fnear, float ffar)
{
    rizz__cam_init(&cam->cam, fov_deg, viewport, fnear, ffar);
    cam->pitch = cam->yaw = 0;
}

static void rizz__cam_fps_lookat(rizz_camera_fps* cam, const sx_vec3 pos, const sx_vec3 target,
                                 const sx_vec3 up)
{
    rizz__cam_lookat(&cam->cam, pos, target, up);

    sx_vec3 euler = sx_quat_toeuler(cam->cam.quat);
    cam->pitch = euler.x;
    cam->yaw = euler.y;
}

static void rizz__cam_update_rot(rizz_camera* cam)
{
    sx_mat4 m = sx_quat_mat4(cam->quat);
    cam->right = sx_vec3fv(m.col1.f);
    cam->up = sx_vec3fv(m.col2.f);
    cam->forward = sx_vec3fv(m.col3.f);
}

static void rizz__cam_fps_pitch(rizz_camera_fps* fps, float pitch)
{
    fps->pitch -= pitch;
    fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITZ, fps->yaw),
                                sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch));
    rizz__cam_update_rot(&fps->cam);
}

static void rizz__cam_fps_pitch_range(rizz_camera_fps* fps, float pitch, float _min, float _max)
{
    fps->pitch = sx_clamp(fps->pitch - pitch, _min, _max);
    fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITZ, fps->yaw),
                                sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch));
    rizz__cam_update_rot(&fps->cam);
}

static void rizz__cam_fps_yaw(rizz_camera_fps* fps, float yaw)
{
    fps->yaw -= yaw;
    fps->cam.quat = sx_quat_mul(sx_quat_rotateaxis(SX_VEC3_UNITZ, fps->yaw),
                                sx_quat_rotateaxis(SX_VEC3_UNITX, fps->pitch));
    rizz__cam_update_rot(&fps->cam);
}

static void rizz__cam_fps_forward(rizz_camera_fps* fps, float forward)
{
    fps->cam.pos = sx_vec3_add(fps->cam.pos, sx_vec3_mulf(fps->cam.forward, forward));
}

static void rizz__cam_fps_strafe(rizz_camera_fps* fps, float strafe)
{
    fps->cam.pos = sx_vec3_add(fps->cam.pos, sx_vec3_mulf(fps->cam.right, strafe));
}

rizz_api_camera the__camera = { .init = rizz__cam_init,
                                .lookat = rizz__cam_lookat,
                                .perspective_mat = rizz__cam_perspective_mat,
                                .ortho_mat = rizz__cam_ortho_mat,
                                .view_mat = rizz__cam_view_mat,
                                .calc_frustum_points = rizz__calc_frustum_points,
                                .calc_frustum_points_range = rizz__calc_frustum_points_range,
                                .fps_init = rizz__cam_fps_init,
                                .fps_lookat = rizz__cam_fps_lookat,
                                .fps_pitch = rizz__cam_fps_pitch,
                                .fps_pitch_range = rizz__cam_fps_pitch_range,
                                .fps_yaw = rizz__cam_fps_yaw,
                                .fps_forward = rizz__cam_fps_forward,
                                .fps_strafe = rizz__cam_fps_strafe };

#include "rizz/imgui-extra.h"

#include "imguizmo/ImGuizmo.h"
#include "imgui-internal.h"
#include "sx/math-vec.h"

// gizmo
static void imgui__gizmo_set_rect(const sx_rect rc)
{
    ImGuizmo_SetRect(rc.xmin, rc.ymin, rc.xmax - rc.xmin, rc.ymax - rc.ymin);
}

static void imgui__gizmo_decompose_mat4(const sx_mat4* mat, sx_vec3* translation, sx_vec3* rotation,
                                        sx_vec3* scale)
{
    sx_mat4 transpose = sx_mat4_transpose(mat);
    ImGuizmo_DecomposeMatrixToComponents(transpose.f, translation->f, rotation->f, scale->f);
}

static void imgui__gizmo_compose_mat4(sx_mat4* mat, const sx_vec3 translation,
                                      const sx_vec3 rotation, const sx_vec3 scale)
{
    ImGuizmo_RecomposeMatrixFromComponents(translation.f, rotation.f, scale.f, mat->f);
    sx_mat4_transpose(mat);
}

static inline void imgui__mat4_to_gizmo(float dest[16], const sx_mat4* src)
{
    dest[0] = src->m11;
    dest[1] = src->m21;
    dest[2] = src->m31;
    dest[3] = src->m41;
    dest[4] = src->m12;
    dest[5] = src->m22;
    dest[6] = src->m32;
    dest[7] = src->m42;
    dest[8] = src->m13;
    dest[9] = src->m23;
    dest[10] = src->m33;
    dest[11] = src->m43;
    dest[12] = src->m14;
    dest[13] = src->m24;
    dest[14] = src->m34;
    dest[15] = src->m44;
}

static inline sx_mat4 imgui__mat4_from_gizmo(const float src[16])
{
    return sx_mat4v(
        sx_vec4f(src[0], src[1], src[2], src[3]), sx_vec4f(src[4], src[5], src[6], src[7]),
        sx_vec4f(src[8], src[9], src[10], src[11]), sx_vec4f(src[12], src[13], src[14], src[15]));
}

static void imgui__gizmo_translate(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj,
                                   imgui_gizmo_mode mode, sx_mat4* delta_mat, sx_vec3* snap)
{
    float _mat[16];
    float tview[16];
    float tproj[16];
    float _delta[16];
    imgui__mat4_to_gizmo(_mat, mat);
    imgui__mat4_to_gizmo(tview, view);
    imgui__mat4_to_gizmo(tproj, proj);
    ImGuizmo_Manipulate(tview, tproj, TRANSLATE, (enum MODE)mode, _mat, delta_mat ? _delta : NULL,
                        snap ? snap->f : NULL, NULL, NULL);
    if (delta_mat)
        *delta_mat = imgui__mat4_from_gizmo(_delta);
    *mat = imgui__mat4_from_gizmo(_mat);
}

static void imgui__gizmo_rotation(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj,
                                  imgui_gizmo_mode mode, sx_mat4* delta_mat, float* snap)
{
    float _mat[16];
    float tview[16];
    float tproj[16];
    float _delta[16];
    imgui__mat4_to_gizmo(_mat, mat);
    imgui__mat4_to_gizmo(tview, view);
    imgui__mat4_to_gizmo(tproj, proj);
    ImGuizmo_Manipulate(tview, tproj, ROTATE, (enum MODE)mode, _mat, delta_mat ? _delta : NULL,
                        snap, NULL, NULL);
    if (delta_mat)
        *delta_mat = imgui__mat4_from_gizmo(_delta);
    *mat = imgui__mat4_from_gizmo(_mat);
}

static void imgui__gizmo_scale(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj,
                               imgui_gizmo_mode mode, sx_mat4* delta_mat, float* snap)
{
    float _mat[16];
    float tview[16];
    float tproj[16];
    float _delta[16];
    imgui__mat4_to_gizmo(_mat, mat);
    imgui__mat4_to_gizmo(tview, view);
    imgui__mat4_to_gizmo(tproj, proj);
    ImGuizmo_Manipulate(tview, tproj, SCALE, (enum MODE)mode, _mat, delta_mat ? _delta : NULL, snap,
                        NULL, NULL);
    if (delta_mat)
        *delta_mat = imgui__mat4_from_gizmo(_delta);
    *mat = imgui__mat4_from_gizmo(_mat);
}

void imgui__imguizmo_begin(void)
{
    ImGuizmo_BeginFrame();
}

void imgui__imguizmo_setrect(float x, float y, float width, float height)
{
    ImGuizmo_SetRect(x, y, width, height);
}


void imgui__get_imguizmo_api(rizz_api_imguizmo* api)
{
    sx_assert(api);

    *api = (rizz_api_imguizmo) {
        .hover = ImGuizmo_IsOver, 
        .is_using = ImGuizmo_IsUsing,
        .set_current_window = ImGuizmo_SetDrawlist,
        .set_ortho = ImGuizmo_SetOrthographic, 
        .enable = ImGuizmo_Enable,
        .set_rect = imgui__gizmo_set_rect,
        .decompose_mat4 = imgui__gizmo_decompose_mat4,
        .compose_mat4 = imgui__gizmo_compose_mat4, 
        .translate = imgui__gizmo_translate,
        .rotation = imgui__gizmo_rotation, 
        .scale = imgui__gizmo_scale
    };
}

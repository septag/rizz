#include "rizz/utility.h"

#include "sx/math-vec.h"

static sx_vec3 bezier3d(sx_vec3 a, sx_vec3 b, sx_vec3 c, sx_vec3 d, float t)
{
    t = sx_clamp(t, 0, 1);

    sx_vec3 ab = sx_vec3_lerp(a, b, t);
    sx_vec3 bc = sx_vec3_lerp(b, c, t);
    sx_vec3 cd = sx_vec3_lerp(c, d, t);

    sx_vec3 abc = sx_vec3_lerp(ab, bc, t);
    sx_vec3 bcd = sx_vec3_lerp(bc, cd, t);

    return sx_vec3_lerp(abc, bcd, t);
}

static sx_vec2 bezier2d(sx_vec2 a, sx_vec2 b, sx_vec2 c, sx_vec2 d, float t)
{
    t = sx_clamp(t, 0, 1);

    sx_vec2 ab = sx_vec2_lerp(a, b, t);
    sx_vec2 bc = sx_vec2_lerp(b, c, t);
    sx_vec2 cd = sx_vec2_lerp(c, d, t);

    sx_vec2 abc = sx_vec2_lerp(ab, bc, t);
    sx_vec2 bcd = sx_vec2_lerp(bc, cd, t);

    return sx_vec2_lerp(abc, bcd, t);
}

void spline__eval3d(const rizz_spline3d_desc* desc, sx_vec3* result)
{
    float t = desc->time;
    int num = desc->num_nodes;

    if (desc->norm)
        t *= (float)(num + (uint32_t)desc->loop);

    int a, b;
    if (desc->loop) {
        t = sx_mod(t, (float)desc->num_nodes);
        a = (int)sx_floor(t);
        b = a + 1;

        if (t >= (float)(desc->num_nodes - 1))
            b = 0;
    } else {
        t = sx_mod(t, (float)(desc->num_nodes - 1));
        a = (int)sx_floor(t);
        b = a + 1;
    }

    const rizz_spline3d_node* n1;
    const rizz_spline3d_node* n2;

    if (desc->node_stride == 0) {
        n1 = &desc->nodes[a];
        n2 = &desc->nodes[b];
    } else {
        n1 = (rizz_spline3d_node*)(((char*)desc->nodes) + desc->node_stride * a);
        n2 = (rizz_spline3d_node*)(((char*)desc->nodes) + desc->node_stride * b);
    }

    t = t - (float)a;
    *result = bezier3d(n1->pos, sx_vec3_add(n1->pos, n1->rwing), sx_vec3_add(n2->pos, n2->lwing),
                       n2->pos, t);
    if (desc->usereval)
        desc->usereval(n1, n2, t, result);
}

void spline__eval2d(const rizz_spline2d_desc* desc, sx_vec2* result)
{
    float t = desc->time;
    int num = desc->num_nodes;

    if (desc->norm)
        t *= (float)(num + (uint32_t)desc->loop);

    int a, b;
    if (desc->loop) {
        t = sx_mod(t, (float)desc->num_nodes);
        a = (int)sx_floor(t);
        b = a + 1;

        if (t >= (float)(desc->num_nodes - 1))
            b = 0;
    } else {
        t = sx_mod(t, (float)(desc->num_nodes - 1));
        a = (int)sx_floor(t);
        b = a + 1;
    }

    const rizz_spline2d_node* n1;
    const rizz_spline2d_node* n2;

    if (desc->node_stride == 0) {
        n1 = &desc->nodes[a];
        n2 = &desc->nodes[b];
    } else {
        n1 = (rizz_spline2d_node*)(((char*)desc->nodes) + desc->node_stride * a);
        n2 = (rizz_spline2d_node*)(((char*)desc->nodes) + desc->node_stride * b);
    }

    t = t - (float)a;
    *result = bezier2d(n1->pos, sx_vec2_add(n1->pos, n1->rwing), sx_vec2_add(n2->pos, n2->lwing),
                       n2->pos, t);
    if (desc->usereval)
        desc->usereval(n1, n2, t, result);
}
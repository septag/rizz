#version 450

layout (location = POSITION) in vec3 a_pos;
layout (location = NORMAL) in vec3 a_normal;
layout (location = TEXCOORD0) in vec2 a_uv;

layout (location = TEXCOORD0) out vec2 f_uv;
layout (location = TEXCOORD1) out vec3 f_normal;

layout (binding = 0, std140) uniform vs_globals {
    mat4 viewproj_mat;
    mat4 world_mat;
};

void main()
{
    vec4 pos = world_mat * vec4(a_pos.xyz, 1.0);
    gl_Position = viewproj_mat * pos;
    f_uv = a_uv;

    f_normal = mat3(world_mat) * a_normal;
}
#version 450

layout (location = POSITION) in vec3 a_pos;
layout (location = TEXCOORD0) in vec2 a_uv;

layout (location = TEXCOORD0) out vec2 f_uv;

layout (binding = 0, std140) uniform globals {
    mat4 vp;
};

void main()
{
    vec4 pos = vec4(a_pos.xy, 0, 1.0);
    gl_Position = vp * pos;
    f_uv = a_uv;
}
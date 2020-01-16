#version 450

layout (location = POSITION) in vec2 a_pos;
layout (location = TEXCOORD0) in vec2 a_uv;
layout (location = COLOR0) in vec4 a_color;

layout (location = COLOR0) flat out vec4 f_color;
layout (location = TEXCOORD0) out vec2 f_uv;

layout (binding = 0, std140) uniform matrices {
    mat4 vp;
};

void main() 
{
    vec4 pos = vec4(a_pos.xy, 0, 1.0);
    gl_Position = vp * pos;
    f_color = a_color;
    f_uv = a_uv;
}
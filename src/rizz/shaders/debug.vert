#version 450

layout (location = POSITION) in vec3 a_pos;
layout (location = TEXCOORD0) in vec2 a_uv;
layout (location = COLOR0) in vec4 a_color;

layout (location = TEXCOORD0) out vec2 f_uv;
layout (location = COLOR0) flat out vec4 f_color;

layout (binding = 0, std140) uniform matrices {
    mat4 model;
    mat4 vp;
};

void main() {
    vec4 world_pos = model * vec4(a_pos, 1.0);
    gl_Position = vp * world_pos;

    f_uv = a_uv;
    f_color = a_color;
}

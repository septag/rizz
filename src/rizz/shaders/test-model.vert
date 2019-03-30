#version 450

layout (location=POSITION) in vec3 a_pos;
layout (location=NORMAL) in vec3 a_normal;

layout (binding=0, std140) uniform mesh_uniforms_vs {
    mat4 model;
    mat4 vp;
};

layout (location=TEXCOORD0) out vec3 v_normal;

void main() {
    vec4 world_pos = model * vec4(a_pos, 1.0);
    gl_Position = vp * world_pos;
    v_normal = a_normal;
}

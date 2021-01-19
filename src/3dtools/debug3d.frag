#version 450

layout (location = TEXCOORD0) in vec2 f_uv;
layout (location = TEXCOORD1) flat in vec4 f_color;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_map;

void main() {
    vec4 color = texture(tex_map, f_uv);
    frag_color = color * f_color;
}
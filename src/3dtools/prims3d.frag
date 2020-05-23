#version 450

layout (location = TEXCOORD0) in vec2 f_uv;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_map;

void main() {
    vec4 color = vec4(1.0, 1.0, 1.0, texture(tex_map, f_uv).r);
    frag_color = color;
}
#version 450

layout (location = COLOR0) flat in vec4 f_color;
layout (location = TEXCOORD0) in vec2 f_uv;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_font_atlas;

void main() {
    vec4 color = vec4(1.0, 1.0, 1.0, texture(tex_font_atlas, f_uv).r);
    frag_color = color * f_color;
}
#version 450

precision lowp float;

layout (location = TEXCOORD0) in vec2 f_uv;
layout (location = COLOR0) flat in vec4 f_color;
layout (location = SV_Target0) out vec4 out_color;

layout (binding = 0) uniform sampler2D tex;

void main() {
    out_color = texture(tex, f_uv) * f_color;
}
#version 450

precision mediump float;

layout (location = TEXCOORD0)  in  vec2 f_coord;
layout (location = COLOR0) flat in vec4 f_color;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_image;

void main() 
{
    vec4 color = texture(tex_image, f_coord);
    frag_color = color * f_color;
}

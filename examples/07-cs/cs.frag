#version 450

precision mediump float;

layout (location = TEXCOORD0)  in  vec2 f_coord;
layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_image;

void main() 
{
    frag_color = texture(tex_image, f_coord);
}

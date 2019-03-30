#version 450

layout (location=COLOR0)     in vec4 f_color;
layout (location=TEXCOORD0)  in vec2 f_coord;

layout (location=SV_Target0) out vec4 frag_color;

layout (binding=0) uniform sampler2D tex;

void main()
{
    frag_color = f_color * texture(tex, f_coord);
}

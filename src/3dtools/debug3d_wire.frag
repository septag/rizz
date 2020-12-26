#version 450

precision lowp float;

layout (location = COLOR0) flat in vec4 f_color;
layout (location = SV_Target0) out vec4 out_color;

void main() 
{
    out_color = f_color;
}
#version 450

layout (location=POSITION)  in  vec2 a_pos;
layout (location=TEXCOORD0) in  vec2 a_coord;
layout (location=COLOR0)    in  vec4 a_color;

layout (location=COLOR0)    out vec4 f_color;
layout (location=TEXCOORD0) out vec2 f_coord;

layout (std140, binding = 0) uniform globals {
    vec4 disp_size;     // x,y = width/height of view
};

void main() 
{
    vec2 proj_pos = ((a_pos/disp_size.xy)-0.5)*vec2(2.0f, -2.0f);
    gl_Position = vec4(proj_pos.xy, 0.5, 1.0);
    f_coord = a_coord;
    f_color = a_color;
}
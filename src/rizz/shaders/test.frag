#version 450

precision mediump float;

layout (location = TEXCOORD0) in vec2 f_coord;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D color_map;

layout (binding=0, std140) uniform globals
{
    vec4 time;
};

void main() 
{
    vec4 color = vec4(sin(time.x), cos(time.x*2.0f), 1.0, 1.0);
    frag_color = texture(color_map, f_coord);// * color;
}

#version 450

layout (location = POSITION) in vec2 a_pos;
layout (location = TEXCOORD0) in vec2 a_coord;

layout (location = TEXCOORD0) out vec2 f_coord;
layout (location = TEXCOORD1) out vec3 f_ray;

layout (binding=0, std140) uniform vars {
    vec4 camera_corners[4];
    vec4 camera_pos;
};

#define quad_ratio camera_pos.w

void main() 
{
    gl_Position = vec4(a_pos.x, a_pos.y*quad_ratio, 0, 1.0);
    f_coord = a_coord;
    f_ray = camera_corners[gl_VertexIndex].xyz - camera_pos.xyz;
}

#version 450

layout (location = POSITION) in vec2 a_pos;

layout (location = TEXCOORD0) out vec3 f_ray;

layout (binding=0, std140) uniform vs_vars {
    vec4 camera_corners[4];
    vec4 camera_pos;
    vec4 screen;
};

void main() 
{
    gl_Position = vec4(a_pos.x*screen.x, a_pos.y*screen.y, 0, 1.0);
    f_ray = camera_corners[gl_VertexIndex].xyz - camera_pos.xyz;
}

#version 450

layout (location = POSITION) in vec3 a_pos;
layout (location = TEXCOORD0) in vec2 a_uv;
layout (location = COLOR0) in vec4 a_color;

layout (location = TEXCOORD0) out vec2 f_uv;
layout (location = COLOR0) flat out vec4 f_color;

layout (binding = 0, std140) uniform params {
    mat4 inv_view;
    mat4 vp;
};

const vec3 quad_positions[4] = {
    vec3(-1.0f, 0.0f, -1.0f),
    vec3(1.0f, 0.0f, -1.0f),
    vec3(1.0f, 0.0f,  1.0f),
    vec3(-1.0f, 0.0f,  1.0f)
};

struct particle_t {
    vec4 pos;
    vec4 vel;
}; 

layout(std430, binding = 0) readonly buffer particles_b {
    particle_t particles[];
};

#define PARTICLE_RADIUS 10.0

void main()
{
    int particle_id = gl_InstanceIndex;
    vec3 pos = a_pos * PARTICLE_RADIUS;
    pos = mat3(inv_view) * pos;
    // vec4 pos_billboard = /*inv_view * */vec4(pos, 1.0);
    // pos_billboard.xyz += particles[particle_id].pos.xyz;
    pos += particles[particle_id].pos.xyz;
    // vec4 pos_billboard = vec4(pos, 1.0);
    gl_Position = vp * vec4(pos, 1.0);
    // gl_Position = pos_billboard;
    f_uv = a_uv;

    float mag = particles[particle_id].vel.w / 9.0;
    f_color = mix(vec4(1.0, 0.1, 0.1, 1.0), a_color, mag);
}
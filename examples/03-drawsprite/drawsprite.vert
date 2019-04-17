#version 450

layout (location = POSITION)  in  vec2 a_pos;
layout (location = TEXCOORD0) in  vec2 a_coord;
layout (location = TEXCOORD1) in  vec4 a_transform;

layout (location = TEXCOORD0) out vec2 f_coord;

#ifdef WIREFRAME
layout (location = TEXCOORD2) in  vec3 a_bc;
layout (location = TEXCOORD1) out vec3 f_bc;
#endif

layout (binding=0, std140) uniform params {
    mat4 vp;
    vec4 motion;
};

#define motion_time motion.x
#define motion_amp motion.y
    
void main() {
    vec2 tpos = vec2(a_transform.x, 
        a_transform.y + motion_amp*sin(motion_time*3.0+(a_pos.x + a_transform.x)*0.3));
    gl_Position = vp * vec4(a_pos + tpos, 0.0, 1.0);
    f_coord = a_coord;

#ifdef WIREFRAME
    f_bc = a_bc;
#endif
}  
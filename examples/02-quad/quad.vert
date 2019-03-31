#version 450

layout (location = POSITION)  in  vec3 a_pos;
layout (location = TEXCOORD0) in  vec2 a_coord;
layout (location = TEXCOORD0) out vec2 f_coord;

layout (binding=0, std140) uniform matrices {
    mat4 mvp;
};
    
void main() {
    gl_Position = mvp * vec4(a_pos, 1.0);
    f_coord = a_coord;
}  
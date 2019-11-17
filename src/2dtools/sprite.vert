#version 450

layout (location = POSITION) in vec2 a_pos;
layout (location = COLOR0) in vec4 a_color1;
layout (location = TEXCOORD1) in vec3 a_transform1;
layout (location = TEXCOORD2) in vec3 a_transform2;

layout (location = COLOR0) flat out vec4 f_color;

#ifdef WIREFRAME
layout (location = TEXCOORD3) in  vec3 a_bc;
layout (location = TEXCOORD0) out vec3 f_bc;
#else
layout (location = TEXCOORD0) in vec2 a_uv;
layout (location = COLOR1) in vec4 a_color2;
layout (location = TEXCOORD0) out vec2 f_uv;
#endif

layout (binding = 0, std140) uniform matrices {
    mat4 vp;
};

void main() 
{
    mat4 model = mat4(vec4(a_transform1[0], a_transform1[1], 0, 0),
                      vec4(a_transform1[2], a_transform2[0], 0, 0),
                      vec4(0, 0, 1.0, 0),
                      vec4(a_transform2[1], a_transform2[2], 0, 1.0));    
    vec4 pos = model * vec4(a_pos.xy, 0, 1.0);
    gl_Position = vp * pos;


#ifdef WIREFRAME
    f_color = a_color1;
    f_bc = a_bc;
#else
    f_color = a_color1 * a_color2;
    f_uv = a_uv;
#endif
}
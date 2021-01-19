#version 450

layout (location = POSITION)  in vec3 a_pos;
layout (location = NORMAL)    in vec3 a_normal;
layout (location = TEXCOORD0) in vec2 a_uv;
layout (location = TEXCOORD1) in vec4 a_inst_tx1;
layout (location = TEXCOORD2) in vec4 a_inst_tx2;
layout (location = TEXCOORD3) in vec4 a_inst_tx3;
layout (location = TEXCOORD4) in vec3 a_inst_scale;
layout (location = TEXCOORD5) in vec4 a_inst_color;

layout (location = TEXCOORD0) out vec2 f_uv;
layout (location = TEXCOORD1) flat out vec4 f_color;

#define a_inst_pos a_inst_tx1.xyz
#define a_inst_rot mat3(vec3(a_inst_tx1.w, a_inst_tx2.x, a_inst_tx2.y), \
                        vec3(a_inst_tx2.z, a_inst_tx2.w, a_inst_tx3.x), \
                        vec3(a_inst_tx3.y, a_inst_tx3.z, a_inst_tx3.w))

layout (binding = 0, std140) uniform globals {
    mat4 vp;
};

void main()
{
    vec3 pos = a_pos * a_inst_scale;
    pos = a_inst_rot * pos;
    pos = pos + a_inst_pos;
    gl_Position = vp * vec4(pos, 1.0);

    const float e = 0.00001;
    
    #ifdef BOX_UV_WORKAROUND
        vec3 normal = abs(a_normal);
        vec2 uv = a_uv;
        if (normal.x > e) {
            uv *= a_inst_scale.yz;
        } else if (normal.y > e) {
            uv *= a_inst_scale.xz;
        } else if (normal.z > e) {
            uv *= a_inst_scale.xy;
        }
        f_uv = uv;
    #else
        f_uv = a_uv;
    #endif

    f_color = a_inst_color;
}
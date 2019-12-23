#version 450

layout (location = COLOR0) flat in vec4 f_color;
#ifdef WIREFRAME
layout (location = TEXCOORD0) in vec3 f_bc;
#else
layout (location = TEXCOORD0) in vec2 f_uv;
#endif

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_sprite;

void main() {
#ifdef WIREFRAME
    vec3 fw = abs(dFdx(f_bc)) + abs(dFdy(f_bc));
    vec3 val = smoothstep(vec3(0), fw*1.5, f_bc);
    float edge = min(min(val.x, val.y), val.z);
    vec4 edge_color = vec4(0.0, 1.0, 0.3, 1.0);
    frag_color = vec4(edge_color.rgb, 1.0 - edge) * f_color;
#else
    vec4 color = texture(tex_sprite, f_uv);
    frag_color = color * f_color;
#endif    
}
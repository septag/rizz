#version 450

precision mediump float;

layout (location = TEXCOORD0)  in  vec2 f_coord;

#ifdef WIREFRAME
layout (location = TEXCOORD1)  in  vec3 f_bc;
#endif

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_image;

void main() 
{
    vec4 color = texture(tex_image, f_coord);
#ifdef WIREFRAME
    vec3 fw = abs(dFdx(f_bc)) + abs(dFdy(f_bc));
    vec3 val = smoothstep(vec3(0), fw*1.5, f_bc);
    float edge = min(min(val.x, val.y), val.z);
    vec4 edge_color = vec4(0.0, 1.0, 0.3, 1.0);
    frag_color = mix(edge_color, color, edge);
#else
    frag_color = color;
#endif
}

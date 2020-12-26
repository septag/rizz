#version 450

layout (location = TEXCOORD0) in vec2 f_uv;
layout (location = TEXCOORD1) flat in vec4 f_color;
layout (location = TEXCOORD2) flat in vec3 f_normal;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_map;

void main() {
    vec4 color = texture(tex_map, f_uv);

    #if 0  // debug light
    vec3 light = normalize(vec3(0, 0.5f, -0.5f));
    float n_dot_l = max(0, dot(f_normal, -light));
    #endif

    frag_color = color * f_color;
}
#version 450
layout (location = TEXCOORD0) in vec2 f_uv;
layout (location = TEXCOORD1) in vec3 f_normal;

layout (location = SV_Target0) out vec4 frag_color;

layout (binding = 0) uniform sampler2D tex_name;

layout (binding = 0, std140) uniform fs_globals {
    vec3 light_dir;
};

void main() {
    vec3 normal = normalize(f_normal);
    vec3 lv = -light_dir;
    float n_dot_l = max(0.1, dot(normal, lv));

    vec4 color = vec4(1.0, 1.0, 1.0, texture(tex_name, f_uv).r);
    frag_color = vec4(color.xyz*n_dot_l, color.w);
}
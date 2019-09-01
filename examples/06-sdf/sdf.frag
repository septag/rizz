#version 450

layout (location = TEXCOORD0) in vec2 f_coord;
layout (location = TEXCOORD1) in vec3 f_ray;

layout (location = SV_Target0) out vec4 frag_color; 

layout (binding=0, std140) uniform vars {
    vec4 camera_pos;
    vec4 sphere1;
    vec4 box1;
    vec4 light;
    vec4 anim;
    mat4 shape_mat;
    vec3 back_light_dir;
};

#define anim_tm anim.x
#define shape_transition anim.y
#define shape_f1 anim.z
#define shape_f2 anim.w

const vec4 bg_color = vec4(0.25f, 0.5f, 0.75f, 1.0f);

float sd_sphere(vec3 p, float r)
{
    return length(p) -  r;
}

float sd_star_2d(in vec2 p, in float r, in float n, in float m)
{
    // next 4 lines can be precomputed for a given shape
    float an = 3.141593/float(n);
    float en = 6.283185/m;
    vec2  acs = vec2(cos(an),sin(an));
    vec2  ecs = vec2(cos(en),sin(en)); // ecs=vec2(0,1) for regular polygon,

    float bn = mod(atan(p.x,p.y),2.0*an) - an;
    p = length(p)*vec2(cos(bn),abs(sin(bn)));
    p -= r*acs;
    p += ecs*clamp( -dot(p,ecs), 0.0, r*acs.y/ecs.y);
    return length(p)*sign(p.x);
}

float sd_plane(vec3 p, vec4 n)
{
    return dot(p, n.xyz) + n.w;
}

float op_extrude( in vec3 p, in float sd_shape, in float h )
{
    float d = sd_shape;
    vec2 w = vec2( d, abs(p.z) - h );
    return min(max(w.x,w.y),0.0) + length(max(w,0.0));
}

float sd_star_3d(in vec3 p)
{
    vec4 p_transformed = shape_mat * vec4(p, 1.0);
    return op_extrude(p_transformed.xyz, sd_star_2d(p_transformed.xy, 0.5f, shape_f1, shape_f2), 0.2);
}

float displacement(vec3 p)
{
    float t = anim_tm;
    return sin(10*t*p.x)*sin(10*t*p.y)*sin(10*t*p.z)*mix(0.03, 0.09, t);
}

float op_smooth_union(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5*(d2-d1)/k, 0.0, 1.0);
    return mix(d2, d1, h) - k*h*(1.0-h);
}

float op_displace(float primitive, in vec3 p)
{
    float d1 = primitive;
    float d2 = displacement(p);
    return d1+d2;
}


float distance_field(vec3 p) 
{
    float plane = sd_plane(p, vec4(0, 0, 1.0, 0));
    float sphere = op_displace(sd_sphere(p - sphere1.xyz, sphere1.w), p);

    float shape = sd_star_3d(p);
     
    return op_smooth_union(mix(sphere, shape, shape_transition), plane, 0.5);
}

vec3 calc_normal(vec3 p)
{
    const float h = 0.0001;
    const vec2 k = vec2(1.0, -1.0);
    return normalize( k.xyy*distance_field(p + k.xyy*h) + 
                      k.yyx*distance_field(p + k.yyx*h) + 
                      k.yxy*distance_field(p + k.yxy*h) + 
                      k.xxx*distance_field(p + k.xxx*h));                     
}

float calc_soft_shadow(vec3 ro, vec3 rd, float mint, float maxt, float k)
{
    float r = 1.0;

    for (float t = mint; t < maxt;) {
        float h = distance_field(rd*t + ro);
        if (h < 0.001) {
            return 0.0;
        }

        r = min(r, k*h/t);
        t += h;
    }
    return r;
}

vec4 shade(vec3 p)
{
    vec3 normal = calc_normal(p);
    float lit = max(0.0, dot(normal, -light.xyz));

    // shadow
    float shadow = calc_soft_shadow(p, -light.xyz, 0.1f, 10.0f, light.w);
    lit *= shadow;

    // back light
    float back_lit = max(0.0, dot(normal, -back_light_dir));
    vec3 lit_color = vec3(lit, lit, lit) + vec3(back_lit, back_lit, back_lit)*bg_color.xyz*0.1;
    
    return vec4(sqrt(lit_color), 1.0);    
}

vec4 raymarch(vec3 ray_dir, vec3 ray_origin)
{
    const int max_iteration = 128;
    const float max_distance = 10.0;
    float t = 0;    

    for (int i = 0; i < max_iteration; i++) {
        if (t > max_distance) {
            return bg_color;
        }

        vec3 p = ray_dir*t + ray_origin;
        float d = distance_field(p);
        if (d < 0.001) {
            return shade(p);
        }

        t += d;
    }

    return bg_color;
}

void main()
{
    vec3 ray_dir = normalize(f_ray);
    vec3 ray_origin = camera_pos.xyz;

    frag_color = raymarch(ray_dir, ray_origin);
}
#include "sx/math-scalar.h"

// clang-format off
static int perm[] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,
    103,30,69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,
    26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,
    87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,
    46,245,40,244,102,143,54, 65,25,63,161,1,216,80,73,209,76,132,
    187,208, 89,18,169,200,196,135,130,116,188,159,86,164,100,109,
    198,173,186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,
    255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,
    170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,
    172,9,129,22,39,253, 19,98,108,110,79,113,224,232,178,185,112,
    104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,
    241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,
    157,184, 84,204,176,115,121,50,45,127, 4,150,254,138,236,205,
    93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,

    // second copy
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,
    103,30,69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,
    26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,
    87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,
    46,245,40,244,102,143,54, 65,25,63,161,1,216,80,73,209,76,132,
    187,208, 89,18,169,200,196,135,130,116,188,159,86,164,100,109,
    198,173,186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,
    255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,
    170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,
    172,9,129,22,39,253, 19,98,108,110,79,113,224,232,178,185,112,
    104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,
    241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,
    157,184, 84,204,176,115,121,50,45,127, 4,150,254,138,236,205,
    93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
};
// clang-format on

static inline float fade(float x)
{
    return x * x * x * (x * (x * 6 - 15) + 10);
}

static inline float grad1(int hash, float x)
{
    return (hash & 1) ? -x : x;
}

static inline float grad2(int hash, float x, float y)
{
    return ((hash & 1) ? -x : x) + ((hash & 2) ? -y : y);
}

static inline float grad3(int hash, float x, float y, float z)
{
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float noise__perlin1d(float x)
{
    float fx = sx_floor(x);
    int hx = ((int)fx & 0xff);
    x -= fx;
    float u = fade(x);
    return sx_lerp(grad1(perm[hx + 0], x), grad1(perm[hx + 1], x - 1), u) * 2;
}

float noise__perlin2d(float x, float y)
{
    float fx = sx_floor(x);
    float fy = sx_floor(y);
    int hx = (int)fx & 0xff;
    int hy = (int)fy & 0xff;
    x -= fx;
    y -= fy;
    float u = fade(x);
    float v = fade(y);
    int a = (perm[hx + 0] + hy) & 0xff;
    int b = (perm[hx + 1] + hy) & 0xff;
    return sx_lerp(sx_lerp(grad2(perm[a + 0], x, y - 0), grad2(perm[b + 0], x - 1, y - 0), u),
                   sx_lerp(grad2(perm[a + 1], x, y - 1), grad2(perm[b + 1], x - 1, y - 1), u), v);
}

float noise__perlin3d(float x, float y, float z)
{
    float fx = sx_floor(x);
    float fy = sx_floor(y);
    float fz = sx_floor(z);
    int hx = (int)fx & 0xff;
    int hy = (int)fy & 0xff;
    int hz = (int)fz & 0xff;
    x -= fx;
    y -= fy;
    z -= fz;
    float u = fade(x);
    float v = fade(y);
    float s = fade(z);
    int a = (perm[hx + 0] + hy) & 0xff;
    int b = (perm[hx + 1] + hy) & 0xff;
    int aa = (perm[a + 0] + hz) & 0xff;
    int ba = (perm[b + 0] + hz) & 0xff;
    int ab = (perm[a + 1] + hz) & 0xff;
    int bb = (perm[b + 1] + hz) & 0xff;
    return sx_lerp(s, 
           sx_lerp(v, sx_lerp(u, grad3(perm[aa + 0], x, y, z),        grad3(perm[ba + 0], x - 1, y, z)),
                      sx_lerp(u, grad3(perm[ab + 0], x, y - 1, z),    grad3(perm[bb + 0], x - 1, y - 1, z))),
           sx_lerp(v, sx_lerp(u, grad3(perm[aa + 1], x, y, z - 1),    grad3(perm[ba + 1], x - 1, y, z - 1)),
                      sx_lerp(u, grad3(perm[ab + 1], x, y - 1, z - 1),grad3(perm[bb + 1], x - 1, y - 1, z - 1))));
}

float noise__perlin1d_fbm(float x, int octave)
{
    float f = 0.0f, w = 0.5f;
    for (int i = 0; i < octave; i++) {
        f += w * noise__perlin1d(x);
        x *= 2.0f;
        w *= 0.5f;
    }
    return f;
}

float noise__perlin2d_fbm(float x, float y, int octave)
{
    float f = 0.0f, w = 0.5f;
    for (int i = 0; i < octave; i++) {
        f += w * noise__perlin2d(x, y);
        x *= 2.0f;
        y *= 2.0f;
        w *= 0.5f;
    }
    return f;
}

float noise__perlin3d_fbm(float x, float y, float z, int octave)
{
    float f = 0.0f, w = 0.5f;
    for (int i = 0; i < octave; i++) {
        f += w * noise__perlin3d(x, y, z);
        x *= 2.0f;
        y *= 2.0f;
        z *= 2.0f;
        w *= 0.5f;
    }
    return f;
}
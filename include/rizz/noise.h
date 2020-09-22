#pragma once

typedef struct rizz_api_noise {
    float (*perlin1d)(float x);
    float (*perlin2d)(float x, float y);
    float (*perlin1d_fbm)(float x, int octave);
    float (*perlin2d_fbm)(float x, float y, int octave);
} rizz_api_noise;
//
// NOTE: audio sources must be all mono. If they are something else they will be downmixed to mono 
//       on load time
//
#pragma once

#include "sx/sx.h"

#ifndef RIZZ_SND_DEVICE_SAMPLE_RATE 
#    define RIZZ_SND_DEVICE_SAMPLE_RATE (44100)
#endif

#ifndef RIZZ_SND_DEVICE_NUM_CHANNELS
#   define RIZZ_SND_DEVICE_NUM_CHANNELS 2
#endif

#ifndef RIZZ_SND_DEVICE_BUFFER_FRAMES 
#   define RIZZ_SND_DEVICE_BUFFER_FRAMES 2048
#endif

typedef struct { uint32_t id; } rizz_snd_source;
typedef struct { uint32_t id; } rizz_snd_instance;

// params for asset manager to pass with "sound" asset-name
typedef struct rizz_snd_load_params {
    float volume;
    bool looping;
    bool singleton;
} rizz_snd_load_params;

typedef struct rizz_api_snd {
    rizz_snd_instance (*play)(rizz_snd_source src, float volume, float pan, bool paused);
    void (*stop)(rizz_snd_instance inst);
    void (*stop_all)();
    
    float (*global_volume)();
    void (*set_global_volume)(float volume);

    void (*source_stop)(rizz_snd_source src);
    bool (*source_looping)(rizz_snd_source src);
    void (*source_set_looping)(rizz_snd_source src);
    void (*source_set_singleton)(rizz_snd_source src);
    void (*source_set_volume)(rizz_snd_source src);

    bool (*test_overrun)();
    int (*frames_remain)();

    void (*update)(float dt);
} rizz_api_snd;
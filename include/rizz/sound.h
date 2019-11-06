//
// RIZZ_SND_DEVICE_MIXER_MAX_LANES: maximum sounds that can be mixed by the mixer
// RIZZ_SND_DEVICE_MAX_BUSES: maximum number of buses, defined by their index (0..(n-1))
//                            each bus can have maximum number of sounds (lanes) that are allowed
//                            to be played at a time.
//                            By default, the first bus (bus=0) has all the lanes available
//                            unless you use `bus_set_max_lanes` API to define maximum sounds for
//                            each of them.
//                            Also, keep in mind that, the total lanes of all buses should not
//                            exceed RIZZ_SND_DEVICE_MAX_LANES
// NOTE: audio sources must be all mono. If they are something else they will be downmixed to mono
//       on load time
//
#pragma once

#define RIZZ_SND_DEVICE_SAMPLE_RATE (44100)
#define RIZZ_SND_DEVICE_NUM_CHANNELS 2
#define RIZZ_SND_DEVICE_BUFFER_FRAMES 2048
#define RIZZ_SND_DEVICE_MAX_LANES 32
#define RIZZ_SND_DEVICE_MAX_BUSES 8

// clang-format off
typedef struct { uint32_t id; } rizz_snd_source;
typedef struct { uint32_t id; } rizz_snd_instance;
// clang-format on

// params for asset manager to pass with "sound" asset-name
typedef struct rizz_snd_load_params {
    float volume;
    bool looping;
    bool singleton;
} rizz_snd_load_params;

// thread-safe queued API
// this api is async and can be used in worker threads
// all calls are queued for execution on sound-system update
typedef struct rizz_api_snd_queue {
    void (*play)(rizz_snd_source src, int bus, float volume, float pan, bool paused);
    void (*play_clocked)(rizz_snd_source src, float wait_tm, int bus, float volume, float pan);
    void (*bus_stop)(int bus);
    void (*set_master_volume)(float volume);
    void (*set_master_pan)(float pan);
    void (*source_stop)(rizz_snd_source src);
    void (*source_set_volume)(rizz_snd_source src, float vol);
} rizz_api_snd_queue;

typedef struct rizz_api_snd {
    rizz_api_snd_queue queued;

    rizz_snd_instance (*play)(rizz_snd_source src, int bus, float volume, float pan, bool paused);
    rizz_snd_instance (*play_clocked)(rizz_snd_source src, float wait_tm, int bus, float volume,
                                      float pan);
    void (*stop)(rizz_snd_instance inst);
    void (*stop_all)();
    void (*resume)(rizz_snd_instance inst);

    void (*bus_set_max_lanes)(int bus, int max_lanes);
    void (*bus_stop)(int bus);

    float (*master_volume)();
    float (*master_pan)();
    void (*set_master_volume)(float volume);
    void (*set_master_pan)(float pan);

    float (*source_duration)(rizz_snd_source src);
    void (*source_stop)(rizz_snd_source src);
    bool (*source_looping)(rizz_snd_source src);
    float (*source_volume)(rizz_snd_source src);
    void (*source_set_looping)(rizz_snd_source src, bool loop);
    void (*source_set_singleton)(rizz_snd_source src, bool singleton);
    void (*source_set_volume)(rizz_snd_source src, float vol);
    rizz_snd_source (*source_get)(rizz_asset snd_asset);

    void (*show_debugger)(bool* p_open);
} rizz_api_snd;
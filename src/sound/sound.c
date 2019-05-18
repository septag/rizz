#include "rizz/sound.h"

#include "rizz/asset.h"
#include "rizz/core.h"
#include "rizz/plugin.h"
#include "rizz/reflect.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/atomic.h"
#include "sx/handle.h"
#include "sx/math.h"
#include "sx/string.h"
#include "sx/timer.h"

#define FIXPOINT_FRAC_BITS 20
#define FIXPOINT_FRAC_MUL (1 << FIXPOINT_FRAC_BITS)
#define FIXPOINT_FRAC_MASK ((1 << FIXPOINT_FRAC_BITS) - 1)

RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_core*   the_core;
RIZZ_STATE static rizz_api_asset*  the_asset;
RIZZ_STATE static rizz_api_refl*   the_refl;

RIZZ_STATE static const sx_alloc* g_snd_alloc;

#define DR_WAV_IMPLEMENTATION
#define DRWAV_ASSERT(e) sx_assert(e)
#define DRWAV_MALLOC(sz) sx_malloc(g_snd_alloc, sz)
#define DRWAV_REALLOC(p, sz) sx_realloc(g_snd_alloc, p, sz)
#define DRWAV_FREE(p) sx_free(g_snd_alloc, p)
#define DRWAV_COPY_MEMORY(dst, src, sz) sx_memcpy(dst, src, sz)
#define DRWAV_ZERO_MEMORY(p, sz) sx_memset(p, 0, sz)
#define DR_WAV_NO_STDIO
#include "dr_libs/dr_wav.h"

#define SOKOL_ASSERT(c) sx_assert(c)
#define SOKOL_LOG(msg) rizz_log_error(the_core, msg);
#define SOKOL_MALLOC(s) sx_malloc(g_snd_alloc, s)
#define SOKOL_FREE(p) sx_free(g_snd_alloc, p)
#define SOKOL_API_DECL static
#define SOKOL_API_IMPL static
#define SOKOL_IMPL
#include "sokol/sokol_audio.h"

typedef enum snd__source_flags_ {
    SND_SOURCEFLAG_LOOPING = 0x1,
    SND_SOURCEFLAG_SINGLETON = 0x2
} snd__source_flags_;
typedef uint32_t snd__source_flags;

typedef struct snd__metadata {
    int num_frames;
    int num_channels;
    int bps;
} snd__metadata;

typedef struct snd__source {
    float*            samples;
    int               num_frames;
    int               sample_rate;
    snd__source_flags flags;
} snd__source;

typedef struct snd__instance {
    rizz_snd_source srchandle;
    int             pos;    // unit: frame
} snd__instance;

typedef struct snd__ringbuffer {
    float*        samples;
    int           capacity;
    int           start;
    int           end;
    sx_atomic_int size;
} snd__ringbuffer;

typedef struct snd__context {
    sx_handle_pool*    source_handles;
    snd__source*       sources;
    sx_handle_pool*    instance_handles;
    snd__instance*     instances;
    rizz_snd_instance* playlist;
    snd__ringbuffer    mixer_buffer;
} snd__context;

RIZZ_STATE static snd__context g_snd;

////////////////////////////////////////////////////////////////////////////////////////////////////
// SpSc thread-safe ring-buffer
// NOTE: this ring-buffer shouldn't be atomic because two threads are writing and using `size`
//       variable, but it should be 'safe', because consumer thread is always shrinking the size
//       and producer thread is expanding. which doesn't make the ring-buffer to overflow.
//       But again, more tests are needed, because obviously I'm not doing the right thing in terms
//       of thread-safetly
static bool snd__ringbuffer_init(snd__ringbuffer* rb, int size) {
    sx_memset(rb, 0x0, sizeof(*rb));

    rb->samples = sx_malloc(g_snd_alloc, size * sizeof(float));
    if (!rb->samples) {
        sx_out_of_memory();
        return false;
    }
    rb->capacity = size;
    rb->size = 0;
    rb->start = rb->end = 0;

    return true;
}

static void snd__ringbuffer_release(snd__ringbuffer* rb) {
    sx_assert(rb);
    sx_free(g_snd_alloc, rb->samples);
}

static inline int snd__ringbuffer_expect(snd__ringbuffer* rb) {
    int r = rb->capacity - rb->size;
    return r;
}

static int snd__ringbuffer_consume(snd__ringbuffer* rb, float* samples, int count) {
    sx_assert(count > 0);

    count = sx_min(count, rb->size);
    if (count == 0) {
        return 0;
    }
    sx_atomic_fetch_add(&rb->size, -count);

    int remain = rb->capacity - rb->start;
    if (remain >= count) {
        sx_memcpy(samples, &rb->samples[rb->start], count * sizeof(float));
    } else {
        sx_memcpy(samples, &rb->samples[rb->start], remain * sizeof(float));
        sx_memcpy(samples, rb->samples, (count - remain) * sizeof(float));
    }

    rb->start = (rb->start + count) % rb->capacity;

    return count;
}

static void snd__ringbuffer_produce(snd__ringbuffer* rb, const float* samples, int count) {
    sx_assert(count > 0);
    sx_assert(count <= snd__ringbuffer_expect(rb));

    // sx_lock(&rb->lock);

    int remain = rb->capacity - rb->end;
    if (remain >= count) {
        sx_memcpy(&rb->samples[rb->end], samples, count * sizeof(float));
    } else {
        sx_memcpy(&rb->samples[rb->end], samples, remain * sizeof(float));
        sx_memcpy(rb->samples, &samples[remain], (count - remain) * sizeof(float));
    }

    rb->end = (rb->end + count) % rb->capacity;
    sx_atomic_fetch_add(&rb->size, count);

    // sx_unlock(&rb->lock);
}

static void snd__destroy_source(rizz_snd_source handle, const sx_alloc* alloc) {
    sx_assert(sx_handle_valid(g_snd.source_handles, handle.id));

    snd__source* src = &g_snd.sources[sx_handle_index(handle.id)];
    sx_free(alloc, src->samples);

    sx_handle_del(g_snd.source_handles, handle.id);
}

static rizz_asset_load_data snd__on_prepare(const rizz_asset_load_params* params,
                                            const void*                   metadata) {
    const snd__metadata*        meta = metadata;
    const sx_alloc*             alloc = params->alloc ? params->alloc : g_snd_alloc;
    const rizz_snd_load_params* sparams = params->params;

    // create sound source and allocate buffer needed to be filled with sound data
    sx_assert(meta->num_frames > 0);
    float* samples = sx_malloc(alloc, meta->num_frames * sizeof(float));
    if (!samples) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ 0 };
    }

    sx_handle_t handle = sx_handle_new_and_grow(g_snd.source_handles, g_snd_alloc);
    if (!handle) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ 0 };
    }

    snd__source src = { .samples = samples, .num_frames = meta->num_frames };
    if (sparams->looping)
        src.flags |= SND_SOURCEFLAG_LOOPING;
    if (sparams->singleton)
        src.flags |= SND_SOURCEFLAG_SINGLETON;

    int index = sx_handle_index(handle);
    if (index >= sx_array_count(g_snd.sources)) {
        sx_array_push(g_snd_alloc, g_snd.sources, src);
    } else {
        g_snd.sources[index] = src;
    }

    return (rizz_asset_load_data){ .obj = { .id = handle } };
}

static bool snd__on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                         const sx_mem_block* mem) {
    rizz_snd_source srchandle = { .id = (uint32_t)data->obj.id };
    sx_assert(sx_handle_valid(g_snd.source_handles, srchandle.id));

    const sx_alloc* alloc = params->alloc ? params->alloc : g_snd_alloc;

    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    drwav        wav;

    if (!drwav_init_memory(&wav, mem->data, mem->size)) {
        rizz_log_warn(the_core, "loading sound '%s' failed: invalid WAV format", params->path);
        snd__destroy_source(srchandle, alloc);
        return false;
    }

    src->sample_rate = wav.sampleRate;
    float* samples;
    if (wav.channels == 1) {
        samples = src->samples;
    } else {
        const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
        samples = sx_malloc(tmp_alloc, sizeof(float) * wav.totalPCMFrameCount * wav.channels);
        if (!samples) {
            sx_out_of_memory();
            return false;
        }
    }

    uint64_t num_frames = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples);
    if (num_frames != wav.totalPCMFrameCount) {
        rizz_log_warn(the_core, "loading sound '%s' failed: reached EOF");
        snd__destroy_source(srchandle, alloc);
        return false;
    }

    // downmix multiple channels to mono
    if (wav.channels > 1) {
        float  channels_rcp = 1.0f / (float)wav.channels;
        float* dst_samples = src->samples;
        for (int i = 0; i < wav.totalPCMFrameCount; i++) {
            float sum = 0;
            for (int ch = 0; ch < wav.channels; ch++) sum += samples[ch];

            samples += wav.channels;
            dst_samples[i] = sum * channels_rcp;
        }

        the_core->tmp_alloc_pop();
    }

    drwav_uninit(&wav);
    return true;
}

static void snd__on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                             const sx_mem_block* mem) {
    sx_unused(data);
    sx_unused(params);
    sx_unused(mem);
}

static void snd__on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc) {
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
}

static void snd__on_release(rizz_asset_obj obj, const sx_alloc* alloc) {
    rizz_snd_source srchandle = { .id = (uint32_t)obj.id };
    snd__destroy_source(srchandle, alloc ? alloc : g_snd_alloc);
}

static void snd__on_read_metadata(void* metadata, const rizz_asset_load_params* params,
                                  const sx_mem_block* mem) {
    snd__metadata* meta = metadata;
    drwav          wav;
    if (!drwav_init_memory(&wav, mem->data, mem->size)) {
        rizz_log_warn(the_core, "loading sound '%s' failed", params->path);
        return;
    }

    sx_assert(wav.totalPCMFrameCount < INT_MAX);
    meta->num_frames = (int)wav.totalPCMFrameCount;
    meta->num_channels = wav.channels;
    meta->bps = wav.bitsPerSample;

    drwav_uninit(&wav);
}

static void snd__stream_cb(float* buffer, int num_frames, int num_channels) {
    int r = snd__ringbuffer_consume(&g_snd.mixer_buffer, buffer, num_frames * num_channels) /
            num_channels;

    if (r < num_frames) {
        sx_memset(buffer + r * num_channels, 0x0, (num_frames - r) * num_channels * sizeof(float));
    }
}

static bool snd__init() {
    g_snd_alloc = the_core->alloc(RIZZ_MEMID_AUDIO);

    g_snd.source_handles = sx_handle_create_pool(g_snd_alloc, 128);
    g_snd.instance_handles = sx_handle_create_pool(g_snd_alloc, 256);

    if (!g_snd.source_handles || !g_snd.instance_handles) {
        sx_out_of_memory();
        return false;
    }

    int mixer_buffer_size = RIZZ_SND_DEVICE_NUM_CHANNELS * RIZZ_SND_DEVICE_BUFFER_FRAMES * 2;
    if (!snd__ringbuffer_init(&g_snd.mixer_buffer, mixer_buffer_size)) {
        sx_out_of_memory();
        return false;
    }

    saudio_setup(&(saudio_desc){ .sample_rate = RIZZ_SND_DEVICE_SAMPLE_RATE,
                                 .num_channels = RIZZ_SND_DEVICE_NUM_CHANNELS,
                                 .stream_cb = snd__stream_cb,
                                 .buffer_frames = RIZZ_SND_DEVICE_BUFFER_FRAMES,
                                 .num_packets = 32 });


    // register sound resources for asset loader
    rizz_refl_field(the_refl, snd__metadata, int, num_frames, "num_frames");
    rizz_refl_field(the_refl, snd__metadata, int, num_channels, "num_channels");
    rizz_refl_field(the_refl, snd__metadata, int, bps, "bits-per-sample");

    the_asset->register_asset_type(
        "sound",
        (rizz_asset_callbacks){ .on_prepare = snd__on_prepare,
                                .on_load = snd__on_load,
                                .on_finalize = snd__on_finalize,
                                .on_reload = snd__on_reload,
                                .on_release = snd__on_release,
                                .on_read_metadata = snd__on_read_metadata },
        "rizz_snd_load_params", sizeof(rizz_snd_load_params), "snd__metadata",
        sizeof(snd__metadata), (rizz_asset_obj){ .id = 0 }, (rizz_asset_obj){ .id = 0 }, 0);

    return true;
}

static void snd__release() {
    saudio_shutdown();

    if (g_snd.source_handles) {
        if (g_snd.source_handles->count > 0) {
            rizz_log_warn(the_core, "total %d sound_sources are not released",
                          g_snd.source_handles->count);
        }
        sx_handle_destroy_pool(g_snd.source_handles, g_snd_alloc);
    }

    if (g_snd.instance_handles) {
        if (g_snd.instance_handles->count > 0) {
            rizz_log_warn(the_core, "total %d sound_instances are not released",
                          g_snd.instance_handles->count);
        }
        sx_handle_destroy_pool(g_snd.instance_handles, g_snd_alloc);
    }

    sx_array_free(g_snd_alloc, g_snd.sources);
    sx_array_free(g_snd_alloc, g_snd.instances);

    snd__ringbuffer_release(&g_snd.mixer_buffer);

    the_asset->unregister_asset_type("sound");
}

static inline rizz_snd_instance snd__create_instance(rizz_snd_source srchandle) {
    sx_assert(sx_handle_valid(g_snd.source_handles, srchandle.id));
    sx_handle_t handle = sx_handle_new_and_grow(g_snd.instance_handles, g_snd_alloc);

    snd__instance inst = (snd__instance){ .srchandle = srchandle };

    int index = sx_handle_index(handle);
    if (index >= sx_array_count(g_snd.instances)) {
        sx_array_push(g_snd_alloc, g_snd.instances, inst);
    } else {
        g_snd.instances[index] = inst;
    }

    return (rizz_snd_instance){ handle };
}

static rizz_snd_instance snd__play(rizz_snd_source srchandle, float volume, float pan,
                                   bool paused) {
    rizz_snd_instance insthandle = snd__create_instance(srchandle);
    sx_array_push(g_snd_alloc, g_snd.playlist, insthandle);
    return insthandle;
}

// TODO: research on fixed-point math, in order to implement linear sampling
static int snd__resample_point(float* dst, int num_dst_samples, int sample_offset, const float* src,
                               float src_sample_rate, float dst_sample_rate) {
    float    multiplier = dst_sample_rate / src_sample_rate;
    uint32_t step_fixed = (uint32_t)sx_floor(FIXPOINT_FRAC_MUL / multiplier);
    uint64_t pos = ((uint64_t)sample_offset << FIXPOINT_FRAC_BITS);

    for (int i = 0; i < num_dst_samples; i++, pos += step_fixed) {
        int p = (int)(pos >> FIXPOINT_FRAC_BITS);
        dst[i] = src[p];
    }

    int new_pos = (int)(pos >> FIXPOINT_FRAC_BITS);
    return new_pos != sample_offset ? new_pos : sample_offset + num_dst_samples;
}

static void snd__mix(float* dst, int dst_num_frames, int dst_num_channels, int dst_sample_rate) {
    float dst_sample_ratef = (float)dst_sample_rate;
    int   frames_written = 0;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    for (int i = 0; i < sx_array_count(g_snd.playlist); i++) {
        rizz_snd_instance insthandle = g_snd.playlist[i];
        sx_assert(sx_handle_valid(g_snd.instance_handles, insthandle.id));
        snd__instance*  inst = &g_snd.instances[sx_handle_index(insthandle.id)];
        rizz_snd_source srchandle = inst->srchandle;
        sx_assert(sx_handle_valid(g_snd.source_handles, srchandle.id));
        snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];

        int    pos = inst->pos;
        int    src_frames_remain = src->num_frames - pos;
        int    num_frames = sx_min(dst_num_frames, src_frames_remain);
        int    num_samples = num_frames;    // mono
        float* frames;

        if (src->sample_rate != dst_sample_rate) {
            // resample and upmix to device_channels
            frames = sx_malloc(tmp_alloc, sizeof(float) * num_samples);
            sx_assert(frames);

            inst->pos = snd__resample_point(frames, num_samples, inst->pos, src->samples,
                                            (float)src->sample_rate, dst_sample_ratef);
        } else {
            // the source and device sample format is the same. do nothing
            frames = src->samples + inst->pos;
            inst->pos += num_frames;
        }

        // remove sound if EOF / loop
        if (inst->pos >= src->num_frames) {
            inst->pos = 0;

            if (!(src->flags & SND_SOURCEFLAG_LOOPING)) {
                sx_array_pop(g_snd.playlist, i);
                --i;
            }
        }

        // add to destination buffer. upmix to device_channels if required
        int dst_num_samples = num_frames * dst_num_channels;
        for (int s = 0, p = 0; s < dst_num_samples; s += dst_num_channels, p++) {
            for (int ch = 0; ch < dst_num_channels; ch++) {
                dst[s + ch] += frames[p];
            }
        }

        frames_written = sx_max(frames_written, num_frames);
    }
    the_core->tmp_alloc_pop();

    // fill remaining frame with zeros
    if (frames_written < dst_num_frames) {
        sx_memset(dst + frames_written * dst_num_channels, 0x0,
                  (dst_num_frames - frames_written) * dst_num_channels * sizeof(float));
    }
}

int g_frames_remain;

static void snd__update(float dt) {
    int device_channels = saudio_channels();
    int device_sample_rate = saudio_sample_rate();

    int frames_remain = snd__ringbuffer_expect(&g_snd.mixer_buffer) / device_channels;
    int frames_needed = (int)(dt * (float)device_sample_rate * 1.5f);
    int s = (g_snd.mixer_buffer.capacity / device_channels) - frames_remain;
    if ((frames_needed + s) < RIZZ_SND_DEVICE_BUFFER_FRAMES) {
        frames_needed = RIZZ_SND_DEVICE_BUFFER_FRAMES;
    }

    int tmp = frames_remain;
    frames_remain = frames_needed != 0 ? sx_min(frames_remain, frames_needed) : frames_remain;

    g_frames_remain = frames_remain;

    if (frames_remain) {
        const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();

        float* frames = sx_malloc(tmp_alloc, sizeof(float) * frames_remain * device_channels);
        if (!frames) {
            sx_out_of_memory();
            return;
        }
        sx_memset(frames, 0x0, sizeof(float) * frames_remain * device_channels);
        snd__mix(frames, frames_remain, device_channels, device_sample_rate);
        snd__ringbuffer_produce(&g_snd.mixer_buffer, frames, frames_remain * device_channels);

        the_core->tmp_alloc_pop();
    }
}

static bool snd__test_overrun() {
    return _saudio.overrun == 1;
}

static int snd__frames_remain() {
    return g_frames_remain;
}

static rizz_api_snd the__snd = { .play = snd__play,
                                 .test_overrun = snd__test_overrun,
                                 .frames_remain = snd__frames_remain,
                                 .update = snd__update };

rizz_plugin_decl_main(sound, plugin, e) {
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        snd__update((float)sx_tm_sec(the_core->delta_tick()));
        break;
    }
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = the_plugin->get_api(RIZZ_API_CORE, 0);
        the_asset = the_plugin->get_api(RIZZ_API_ASSET, 0);
        the_refl = the_plugin->get_api(RIZZ_API_REFLECT, 0);

        if (!snd__init()) {
            return -1;
        }

        the_plugin->inject_api("sound", 0, &the__snd);
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("sound", 0, &the__snd);
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("sound", 0);
        snd__release();
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(sound, e) {}

rizz_plugin_implement_info(sound, 1000, "sound plugin", NULL, 0);
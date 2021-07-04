#include "rizz/rizz.h"
#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"

#include "rizz/sound.h"

#include "sx/array.h"
#include "sx/atomic.h"
#include "sx/handle.h"
#include "sx/math-scalar.h"
#include "sx/os.h"
#include "sx/pool.h"
#include "sx/string.h"
#include "sx/timer.h"

#include "beep.h"

#include <float.h>
#if SX_COMPILER_CLANG  
#include <assert.h>  // static_assert ?! 
#endif

#include "stb/stb_vorbis.h"

#define FIXPOINT_FRAC_BITS 20
#define FIXPOINT_FRAC_MUL (1 << FIXPOINT_FRAC_BITS)
#define FIXPOINT_FRAC_MASK ((1 << FIXPOINT_FRAC_BITS) - 1)

RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_core* the_core; 
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_refl* the_refl;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;

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
#define SOKOL_LOG(msg) rizz_log_error(msg);
#define SOKOL_MALLOC(s) sx_malloc(g_snd_alloc, s)
#define SOKOL_FREE(p) sx_free(g_snd_alloc, p)
#define SOKOL_API_DECL static
#define SOKOL_API_IMPL static
#define SOKOL_IMPL
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wsign-compare")
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)
#include "sokol/sokol_audio.h"
SX_PRAGMA_DIAGNOSTIC_POP()

typedef enum snd__source_flags_ {
    SND_SOURCEFLAG_LOOPING = 0x1,
    SND_SOURCEFLAG_SINGLETON = 0x2
} snd__source_flags_;
typedef uint32_t snd__source_flags;

typedef enum snd__instance_state {
    SND_INSTANCESTATE_NONE = 0,
    SND_INSTANCESTATE_PLAYING
} snd__instance_state;

typedef enum snd__source_format {
    SND_SOURCEFORMAT_WAV = 0,
    SND_SOURCEFORMAT_OGG
} snd__source_format;

typedef struct snd__source {
    void* data;
    float* samples;
    int num_frames;
    int sample_rate;
    snd__source_flags flags;
    snd__source_format fmt;
    float volume;
    sx_str_t name;    // for debugging purposes
    int num_plays;
} snd__source;

typedef struct snd__instance {
    int64_t play_frame;
    rizz_snd_source srchandle;
    int pos;    // unit: frame
    float volume;
    float pan;
    int bus_id;
    snd__instance_state state;
} snd__instance;

typedef struct snd__ringbuffer {
    float* samples;
    int capacity;
    int start;
    int end;
    sx_atomic_int size;
} snd__ringbuffer;

typedef struct snd__bus {
    int max_lanes;
    int num_lanes;
} snd__bus;

typedef struct snd__clocked {
    struct snd__clocked* next;    // next item to be queued for play
    rizz_snd_source src;
    rizz_snd_instance inst;
    int bus_id;
    float tm;
    float wait_tm;
    bool first;
} snd__clocked;

typedef enum snd__command {
    SND_COMMAND_PLAY = 0,
    SND_COMMAND_PLAY_CLOCKED,
    SND_COMMAND_BUS_STOP,
    SND_COMMAND_SET_MASTER_VOLUME,
    SND_COMMAND_SET_MASTER_PAN,
    SND_COMMAND_SOURCE_STOP,
    SND_COMMAND_SOURCE_SET_VOLUME,
    _SND_COMMAND_COUNT,
    _SND_COMMAND_ = INT32_MAX
} snd__command;

typedef struct snd__cmdheader {
    snd__command cmd;
    int cmdbuffer_idx;
    int params_offset;
} snd__cmdheader;

typedef struct snd__cmdbuffer {
    uint8_t* params_buff;    // sx_array
    snd__cmdheader* cmds;    // sx_array
    int index;
    int cmd_idx;
} snd__cmdbuffer;

typedef uint8_t* (*snd__run_command_cb)(uint8_t* buff);

typedef struct snd__context {
    snd__cmdbuffer** cmd_buffers;
    sx_handle_pool* source_handles;
    snd__source* sources;
    sx_handle_pool* instance_handles;
    snd__instance* instances;
    sx_strpool* name_pool;
    sx_pool* clocked_pool;
    snd__clocked** clocked;
    int num_cmdbuffers;
    rizz_snd_instance playlist[RIZZ_SND_DEVICE_MAX_LANES];
    int num_plays;
    snd__ringbuffer mixer_buffer;
    float master_volume;
    float master_pan;
    snd__ringbuffer mixer_plot_buffer;
    snd__bus buses[RIZZ_SND_DEVICE_MAX_BUSES];
    rizz_snd_source silence_src;
    rizz_snd_source beep_src;
} snd__context;

RIZZ_STATE static snd__context g_snd;

static char k__snd_silent[] = { 0, 0, 0, 0 };

////////////////////////////////////////////////////////////////////////////////////////////////////
// SpSc thread-safe ring-buffer
// NOTE: this ring-buffer shouldn't be atomic because two threads are writing and using `size`
//       variable, but it should be 'safe', because consumer thread is always shrinking the size
//       and producer thread is expanding it.
//       Which doesn't make the ring-buffer to overflow due to 'size' being always sufficient (or
//       more). It just may make it not very optimal.
//       But again, more tests are needed, because obviously I'm not doing the right thing in
//       terms of thread-safetly
static bool snd__ringbuffer_init(snd__ringbuffer* rb, int size)
{
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

static void snd__ringbuffer_release(snd__ringbuffer* rb)
{
    sx_assert(rb);
    sx_free(g_snd_alloc, rb->samples);
}

static inline int snd__ringbuffer_expect(snd__ringbuffer* rb)
{
    return rb->capacity - rb->size;
}

static int snd__ringbuffer_consume(snd__ringbuffer* rb, float* samples, int count)
{
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
        sx_memcpy(&samples[remain], rb->samples, (count - remain) * sizeof(float));
    }

    rb->start = (rb->start + count) % rb->capacity;

    return count;
}

static void snd__ringbuffer_produce(snd__ringbuffer* rb, const float* samples, int count)
{
    sx_assert(count > 0);
    sx_assert(count <= snd__ringbuffer_expect(rb));

    int remain = rb->capacity - rb->end;
    if (remain >= count) {
        sx_memcpy(&rb->samples[rb->end], samples, count * sizeof(float));
    } else {
        sx_memcpy(&rb->samples[rb->end], samples, remain * sizeof(float));
        sx_memcpy(rb->samples, &samples[remain], (count - remain) * sizeof(float));
    }

    rb->end = (rb->end + count) % rb->capacity;
    sx_atomic_fetch_add(&rb->size, count);
}


static void snd__destroy_source(rizz_snd_source handle, const sx_alloc* alloc)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, handle.id));

    snd__source* src = &g_snd.sources[sx_handle_index(handle.id)];

    sx_free(alloc, src->data);

    if (src->name) {
        sx_strpool_del(g_snd.name_pool, src->name);
    }

    sx_handle_del(g_snd.source_handles, handle.id);

    // TODO: delete all instances with this source
}

static const char* snd__vorbis_get_error(int error)
{
    switch (error) {
    case VORBIS_need_more_data:
        return "need_more_data";
    case VORBIS_invalid_api_mixing:
        return "invalid_api_mixing";
    case VORBIS_outofmem:
        return "out_of_mem";
    case VORBIS_feature_not_supported:
        return "feature_not_supported";
    case VORBIS_too_many_channels:
        return "too_many_channels";
    case VORBIS_file_open_failure:
        return "file_open_failure";
    case VORBIS_seek_without_length:
        return "seek_without_length";
    case VORBIS_unexpected_eof:
        return "unexpected_eof";
    case VORBIS_seek_invalid:
        return "seek_invalid";
    case VORBIS_invalid_setup:
        return "invalid_setup";
    case VORBIS_invalid_stream:
        return "invalid_stream";
    case VORBIS_missing_capture_pattern:
        return "missing_capture_pattern";
    case VORBIS_invalid_stream_structure_version:
        return "invalid_stream_structure_version";
    case VORBIS_continued_packet_flag_invalid:
        return "continued_packet_flag_invalid";
    case VORBIS_incorrect_stream_serial_number:
        return "incorrect_stream_serial_number";
    case VORBIS_invalid_first_page:
        return "invaid_first_page";
    case VORBIS_bad_packet_type:
        return "bad_packet_type";
    case VORBIS_cant_find_last_page:
        return "cant_find_last_page";
    case VORBIS_seek_failed:
        return "seek_failed";
    case VORBIS_ogg_skeleton_not_supported:
        return "ogg_skeleton_not_supported";
    default:
        return "";
    }
}

static rizz_asset_load_data snd__on_prepare(const rizz_asset_load_params* params,
                                            const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_snd_alloc;
    const rizz_snd_load_params* sparams = params->params;

    drwav wav;
    char ext[32];
    snd__source_format fmt;
    uint32_t vorbis_buffer_size;
    int num_frames;

    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, ".wav")) {
        if (!drwav_init_memory(&wav, mem->data, (size_t)mem->size)) {
            rizz_log_warn("loading sound '%s' failed", params->path);
            return (rizz_asset_load_data){ {0} };
        }

        sx_assert(wav.totalPCMFrameCount < INT_MAX);    // big wav files are not supported
        fmt = SND_SOURCEFORMAT_WAV;
        num_frames = (int)wav.totalPCMFrameCount;
        vorbis_buffer_size = 0;

        drwav_uninit(&wav);
    } else if (sx_strequalnocase(ext, ".ogg")) {
        int error;
        stb_vorbis* vorbis = stb_vorbis_open_memory(mem->data, (int)mem->size, &error, NULL);
        if (!vorbis) {
            rizz_log_warn("loading sound '%s' failed: %s", snd__vorbis_get_error(error));
            return (rizz_asset_load_data){ {0} };
        }
        stb_vorbis_info info = stb_vorbis_get_info(vorbis);

        fmt = SND_SOURCEFORMAT_OGG;
        num_frames = stb_vorbis_stream_length_in_samples(vorbis) / info.channels;
        vorbis_buffer_size = info.setup_memory_required + info.setup_temp_memory_required +
                                   info.temp_memory_required;
        stb_vorbis_close(vorbis);
    } else {
        sx_assertf(0, "file format not supported");
        return (rizz_asset_load_data){ {0} };
    }

    sx_handle_t handle = sx_handle_new_and_grow(g_snd.source_handles, g_snd_alloc);
    if (!handle) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ { 0 } };
    }

    snd__source_flags flags = (sparams->looping ? SND_SOURCEFLAG_LOOPING : 0) |
                              (sparams->singleton ? SND_SOURCEFLAG_SINGLETON : 0);
    snd__source src = { .num_frames = num_frames,
                        .volume = 1.0f,
                        .name =
                            sx_strpool_add(g_snd.name_pool, params->path, sx_strlen(params->path)),
                        .flags = flags,
                        .fmt = fmt };

    sx_array_push_byindex(g_snd_alloc, g_snd.sources, src, sx_handle_index(handle));

    // allocate memory for source data + samples and extra int for vorbis_buffer_size
    int samples_sz = src.num_frames * sizeof(float);    // channels=1
    int total_sz = sizeof(int) + samples_sz + sizeof(snd__source) + 16;

    void* data = sx_malloc(alloc, total_sz);
    if (!data) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ { 0 } };
    }
    src.data = data;
    uint8_t* buff = (uint8_t*)data;
    *((snd__source*)buff) = src;
    buff += sizeof(snd__source);

    if (fmt == SND_SOURCEFORMAT_OGG) {
        // pass temp vorbis_buffer_size to loader
        *((int*)buff) = vorbis_buffer_size;
        buff += sizeof(int);
    }

    return (rizz_asset_load_data){ .obj = { .id = handle }, .user1 = data };
}

static bool snd__on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                         const sx_mem_block* mem)
{
    rizz_snd_source srchandle = { .id = (uint32_t)data->obj.id };
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));

    const sx_alloc* alloc = params->alloc ? params->alloc : g_snd_alloc;

    uint8_t* buff = (uint8_t*)data->user1;
    snd__source* src = (snd__source*)buff;
    buff += sizeof(snd__source);

    if (src->fmt == SND_SOURCEFORMAT_WAV) {
        drwav wav;
        if (!drwav_init_memory(&wav, mem->data, (size_t)mem->size)) {
            rizz_log_warn("loading sound '%s' failed: invalid WAV format", params->path);
            snd__destroy_source(srchandle, alloc);
            return false;
        }
        sx_assert(wav.totalPCMFrameCount < INT_MAX);    // big wav files are not supported

        src->sample_rate = wav.sampleRate;
        float* samples = sx_align_ptr(buff, 0, 16);
        float* wav_samples = NULL;
        const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
        sx_scope(the_core->tmp_alloc_pop()) {
            if (wav.channels > 1) {
                wav_samples =
                    sx_malloc(tmp_alloc, sizeof(float) * (size_t)wav.totalPCMFrameCount * wav.channels);
                if (!wav_samples) {
                    sx_out_of_memory();
                    sx_assert_always(0);
                }
            }

            uint64_t num_frames = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples);
            if (num_frames != wav.totalPCMFrameCount) {
                rizz_log_warn("loading sound '%s' failed: reached EOF", params->path);
                snd__destroy_source(srchandle, alloc);
                return false;
            }

            // down-mix multiple channels to mono
            if (wav.channels > 1) {
                sx_assert(wav_samples);
                float channels_rcp = 1.0f / (float)wav.channels;
                for (int i = 0, c = (int)wav.totalPCMFrameCount; i < c; i++) {
                    float sum = 0;
                    for (int ch = 0; ch < wav.channels; ch++) {
                        sum += wav_samples[ch];
                    }

                    wav_samples += wav.channels;
                    samples[i] = sum * channels_rcp;
                }

            }
        } // scope

        drwav_uninit(&wav);
        src->samples = samples;
    } else if (src->fmt == SND_SOURCEFORMAT_OGG) {
        // TODO: possible bug with tmp_alloc or vorbis. I saw some random crashes, until I put
        //       the tmp_buff = sx_malloc right next to vorbis_buff
        int vorbis_err;
        const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
        sx_scope(the_core->tmp_alloc_pop()) {
            int vorbis_buffer_size = *((int*)buff);
            buff += sizeof(int);

            sx_assert(vorbis_buffer_size > 0);
            void* vorbis_buff = sx_malloc(tmp_alloc, vorbis_buffer_size);
            float* tmp_buff = sx_malloc(tmp_alloc, sizeof(float) * 4096);

            stb_vorbis* vorbis =
                stb_vorbis_open_memory(mem->data, (int)mem->size, &vorbis_err,
                                       &(stb_vorbis_alloc){
                                           .alloc_buffer = vorbis_buff,
                                           .alloc_buffer_length_in_bytes = vorbis_buffer_size,
                                       });
            if (!vorbis) {
                rizz_log_warn("loading sound '%s' failed: %s", params->path,
                              snd__vorbis_get_error(vorbis_err));
                snd__destroy_source(srchandle, alloc);
                the_core->tmp_alloc_pop();
            }

            float* samples = sx_align_ptr(buff, 0, 16);
            float* dst_samples = samples;

            while (1) {
                int n = stb_vorbis_get_samples_float_interleaved(vorbis, 1, tmp_buff, 4096);
                if (n == 0) {
                    break;
                }
                sx_memcpy(dst_samples, tmp_buff, n * sizeof(float));
                dst_samples += n;
            }
            src->sample_rate = stb_vorbis_get_info(vorbis).sample_rate;
            src->samples = samples;

            stb_vorbis_close(vorbis);
        } // scope
    }

    return true;
}

static void snd__on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                             const sx_mem_block* mem)
{
    sx_unused(data);
    sx_unused(params);
    sx_unused(mem);

    uint8_t* buff = (uint8_t*)data->user1;
    snd__source* src = (snd__source*)buff;
    sx_handle_t srchandle = (sx_handle_t)data->obj.id;
    g_snd.sources[sx_handle_index(srchandle)] = *src;
}

static void snd__on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
}

static void snd__on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz_snd_source srchandle = { .id = (uint32_t)obj.id };
    snd__destroy_source(srchandle, alloc ? alloc : g_snd_alloc);
}

static void snd__stream_cb(float* buffer, int num_frames, int num_channels)
{
    int r = snd__ringbuffer_consume(&g_snd.mixer_buffer, buffer, num_frames * num_channels) /
            num_channels;

    if (r < num_frames) {
        sx_memset(buffer + r * num_channels, 0x0, (num_frames - r) * num_channels * sizeof(float));
    }

    if (the_imgui) {
        int num_expected = snd__ringbuffer_expect(&g_snd.mixer_plot_buffer);
        int num_push_samples = sx_min(num_expected, num_frames * num_channels);
        if (num_push_samples > 0) {
            snd__ringbuffer_produce(&g_snd.mixer_plot_buffer, buffer, num_push_samples);
        }
    }
}

static rizz_snd_source snd__create_dummy_source(const char* samples, int num_samples,
                                                const char* name)
{
    // convert to float
    float* fsamples = sx_malloc(g_snd_alloc, sizeof(float) * num_samples);
    if (!fsamples) {
        sx_out_of_memory();
        return (rizz_snd_source){ 0 };
    }

    for (int i = 0; i < num_samples; i++) {
        fsamples[i] = (float)samples[i] / 128.0f;
    }

    snd__source src =
        (snd__source){ .samples = fsamples,
                       .num_frames = num_samples,
                       .sample_rate = 22050,
                       .volume = 1.0f,
                       .name = sx_strpool_add(g_snd.name_pool, name, sx_strlen(name)) };
    sx_handle_t handle = sx_handle_new_and_grow(g_snd.source_handles, g_snd_alloc);
    if (!handle) {
        sx_out_of_memory();
        return (rizz_snd_source){ 0 };
    }

    sx_array_push_byindex(g_snd_alloc, g_snd.sources, src, sx_handle_index(handle));
    return (rizz_snd_source){ handle };
}

static void snd__destroy_dummy_source(rizz_snd_source srchandle)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    if (src->samples) {
        sx_free(g_snd_alloc, src->samples);
    }
}

static void* snd__cmdbuffer_init(int thread_index, uint32_t thread_id, void* user)
{
    sx_unused(thread_id);
    sx_unused(user);
    sx_assert(!g_snd.cmd_buffers[thread_index]);

    snd__cmdbuffer* cb = sx_malloc(g_snd_alloc, sizeof(snd__cmdbuffer));
    if (!cb) {
        sx_out_of_memory();
        return NULL;
    }

    *cb = (snd__cmdbuffer){ .index = thread_index };
    g_snd.cmd_buffers[thread_index] = cb;

    return cb;
}

static bool snd__init()
{
    static_assert(RIZZ_SND_DEVICE_NUM_CHANNELS == 1 || RIZZ_SND_DEVICE_NUM_CHANNELS == 2,
                  "only mono or stereo output channel is supported");

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

    if (the_imgui &&
        !snd__ringbuffer_init(&g_snd.mixer_plot_buffer,
                              RIZZ_SND_DEVICE_BUFFER_FRAMES * RIZZ_SND_DEVICE_NUM_CHANNELS)) {
        sx_out_of_memory();
        return false;
    }

    g_snd.name_pool = sx_strpool_create(g_snd_alloc, NULL);
    g_snd.clocked_pool = sx_pool_create(g_snd_alloc, sizeof(snd__clocked), 128);
    if (!g_snd.name_pool || !g_snd.clocked_pool) {
        sx_out_of_memory();
        return false;
    }

    saudio_setup(&(saudio_desc){ .sample_rate = RIZZ_SND_DEVICE_SAMPLE_RATE,
                                 .num_channels = RIZZ_SND_DEVICE_NUM_CHANNELS,
                                 .stream_cb = snd__stream_cb,
                                 .buffer_frames = RIZZ_SND_DEVICE_BUFFER_FRAMES,
                                 .num_packets = 32 });

    // silent/beep source sources
    g_snd.beep_src =
        snd__create_dummy_source((const char*)k__snd_beep, sizeof(k__snd_beep), "beep");
    g_snd.silence_src = snd__create_dummy_source(k__snd_silent, sizeof(k__snd_silent), "silence");

    the_asset->register_asset_type(
        "sound",
        (rizz_asset_callbacks){ .on_prepare = snd__on_prepare,
                                .on_load = snd__on_load,
                                .on_finalize = snd__on_finalize,
                                .on_reload = snd__on_reload,
                                .on_release = snd__on_release },
        "rizz_snd_load_params", sizeof(rizz_snd_load_params), (rizz_asset_obj){ .id = g_snd.beep_src.id },
        (rizz_asset_obj){ .id = g_snd.silence_src.id }, 0);

    g_snd.master_volume = 1.0f;

    // initialize first bus to use all lanes
    g_snd.buses[0].max_lanes = RIZZ_SND_DEVICE_MAX_LANES;

    // we have a command buffer for each worker thread
    g_snd.cmd_buffers =
        sx_malloc(g_snd_alloc, sizeof(snd__cmdbuffer*) * the_core->job_num_threads());
    if (!g_snd.cmd_buffers) {
        sx_out_of_memory();
        return false;
    }
    g_snd.num_cmdbuffers = the_core->job_num_threads();
    sx_memset(g_snd.cmd_buffers, 0x0, sizeof(snd__cmdbuffer*) * the_core->job_num_threads());
    the_core->tls_register("snd_cmdbuffer", NULL, snd__cmdbuffer_init);

    return true;
}

static void snd__release()
{
    saudio_shutdown();

    if (g_snd.cmd_buffers) {
        for (int i = 0; i < g_snd.num_cmdbuffers; i++) {
            snd__cmdbuffer* cb = g_snd.cmd_buffers[i];
            if (cb) {
                sx_array_free(g_snd_alloc, cb->params_buff);
                sx_array_free(g_snd_alloc, cb->cmds);
                sx_free(g_snd_alloc, cb);
            }
        }
        sx_free(g_snd_alloc, g_snd.cmd_buffers);
    }

    if (g_snd.beep_src.id) {
        snd__destroy_dummy_source(g_snd.beep_src);
    }

    if (g_snd.silence_src.id) {
        snd__destroy_dummy_source(g_snd.silence_src);
    }

    if (g_snd.source_handles) {
        // two of them are reserved for dummies
        if (g_snd.source_handles->count > 2) {
            rizz_log_warn("sound: total %d sound_sources are not released",
                          g_snd.source_handles->count - 2);
        }
        sx_handle_destroy_pool(g_snd.source_handles, g_snd_alloc);
    }

    if (g_snd.instance_handles) {
        if (g_snd.instance_handles->count > 0) {
            rizz_log_warn("sound: total %d sound_instances are not released",
                          g_snd.instance_handles->count);
        }
        sx_handle_destroy_pool(g_snd.instance_handles, g_snd_alloc);
    }

    sx_array_free(g_snd_alloc, g_snd.sources);
    sx_array_free(g_snd_alloc, g_snd.instances);
    sx_array_free(g_snd_alloc, g_snd.clocked);

    snd__ringbuffer_release(&g_snd.mixer_buffer);
    snd__ringbuffer_release(&g_snd.mixer_plot_buffer);

    if (g_snd.name_pool) {
        sx_strpool_destroy(g_snd.name_pool, g_snd_alloc);
    }
    if (g_snd.clocked_pool) {
        sx_pool_destroy(g_snd.clocked_pool, g_snd_alloc);
    }

    the_asset->unregister_asset_type("sound");
}

static inline rizz_snd_instance snd__create_instance(rizz_snd_source srchandle, int bus,
                                                     float volume, float pan)
{
    sx_assert(bus >= 0 && bus < RIZZ_SND_DEVICE_MAX_BUSES);
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
    sx_handle_t handle = sx_handle_new_and_grow(g_snd.instance_handles, g_snd_alloc);

    snd__instance inst = (snd__instance){ .srchandle = srchandle,
                                          .volume = volume,
                                          .pan = sx_clamp(pan, -1.0f, 1.0f),
                                          .bus_id = bus,
                                          .play_frame = -1 };

    sx_array_push_byindex(g_snd_alloc, g_snd.instances, inst, sx_handle_index(handle));
    return (rizz_snd_instance){ handle };
}

static inline void snd__destroy_instance(rizz_snd_instance insthandle, bool remove_from_bus)
{
    sx_assert_always(sx_handle_valid(g_snd.instance_handles, insthandle.id));
    snd__instance* inst = &g_snd.instances[sx_handle_index(insthandle.id)];
    if (remove_from_bus && inst->play_frame >= 0) {
        // it means that the sound is already queued for play, so remove it from the bus
        sx_assert(g_snd.buses[inst->bus_id].num_lanes > 0);
        --g_snd.buses[inst->bus_id].num_lanes;
    }

    if (inst->state == SND_INSTANCESTATE_PLAYING) {
        // decrement num_plays from the source
        sx_assert_always(sx_handle_valid(g_snd.source_handles, inst->srchandle.id));
        snd__source* src = &g_snd.sources[sx_handle_index(inst->srchandle.id)];
        sx_assert(src->num_plays > 0);
        --src->num_plays;
    }

    sx_handle_del(g_snd.instance_handles, insthandle.id);
}

static void snd__queue_play(rizz_snd_instance insthandle)
{
    sx_assert_always(sx_handle_valid(g_snd.instance_handles, insthandle.id));
    snd__instance* inst = &g_snd.instances[sx_handle_index(insthandle.id)];
    int bus_id = inst->bus_id;
    sx_assert_always(g_snd.buses[bus_id].max_lanes > 0 &&
                  "bus is not initialized. call `bus_set_max_lanes` on bus_id"); 

    sx_assert_always(sx_handle_valid(g_snd.source_handles, inst->srchandle.id));

    snd__source* src = &g_snd.sources[sx_handle_index(inst->srchandle.id)];
    sx_assert(src->samples);

    inst->play_frame = the_core->frame_index();
    inst->pos = 0;
    inst->state = SND_INSTANCESTATE_PLAYING;

    // for singleton sources, check all playing sounds and remove any with the same audio source
    if (src->flags & SND_SOURCEFLAG_SINGLETON) {
        rizz_snd_source srchandle = inst->srchandle;
        for (int i = 0, c = g_snd.num_plays; i < c; i++) {
            rizz_snd_instance playing_insthandle = g_snd.playlist[i];
            snd__instance* playing_inst = &g_snd.instances[sx_handle_index(playing_insthandle.id)];
            if (playing_inst->srchandle.id == srchandle.id) {
                snd__destroy_instance(playing_insthandle, true);
                sx_swap(g_snd.playlist[i], g_snd.playlist[c - 1], rizz_snd_instance);
                --c;
            }
            g_snd.num_plays = c;
        }
    }

    if (g_snd.buses[bus_id].max_lanes == g_snd.buses[bus_id].num_lanes) {
        // remove one sound from the playlist and replace it with this one
        int64_t min_play_frame = INT64_MAX;
        int replace_inst_index = -1;
        for (int i = 0, c = g_snd.num_plays; i < c; i++) {
            rizz_snd_instance check_handle = g_snd.playlist[i];
            snd__instance* check_inst = &g_snd.instances[sx_handle_index(check_handle.id)];
            if (check_inst->bus_id == bus_id && check_inst->play_frame < min_play_frame) {
                min_play_frame = check_inst->play_frame;
                replace_inst_index = i;
            }
        }

        sx_assert(replace_inst_index != -1);
        snd__destroy_instance(g_snd.playlist[replace_inst_index], false);
        g_snd.playlist[replace_inst_index] = insthandle;
    } else {
        ++g_snd.buses[bus_id].num_lanes;
        g_snd.playlist[g_snd.num_plays++] = insthandle;
    }
    sx_assert(g_snd.num_plays <= RIZZ_SND_DEVICE_MAX_LANES);

    ++src->num_plays;
}

static rizz_snd_instance snd__play(rizz_snd_source srchandle, int bus, float volume, float pan,
                                   bool paused)
{
    rizz_snd_instance insthandle = snd__create_instance(srchandle, bus, volume, pan);
    if (!paused) {
        snd__queue_play(insthandle);
    }
    return insthandle;
}

static rizz_snd_instance snd__play_clocked(rizz_snd_source src, float wait_tm, int bus,
                                           float volume, float pan)
{
    snd__clocked* new_clocked = sx_pool_new(g_snd.clocked_pool);
    if (!new_clocked) {
        sx_out_of_memory();
        return (rizz_snd_instance){ 0 };
    }
    sx_memset(new_clocked, 0x0, sizeof(*new_clocked));
    new_clocked->src = src;
    new_clocked->inst = snd__create_instance(src, bus, volume, pan);
    new_clocked->bus_id = bus;
    new_clocked->wait_tm = wait_tm;

    // check current clocked items and see if already have the source playing
    for (int i = 0, c = sx_array_count(g_snd.clocked); i < c; i++) {
        snd__clocked* clocked = g_snd.clocked[i];
        if (clocked->src.id == src.id && clocked->bus_id == bus) {
            // add it to the end of the list
            snd__clocked* last = clocked;
            while (last->next) {
                last = last->next;
            }
            last->next = new_clocked;
            return new_clocked->inst;
        }
    }

    // not found in the clocked list, create a new one
    new_clocked->first = true;
    sx_array_push(g_snd_alloc, g_snd.clocked, new_clocked);
    return new_clocked->inst;
}

static void snd__stop(rizz_snd_instance insthandle)
{
    if (sx_handle_valid(g_snd.instance_handles, insthandle.id)) {
        for (int i = 0, c = g_snd.num_plays; i < c; i++) {
            if (g_snd.playlist[i].id == insthandle.id) {
                sx_swap(g_snd.playlist[i], g_snd.playlist[c - 1], rizz_snd_instance);
                --g_snd.num_plays;
                break;
            }
        }
        snd__destroy_instance(insthandle, true);
    }
}

static void snd__stop_all(void)
{
    for (int i = 0, c = g_snd.num_plays; i < c; i++) {
        snd__destroy_instance(g_snd.playlist[i], true);
    }
    g_snd.num_plays = 0;
}

static void snd__resume(rizz_snd_instance insthandle)
{
    if (sx_handle_valid(g_snd.instance_handles, insthandle.id)) {
        snd__queue_play(insthandle);
    }
}

static void snd__bus_stop(int bus)
{
    sx_assert(bus >= 0 && bus < RIZZ_SND_DEVICE_MAX_BUSES);

    int num_plays = g_snd.num_plays;
    for (int i = 0; i < num_plays; i++) {
        rizz_snd_instance insthandle = g_snd.playlist[i];
        snd__instance* inst = &g_snd.instances[sx_handle_index(insthandle.id)];
        if (inst->bus_id == bus) {
            sx_swap(g_snd.playlist[i], g_snd.playlist[num_plays - 1], rizz_snd_instance);
            --num_plays;
            snd__destroy_instance(insthandle, true);
        }
    }
    g_snd.num_plays = num_plays;
}


// TODO: research on fixed-point math, in order to implement linear sampling
static int snd__resample_point(float* dst, int num_dst_samples, int sample_offset, const float* src,
                               float src_sample_rate, float dst_sample_rate)
{
    float multiplier = dst_sample_rate / src_sample_rate;
    uint32_t step_fixed = (uint32_t)sx_floor(FIXPOINT_FRAC_MUL / multiplier);
    uint64_t pos = ((uint64_t)sample_offset << FIXPOINT_FRAC_BITS);

    for (int i = 0; i < num_dst_samples; i++, pos += step_fixed) {
        int p = (int)(pos >> FIXPOINT_FRAC_BITS);
        dst[i] = src[p];
    }

    int new_pos = (int)(pos >> FIXPOINT_FRAC_BITS);
    return new_pos != sample_offset ? new_pos : sample_offset + num_dst_samples;
}

static void snd__mix(float* dst, int dst_num_frames, int dst_num_channels, int dst_sample_rate)
{
    float dst_sample_ratef = (float)dst_sample_rate;
    int frames_written = 0;
    float master_pan_ch1 = g_snd.master_pan > 0.0f ? (1.0f - g_snd.master_pan) : 1.0f;
    float master_pan_ch2 = g_snd.master_pan < 0.0f ? (1.0f + g_snd.master_pan) : 1.0f;

    rizz_snd_instance garbage[RIZZ_SND_DEVICE_MAX_LANES];
    int num_garbage = 0;
    sx_assert(g_snd.num_plays <= RIZZ_SND_DEVICE_MAX_LANES);

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();

    sx_scope(the_core->tmp_alloc_pop()) {

        for (int i = 0, c = g_snd.num_plays; i < c; i++) {
            rizz_snd_instance insthandle = g_snd.playlist[i];
            sx_assert_always(sx_handle_valid(g_snd.instance_handles, insthandle.id));
            snd__instance* inst = &g_snd.instances[sx_handle_index(insthandle.id)];
            rizz_snd_source srchandle = inst->srchandle;
            sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
            snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];

            int pos = inst->pos;
            int src_frames_remain = src->num_frames - pos;
            int num_frames = sx_min(dst_num_frames, src_frames_remain);
            int num_samples = num_frames;    // mono
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
                if (!(src->flags & SND_SOURCEFLAG_LOOPING)) {
                    garbage[num_garbage++] = insthandle;
                } else {
                    inst->pos = 0;
                }
            }

            // add to destination buffer. upmix to device_channels if output is stereo
            float vol = inst->volume * src->volume * g_snd.master_volume;

            int dst_num_samples = num_frames * dst_num_channels;
            if (dst_num_channels == 2) {
                float pan = inst->pan;
                float channel1 = master_pan_ch1 * (pan > 0 ? (1.0f - pan) : 1.0f) * vol;    // left
                float channel2 = master_pan_ch2 * (pan < 0 ? (1.0f + pan) : 1.0f) * vol;    // right

                for (int s = 0, p = 0; s < dst_num_samples; s += dst_num_channels, p++) {
                    dst[s] += frames[p] * channel1;
                    dst[s + 1] += frames[p] * channel2;
                }
            } else if (dst_num_channels == 1) {
                for (int s = 0, p = 0; s < dst_num_samples; s += dst_num_channels, p++) {
                    dst[s] += frames[p] * vol;
                }
            } else {
                sx_assertf(0, "not implemented");
            }

            frames_written = sx_max(frames_written, num_frames);
        }
    } // scope

    // A very naive clipping
    int samples_written = frames_written * dst_num_channels;
    for (int i = 0; i < samples_written; i++) {
        dst[i] = sx_clamp(dst[i], -1.0f, 1.0f);
    }

    // fill remaining frames with zeros
    if (frames_written < dst_num_frames) {
        sx_memset(dst + frames_written * dst_num_channels, 0x0,
                  (dst_num_frames - frames_written) * dst_num_channels * sizeof(float));
    }

    // delete garbage instances and remove from playlist
    for (int i = 0; i < num_garbage; i++) {
        rizz_snd_instance insthandle = garbage[i];
        for (int k = 0; k < g_snd.num_plays; k++) {
            if (insthandle.id == g_snd.playlist[k].id) {
                sx_swap(g_snd.playlist[k], g_snd.playlist[g_snd.num_plays - 1], rizz_snd_instance);
                --g_snd.num_plays;
                break;
            }
        }
        snd__destroy_instance(insthandle, true);
    }
}

static void snd__update(float dt)
{
    int device_channels = saudio_channels();
    int device_sample_rate = saudio_sample_rate();

    // update clocked items
    for (int i = 0, c = sx_array_count(g_snd.clocked); i < c; i++) {
        snd__clocked* clocked = g_snd.clocked[i];
        if (clocked->tm < clocked->wait_tm && !clocked->first) {
            clocked->tm += dt;
        } else {
            if (clocked->inst.id) {
                snd__queue_play(clocked->inst);
            }

            if (!clocked->first) {
                if (clocked->next) {
                    if (!clocked->inst.id) {
                        clocked->next->wait_tm -= clocked->wait_tm;
                    }
                    g_snd.clocked[i] = clocked->next;
                } else {
                    sx_array_pop(g_snd.clocked, i);
                    --c;
                }
                sx_pool_del(g_snd.clocked_pool, clocked);
            } else {
                clocked->first = false;
                clocked->inst.id = 0;
            }
        }
    }

    // mix into main sound buffer
    int frames_remain = snd__ringbuffer_expect(&g_snd.mixer_buffer) / device_channels;
    int frames_needed = (int)(dt * (float)device_sample_rate * 1.5f);
    int s = (g_snd.mixer_buffer.capacity / device_channels) - frames_remain;
    if ((frames_needed + s) < RIZZ_SND_DEVICE_BUFFER_FRAMES) {
        frames_needed = RIZZ_SND_DEVICE_BUFFER_FRAMES;
    }

    frames_remain = frames_needed != 0 ? sx_min(frames_remain, frames_needed) : frames_remain;
    if (frames_remain) {
        const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
        
        sx_scope(the_core->tmp_alloc_pop()) {
            float* frames = sx_malloc(tmp_alloc, sizeof(float) * frames_remain * device_channels);
            if (!frames) {
                sx_out_of_memory();
                return;
            }
            sx_memset(frames, 0x0, sizeof(float) * frames_remain * device_channels);
            snd__mix(frames, frames_remain, device_channels, device_sample_rate);
            snd__ringbuffer_produce(&g_snd.mixer_buffer, frames, frames_remain * device_channels);
        }
    }
}

static void snd__plot_samples_rms(const char* label, const float* samples, int num_samples,
                                  int channel, int num_channels, int height, float scale)
{
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();

    sx_scope(the_core->tmp_alloc_pop()) {
        sx_vec2 region;
        the_imgui->GetContentRegionAvail(&region);
        int width = (int)region.x;
        int num_values;
        float* values;
        const int optimal_values = 50;

        if (num_samples > 0) {
            int num_plot_samples = num_samples / num_channels;
            float* plot_samples = sx_malloc(tmp_alloc, sizeof(float) * num_plot_samples);
            sx_assert(plot_samples);

            for (int i = 0, p = 0; i < num_samples; i += num_channels, p++) {
                plot_samples[p] = samples[i + channel];
            }

            num_values = width * optimal_values / 250;
            if (num_values < 20)
                num_values = 20;
            values = sx_malloc(tmp_alloc, sizeof(float) * num_values);
            sx_assert(values);
            int block_sz = num_plot_samples / num_values;
            float block_sz_rcp = 1.0f / (float)block_sz;
            for (int i = 0; i < num_values; i++) {
                float rms = 0;
                for (int s = 0; s < block_sz; s++) {
                    rms += plot_samples[s] * plot_samples[s];
                }
                values[i] = sx_sqrt(rms * block_sz_rcp) * scale;
                plot_samples += block_sz;
            }
        } else {
            values = sx_malloc(tmp_alloc, sizeof(float) * optimal_values);
            num_values = optimal_values;
            sx_memset(values, 0x0, sizeof(float) * optimal_values);
        }

        the_imgui->PlotHistogram_FloatPtr(label, values, num_values, 0, NULL, 0, 1.0f,
                                          sx_vec2f((float)width, (float)height), sizeof(float));
    } // scope
}

static void snd__plot_samples_wav(const char* label, const float* samples, int num_samples,
                                  int height)
{
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    
    sx_scope(the_core->tmp_alloc_pop()) {
        sx_assert(num_samples > 0);

        sx_vec2 region;
        the_imgui->GetContentRegionAvail(&region);
        int width = (int)region.x;
        const int optimal_values = 50;

        int num_values = width * optimal_values / 250;
        if (num_values < 20)
            num_values = 20;
        float* values = sx_malloc(tmp_alloc, sizeof(float) * num_values * 2);
        sx_assert(values);
        int block_sz = num_samples / num_values;
        const float* plot_samples = samples;
        for (int i = 0; i < num_values; i++) {
            float _max = -FLT_MAX;
            float _min = FLT_MAX;
            int max_idx = -1;
            int min_idx = -1;

            for (int s = 0; s < block_sz; s++) {
                if (plot_samples[s] > _max) {
                    _max = plot_samples[s];
                    max_idx = s;
                }
                if (plot_samples[s] < _min) {
                    _min = plot_samples[s];
                    min_idx = s;
                }
            }
            if (max_idx > min_idx) {
                values[i * 2] = _min;
                values[i * 2 + 1] = _max;
            } else {
                values[i * 2] = _max;
                values[i * 2 + 1] = _min;
            }
            plot_samples += block_sz;
        }

        the_imgui->PlotLines_FloatPtr(label, values, num_values * 2, 0, NULL, -1.0f, 1.0f,
                                      sx_vec2f((float)width, (float)height), sizeof(float));
    } // scope
}

static void snd__show_mixer_tab_contents()
{
    the_imguix->label("channels", "%d", saudio_channels());
    the_imgui->SliderFloat("master", &g_snd.master_volume, 0.0f, 1.2f, "%.1f", 0);
    the_imgui->SliderFloat("pan", &g_snd.master_pan, -1.0f, 1.0f, "%.1f", 0);

    // plot samples
    static float plot_scale = 1.0f;
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    
    sx_scope(the_core->tmp_alloc_pop()) {
        int num_channels = saudio_channels();
        int num_samples = RIZZ_SND_DEVICE_BUFFER_FRAMES * num_channels;
        float* samples = sx_malloc(tmp_alloc, sizeof(float) * num_samples);
        if (!samples) {
            sx_out_of_memory();
            return;
        }
        num_samples = snd__ringbuffer_consume(&g_snd.mixer_plot_buffer, samples, sx_min(512, num_samples));
        sx_vec2 region;
        the_imgui->GetContentRegionAvail(&region);
        float plot_width = region.x;
        the_imgui->Columns(num_channels + 1, "mixer_wav_cols", false);
        the_imgui->SetColumnWidth(0, 25.0f);
        plot_width -= 25.0f;
        the_imgui->SetColumnWidth(1, plot_width * 0.5f);
        the_imgui->SetColumnWidth(2, plot_width * 0.5f);
        the_imgui->VSliderFloat("##plot_scale", sx_vec2f(12.0f, 50.0f), &plot_scale, 1.0f, 10.0f, "", 0);
        the_imgui->NextColumn();
        for (int ch = 0; ch < num_channels; ch++) {
            char plot_str[32];
            sx_snprintf(plot_str, sizeof(plot_str), "channel #%d", ch + 1);
            snd__plot_samples_rms("##mixer_plot", samples, num_samples, ch, num_channels, 50,
                                  plot_scale);
            the_imgui->NextColumn();
        }

        the_imgui->Columns(1, NULL, false);
        the_imgui->Separator();

        if (the_imgui->BeginTable("Sound Sources", 4, 
                                  ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersV|ImGuiTableFlags_BordersOuterH|
                                  ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,
                                  SX_VEC2_ZERO, 0)) {
            int num_sounds = g_snd.num_plays;
            the_imgui->GetContentRegionAvail(&region);
            the_imgui->TableSetupColumn("#", 0, 30.0f, 0);
            the_imgui->TableSetupColumn("Bus", 0, 30.0f, 0);
            the_imgui->TableSetupColumn("Progres", 0, 100.0f, 0);
            the_imgui->TableSetupColumn("Source", 0, sx_max(50.0f, region.x - 160.0f), 0);
            the_imgui->TableHeadersRow();

            ImGuiListClipper clipper;
            the_imgui->ImGuiListClipper_Begin(&clipper, num_sounds, -1.0f);
            while (the_imgui->ImGuiListClipper_Step(&clipper)) {
                int start = num_sounds - clipper.DisplayStart - 1;
                int end = num_sounds - clipper.DisplayEnd;
                for (int i = start; i >= end; i--) {
                    the_imgui->TableNextRow(0, 0);

                    rizz_snd_instance insthandle = g_snd.playlist[i];
                    sx_assert_always(sx_handle_valid(g_snd.instance_handles, insthandle.id));

                    const snd__instance* inst = &g_snd.instances[sx_handle_index(insthandle.id)];
                    sx_assert_always(sx_handle_valid(g_snd.source_handles, inst->srchandle.id));
                    const snd__source* src = &g_snd.sources[sx_handle_index(inst->srchandle.id)];

                    the_imgui->TableNextColumn();
                    the_imgui->Text("%d", i + 1);

                    the_imgui->TableNextColumn();
                    the_imgui->Text("%d", inst->bus_id);

                    float progress = (float)inst->pos / (float)src->num_frames;
                    the_imgui->TableNextColumn();
                    the_imgui->ProgressBar(progress, sx_vec2f(-1.0f, 15.0f), NULL);

                    sx_assert(src->name);
                    the_imgui->TableNextColumn();
                    the_imgui->Text(sx_strpool_cstr(g_snd.name_pool, src->name));
                }
            }
            the_imgui->ImGuiListClipper_End(&clipper);
            the_imgui->EndTable();
        }
    } // scope
}

static float snd__source_duration(rizz_snd_source srchandle)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));

    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    return (float)src->num_frames / (float)src->sample_rate;
}

static void snd__show_sources_tab_contents()
{
    static int selected_source = -1;
    int num_sources = g_snd.source_handles->count;

    sx_vec2 region;
    the_imgui->GetContentRegionAvail(&region);
    the_imgui->BeginChild_Str("SourcesContainer", sx_vec2f(0, region.y*0.5f), false, 0);
    if (the_imgui->BeginTable("Sound Sources", 3, 
                              ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersV|ImGuiTableFlags_BordersOuterH|
                              ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollY, 
                              SX_VEC2_ZERO, 0)) {
        the_imgui->TableSetupColumn("#", 0, 30.0f, 0);
        the_imgui->TableSetupColumn("Play", 0, 35.0f, 0);
        the_imgui->TableSetupColumn("Name", 0, 0, 0);
        the_imgui->TableHeadersRow();

        ImGuiListClipper clipper;
        the_imgui->ImGuiListClipper_Begin(&clipper, num_sources, -1.0f);
        char row_str[32];
        char pcount_str[32];
        while (the_imgui->ImGuiListClipper_Step(&clipper)) {
            int start = num_sources - clipper.DisplayStart - 1;
            int end = num_sources - clipper.DisplayEnd;
            for (int i = start; i >= end; i--) {
                the_imgui->TableNextRow(0, 0);

                sx_handle_t handle = sx_handle_at(g_snd.source_handles, i);
                sx_assert_always(sx_handle_valid(g_snd.source_handles, handle));
                const snd__source* src = &g_snd.sources[sx_handle_index(handle)];

                the_imgui->TableSetColumnIndex(0);
                sx_snprintf(row_str, sizeof(row_str), "%d", i + 1);
                if (the_imgui->Selectable_Bool(row_str, selected_source == i,
                                               ImGuiSelectableFlags_SpanAllColumns, SX_VEC2_ZERO)) {
                    selected_source = i;
                }

                the_imgui->TableSetColumnIndex(1);
                sx_snprintf(pcount_str, sizeof(pcount_str), "%d", src->num_plays);
                the_imgui->ColorButton(pcount_str,
                                       src->num_plays > 0 ? sx_vec4f(0.0f, 0.8f, 0.1f, 1.0f)
                                                          : sx_vec4f(0.3f, 0.3f, 0.3f, 1.0f),
                                       0, sx_vec2f(14.0f, 14.0f));

                the_imgui->TableSetColumnIndex(2);
                the_imgui->Text(sx_strpool_cstr(g_snd.name_pool, src->name));                
            }
        }
        the_imgui->ImGuiListClipper_End(&clipper);

        the_imgui->EndTable();
    } // end:sound source table
    the_imgui->EndChild();

    the_imgui->Separator();

    if (selected_source != -1) {
        sx_handle_t handle = sx_handle_at(g_snd.source_handles, selected_source);
        sx_assert_always(sx_handle_valid(g_snd.source_handles, handle));
        const snd__source* src = &g_snd.sources[sx_handle_index(handle)];
        snd__plot_samples_wav("##source_plot", src->samples, src->num_frames, 70);

        const float offset = 100.0f;
        the_imgui->Columns(2, "source_info_cols", true);
        the_imguix->label_spacing(offset, 0, "sample_rate", "%d", src->sample_rate);
        the_imguix->label_spacing(offset, 0, "channels", "%d", 1);
        the_imguix->label_spacing(offset, 0, "volume", "%.1f", src->volume);
        float duration = snd__source_duration((rizz_snd_source){ handle });
        the_imguix->label_spacing(offset, 0, "duration", "%.3fs (%.2fms)", duration, duration * 1000.0f);
        the_imguix->label_spacing(offset, 0, "looping", (src->flags & SND_SOURCEFLAG_LOOPING) ? "Yes" : "No");
        the_imguix->label_spacing(offset, 0, "singleton", (src->flags & SND_SOURCEFLAG_SINGLETON) ? "Yes" : "No");

        the_imgui->NextColumn();
        static int bus_id = 0;
        const int bus_id_step = 1;
        if (the_imgui->Button("Play", SX_VEC2_ZERO)) {
            snd__play((rizz_snd_source){ handle }, bus_id, 1.0f, 0, false);
        }
        the_imgui->SameLine(0, -1.0f);
        the_imgui->InputScalar("bus_id", ImGuiDataType_S32, &bus_id, &bus_id_step, NULL, "%u", 0);
        bus_id = sx_clamp(bus_id, 0, (RIZZ_SND_DEVICE_MAX_BUSES - 1));
    }
}

static void snd__show_debugger(bool* p_open)
{
    if (!the_imgui) {
        return;
    }

    the_imgui->SetNextWindowSizeConstraints(sx_vec2f(450.0f, 500.0f), sx_vec2f(FLT_MAX, FLT_MAX),
                                            NULL, NULL);
    if (the_imgui->Begin("Sound Debugger", p_open, 0)) {
        if (the_imgui->BeginTabBar("sound_tab", 0)) {

            if (the_imgui->BeginTabItem("Mixer", NULL, 0)) {
                snd__show_mixer_tab_contents();
                the_imgui->EndTabItem();
            }

            if (the_imgui->BeginTabItem("Sources", NULL, 0)) {
                snd__show_sources_tab_contents();
                the_imgui->EndTabItem();
            }
            the_imgui->EndTabBar();
        }
    }
    the_imgui->End();
}

static float snd__master_volume(void)
{
    return g_snd.master_volume;
}

static void snd__set_master_volume(float vol)
{
    g_snd.master_volume = sx_clamp(vol, 0.0f, 1.2f);
}

static float snd__master_pan(void)
{
    return g_snd.master_pan;
}

static void snd__set_master_pan(float pan)
{
    g_snd.master_pan = sx_clamp(pan, -1.0f, 1.0f);
}

static void snd__bus_set_max_lanes(int bus, int max_lanes)
{
    sx_assert(bus >= 0 && bus < RIZZ_SND_DEVICE_MAX_BUSES);

    // count all lanes and check if we have to
    int count = 0;
    for (int i = 0; i < RIZZ_SND_DEVICE_MAX_BUSES; i++) {
        count += i != bus ? g_snd.buses[i].max_lanes : 0;
    }

    int remain = RIZZ_SND_DEVICE_MAX_LANES - count;
    if (max_lanes > remain) {
        rizz_log_warn(
            "sound: maximum lanes for bus '%d' (%d truncated to %d) exceeds total maximum of %d",
            bus, max_lanes, remain, RIZZ_SND_DEVICE_MAX_LANES);
        sx_assertf(0, "wether increase RIZZ_SND_DEVICE_MAX_LANES or fix `max_lanes` parameter");
        g_snd.buses[bus].max_lanes = remain;
    } else {
        g_snd.buses[bus].max_lanes = max_lanes;
    }
}

static float snd__source_volume(rizz_snd_source srchandle)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));

    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    return src->volume;
}

static void snd__source_stop(rizz_snd_source srchandle)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));

    int c = g_snd.num_plays;
    for (int i = 0; i < c; i++) {
        rizz_snd_instance insthandle = g_snd.playlist[i];
        sx_assert_always(sx_handle_valid(g_snd.instance_handles, insthandle.id));
        snd__instance* inst = &g_snd.instances[sx_handle_index(insthandle.id)];
        if (inst->srchandle.id == srchandle.id) {
            sx_swap(g_snd.playlist[i], g_snd.playlist[c - 1], rizz_snd_instance);
            snd__destroy_instance(insthandle, true);
            --c;
            break;
        }
    }
    g_snd.num_plays = c;
}

static bool snd__source_looping(rizz_snd_source srchandle)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    return src->flags & SND_SOURCEFLAG_LOOPING;
}

static void snd__source_set_looping(rizz_snd_source srchandle, bool loop)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    if (loop) {
        src->flags |= SND_SOURCEFLAG_LOOPING;
    } else {
        src->flags &= ~SND_SOURCEFLAG_LOOPING;
    }
}

static void snd__source_set_singleton(rizz_snd_source srchandle, bool singleton)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    if (singleton) {
        src->flags |= SND_SOURCEFLAG_SINGLETON;
    } else {
        src->flags &= ~SND_SOURCEFLAG_SINGLETON;
    }
}

static void snd__source_set_volume(rizz_snd_source srchandle, float vol)
{
    sx_assert_always(sx_handle_valid(g_snd.source_handles, srchandle.id));
    snd__source* src = &g_snd.sources[sx_handle_index(srchandle.id)];
    src->volume = sx_clamp(vol, 0.0f, 1.2f);
}

static inline uint8_t* snd__cb_alloc_params_buff(snd__cmdbuffer* cb, int size, int* offset)
{
    uint8_t* ptr = sx_array_add(g_snd_alloc, cb->params_buff,
                                sx_align_mask(size, SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT - 1));
    if (!ptr) {
        sx_out_of_memory();
        return NULL;
    }

    *offset = (int)(intptr_t)(ptr - cb->params_buff);
    return ptr;
}

typedef struct snd__play_params {
    rizz_snd_source src;
    int bus;
    float vol;
    float pan;
    float wait_tm;
    bool paused;
} snd__play_params;

static void snd__cb_play(rizz_snd_source src, int bus, float volume, float pan, bool paused)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(snd__play_params), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_PLAY,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((snd__play_params*)buff) =
        (snd__play_params){ .src = src, .bus = bus, .vol = volume, .pan = pan, .paused = paused };
}

static uint8_t* snd__cb_run_play(uint8_t* buff)
{
    snd__play_params* p = (snd__play_params*)buff;
    buff += sizeof(snd__play_params);
    snd__play(p->src, p->bus, p->vol, p->pan, p->paused);
    return buff;
}

static void snd__cb_play_clocked(rizz_snd_source src, float wait_tm, int bus, float volume,
                                 float pan)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(snd__play_params), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_PLAY_CLOCKED,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((snd__play_params*)buff) =
        (snd__play_params){ .src = src, .bus = bus, .vol = volume, .pan = pan, .wait_tm = wait_tm };
}

static uint8_t* snd__cb_run_play_clocked(uint8_t* buff)
{
    snd__play_params* p = (snd__play_params*)buff;
    buff += sizeof(snd__play_params);
    snd__play_clocked(p->src, p->wait_tm, p->bus, p->vol, p->pan);
    return buff;
}

static void snd__cb_bus_stop(int bus)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(int), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_BUS_STOP,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((int*)buff) = bus;
}

static uint8_t* snd__cb_run_bus_stop(uint8_t* buff)
{
    int bus_id = *((int*)buff);
    buff += sizeof(int);
    snd__bus_stop(bus_id);
    return buff;
}

static void snd__cb_set_master_volume(float volume)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(int), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_SET_MASTER_VOLUME,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((float*)buff) = volume;
}

static uint8_t* snd__cb_run_set_master_volume(uint8_t* buff)
{
    float vol = *((float*)buff);
    buff += sizeof(float);
    snd__set_master_volume(vol);
    return buff;
}

static void snd__cb_set_master_pan(float pan)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(int), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_SET_MASTER_PAN,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((float*)buff) = pan;
}

static uint8_t* snd__cb_run_set_master_pan(uint8_t* buff)
{
    float pan = *((float*)buff);
    buff += sizeof(float);
    snd__set_master_pan(pan);
    return buff;
}

static void snd__cb_source_stop(rizz_snd_source src)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(rizz_snd_source), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_SOURCE_STOP,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((rizz_snd_source*)buff) = src;
}

static uint8_t* snd__cb_run_source_stop(uint8_t* buff)
{
    rizz_snd_source src = *((rizz_snd_source*)buff);
    buff += sizeof(rizz_snd_source);
    snd__source_stop(src);
    return buff;
}

static void snd__cb_source_set_volume(rizz_snd_source src, float vol)
{
    snd__cmdbuffer* cb = the_core->tls_var("snd_cmdbuffer");
    sx_assert(cb->cmd_idx < INT_MAX);

    int offset = 0;
    uint8_t* buff = snd__cb_alloc_params_buff(cb, sizeof(rizz_snd_source) + sizeof(float), &offset);
    sx_assert(buff);

    snd__cmdheader hdr = { .cmd = SND_COMMAND_SOURCE_SET_VOLUME,
                           .cmdbuffer_idx = cb->index,
                           .params_offset = offset };
    sx_array_push(g_snd_alloc, cb->cmds, hdr);
    ++cb->cmd_idx;

    *((rizz_snd_source*)buff) = src;
    buff += sizeof(rizz_snd_source);
    *((float*)buff) = vol;
}

static uint8_t* snd__cb_run_source_set_volume(uint8_t* buff)
{
    rizz_snd_source src = *((rizz_snd_source*)buff);
    buff += sizeof(rizz_snd_source);
    float vol = *((float*)buff);
    buff += sizeof(float);
    snd__source_set_volume(src, vol);
    return buff;
}

static const snd__run_command_cb k_snd_run_cbs[_SND_COMMAND_COUNT] = {
    snd__cb_run_play,
    snd__cb_run_play_clocked,
    snd__cb_run_bus_stop,
    snd__cb_run_set_master_volume,
    snd__cb_run_set_master_pan,
    snd__cb_run_source_stop,
    snd__cb_run_source_set_volume
};

static void snd__execute_command_buffers()
{
    static_assert((sizeof(k_snd_run_cbs) / sizeof(snd__run_command_cb)) == _SND_COMMAND_COUNT,
                  "k_snd_run_cbs must match snd__command");

    for (int i = 0, c = g_snd.num_cmdbuffers; i < c; i++) {
        snd__cmdbuffer* cb = g_snd.cmd_buffers[i];
        if (!cb) {
            continue;
        }

        for (int k = 0, kc = sx_array_count(cb->cmds); k < kc; k++) {
            const snd__cmdheader* hdr = &cb->cmds[k];
            k_snd_run_cbs[hdr->cmd](&cb->params_buff[hdr->params_offset]);
        }

        // reset
        sx_array_clear(cb->params_buff);
        sx_array_clear(cb->cmds);
        cb->cmd_idx = 0;
    }
}

static rizz_snd_source snd__source_get(rizz_asset snd_asset)
{
#if RIZZ_DEV_BUILD
    sx_assert_always(sx_strequal(the_asset->type_name(snd_asset), "sound") && "asset handle is not a sound");
#endif
    return (rizz_snd_source){ (uint32_t)the_asset->obj(snd_asset).id };
}

static rizz_api_snd the__snd = { .queued = { .play = snd__cb_play,
                                             .play_clocked = snd__cb_play_clocked,
                                             .bus_stop = snd__cb_bus_stop,
                                             .set_master_volume = snd__cb_set_master_volume,
                                             .set_master_pan = snd__cb_set_master_pan,
                                             .source_stop = snd__cb_source_stop,
                                             .source_set_volume = snd__cb_source_set_volume },
                                 .play = snd__play,
                                 .play_clocked = snd__play_clocked,
                                 .stop = snd__stop,
                                 .stop_all = snd__stop_all,
                                 .resume = snd__resume,
                                 .bus_set_max_lanes = snd__bus_set_max_lanes,
                                 .bus_stop = snd__bus_stop,
                                 .master_volume = snd__master_volume,
                                 .set_master_volume = snd__set_master_volume,
                                 .master_pan = snd__master_pan,
                                 .set_master_pan = snd__set_master_pan,
                                 .source_duration = snd__source_duration,
                                 .source_volume = snd__source_volume,
                                 .source_stop = snd__source_stop,
                                 .source_looping = snd__source_looping,
                                 .source_set_looping = snd__source_set_looping,
                                 .source_set_singleton = snd__source_set_singleton,
                                 .source_set_volume = snd__source_set_volume,
                                 .source_get = snd__source_get,
                                 .show_debugger = snd__show_debugger };

rizz_plugin_decl_main(sound, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        rizz_profile(Sound) {
            snd__execute_command_buffers();
            snd__update((float)sx_tm_sec(the_core->delta_tick()));
        }
        break;
    }
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = the_plugin->get_api(RIZZ_API_CORE, 0);
        the_asset = the_plugin->get_api(RIZZ_API_ASSET, 0);
        the_refl = the_plugin->get_api(RIZZ_API_REFLECT, 0);
        the_imgui = the_plugin->get_api_byname("imgui", 0);
        the_imguix = the_plugin->get_api_byname("imgui_extra", 0);

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

rizz_plugin_decl_event_handler(sound, e)
{
    if (e->type == RIZZ_APP_EVENTTYPE_UPDATE_APIS) {
        the_imgui = the_plugin->get_api_byname("imgui", 0);
    }
}


static const char* sound__deps[] = { "imgui" };
rizz_plugin_implement_info(sound, 1000, "sound plugin", sound__deps, 1);

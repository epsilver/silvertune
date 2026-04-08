#include "silvertune.h"
#include <cstring>
#include <new>

// --- Plugin descriptor ---

static const char *features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_PITCH_CORRECTION,
    CLAP_PLUGIN_FEATURE_PITCH_SHIFTER,
    CLAP_PLUGIN_FEATURE_STEREO,
    CLAP_PLUGIN_FEATURE_MONO,
    nullptr
};

extern const clap_plugin_descriptor_t silvertune_descriptor;
const clap_plugin_descriptor_t silvertune_descriptor = {
    .clap_version = CLAP_VERSION,
    .id           = "com.silvertune.silvertune",
    .name         = "Silvertune",
    .vendor       = "Silvertune",
    .url          = "",
    .manual_url   = "",
    .support_url  = "",
    .version      = "0.1.0",
    .description  = "Hard pitch correction / Cher effect",
    .features     = features,
};

// --- Audio ports extension ---

static uint32_t audio_ports_count(const clap_plugin_t *, bool) {
    return 1;
}

static bool audio_ports_get(const clap_plugin_t *, uint32_t index, bool is_input,
                             clap_audio_port_info_t *info) {
    if (index != 0) return false;
    memset(info, 0, sizeof(*info));
    info->id = 0;
    info->channel_count = 2;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = 0;
    strncpy(info->name, is_input ? "Audio In" : "Audio Out", CLAP_NAME_SIZE);
    return true;
}

static const clap_plugin_audio_ports_t silvertune_audio_ports = {
    .count = audio_ports_count,
    .get   = audio_ports_get,
};

// --- Latency extension ---

static uint32_t latency_get(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    return p->total_latency;
}

static const clap_plugin_latency_t silvertune_latency = {
    .get = latency_get,
};

// --- Plugin lifecycle ---

static bool plugin_init(const clap_plugin_t *) {
    return true;
}

static void plugin_destroy(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    delete p->clap_plugin;
    delete p;
}

static bool plugin_activate(const clap_plugin_t *plugin, double sample_rate,
                              uint32_t, uint32_t max_frames) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    p->sample_rate = sample_rate;
    p->max_frames = max_frames;

    // --- Pitch detection setup ---
    p->pitch_buf_size = 1024;
    p->pitch_hop_size = 256;

    p->pitch_detector = new_aubio_pitch(
        "yin", p->pitch_buf_size, p->pitch_hop_size,
        static_cast<uint_t>(sample_rate));

    if (!p->pitch_detector)
        return false;

    aubio_pitch_set_unit(p->pitch_detector, "Hz");
    aubio_pitch_set_silence(p->pitch_detector, -40.0f);
    aubio_pitch_set_tolerance(p->pitch_detector, 0.15f);

    p->pitch_input  = new_fvec(p->pitch_hop_size);
    p->pitch_output = new_fvec(1);

    p->pitch_accum.resize(p->pitch_hop_size, 0.0f);
    p->pitch_accum_pos = 0;
    p->current_pitch_hz = 0.0f;
    p->current_confidence = 0.0f;

    // --- Grain pitch shifter setup ---
    uint32_t grain = 256;
    for (int ch = 0; ch < 2; ++ch) {
        p->shifter[ch].grain_size = grain;
        p->shifter[ch].reset();
    }

    p->mono_buf.resize(max_frames);
    p->total_latency = 0;

    return true;
}

static void plugin_deactivate(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);

    if (p->pitch_detector) {
        del_aubio_pitch(p->pitch_detector);
        p->pitch_detector = nullptr;
    }
    if (p->pitch_input) {
        del_fvec(p->pitch_input);
        p->pitch_input = nullptr;
    }
    if (p->pitch_output) {
        del_fvec(p->pitch_output);
        p->pitch_output = nullptr;
    }
}

static bool plugin_start_processing(const clap_plugin_t *) { return true; }
static void plugin_stop_processing(const clap_plugin_t *) {}

static void plugin_reset(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    for (int ch = 0; ch < 2; ++ch)
        p->shifter[ch].reset();
    p->pitch_accum_pos = 0;
    p->current_pitch_hz = 0.0f;
    p->current_confidence = 0.0f;
}

static clap_process_status plugin_process(const clap_plugin_t *plugin,
                                           const clap_process_t *process) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    return silvertune_process(p, process);
}

static const void *plugin_get_extension(const clap_plugin_t *, const char *id) {
    if (strcmp(id, CLAP_EXT_PARAMS) == 0)       return &silvertune_params;
    if (strcmp(id, CLAP_EXT_STATE) == 0)        return &silvertune_state;
    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)  return &silvertune_audio_ports;
    if (strcmp(id, CLAP_EXT_LATENCY) == 0)      return &silvertune_latency;
    return nullptr;
}

static void plugin_on_main_thread(const clap_plugin_t *) {}

// --- Plugin creation ---

const clap_plugin_t *silvertune_create(const clap_host_t *host) {
    auto *p = new SilvertunePlugin();
    p->host = host;

    auto *clap = new clap_plugin_t();
    clap->desc             = &silvertune_descriptor;
    clap->plugin_data      = p;
    clap->init             = plugin_init;
    clap->destroy          = plugin_destroy;
    clap->activate         = plugin_activate;
    clap->deactivate       = plugin_deactivate;
    clap->start_processing = plugin_start_processing;
    clap->stop_processing  = plugin_stop_processing;
    clap->reset            = plugin_reset;
    clap->process          = plugin_process;
    clap->get_extension    = plugin_get_extension;
    clap->on_main_thread   = plugin_on_main_thread;

    p->clap_plugin = clap;
    return clap;
}

// --- CLAP entry point ---

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t *) {
    return 1;
}

static const clap_plugin_descriptor_t *factory_get_descriptor(
    const clap_plugin_factory_t *, uint32_t index) {
    return (index == 0) ? &silvertune_descriptor : nullptr;
}

static const clap_plugin_t *factory_create_plugin(
    const clap_plugin_factory_t *, const clap_host_t *host, const char *plugin_id) {
    if (strcmp(plugin_id, silvertune_descriptor.id) == 0)
        return silvertune_create(host);
    return nullptr;
}

static const clap_plugin_factory_t silvertune_factory = {
    .get_plugin_count      = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_descriptor,
    .create_plugin         = factory_create_plugin,
};

bool silvertune_entry_init(const char *) { return true; }
void silvertune_entry_deinit(void) {}

const void *silvertune_entry_get_factory(const char *factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
        return &silvertune_factory;
    return nullptr;
}

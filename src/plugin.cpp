#include "silvertune.h"
#include "gui.h"
#include <clap/ext/gui.h>
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
    p->yin.init(static_cast<float>(sample_rate));
    p->held_ratio = 1.0f;

    // --- Grain pitch shifter setup ---
    for (int ch = 0; ch < 2; ++ch) {
        p->shifter[ch].grain_size = 512;
        p->shifter[ch].reset();
        p->doubler[ch].grain_size = 512;
        p->doubler[ch].reset();
    }

    p->mono_buf.resize(max_frames);
    p->total_latency = 0;

    return true;
}

static void plugin_deactivate(const clap_plugin_t *) {}

static bool plugin_start_processing(const clap_plugin_t *) { return true; }
static void plugin_stop_processing(const clap_plugin_t *) {}

static void plugin_reset(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    for (int ch = 0; ch < 2; ++ch) {
        p->shifter[ch].reset();
        p->doubler[ch].reset();
    }
    p->yin.init(static_cast<float>(p->sample_rate));
    p->held_ratio = 1.0f;
}

static clap_process_status plugin_process(const clap_plugin_t *plugin,
                                           const clap_process_t *process) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    return silvertune_process(p, process);
}

// --- GUI extension ---

static bool gui_is_api_supported(const clap_plugin_t *, const char *api, bool is_floating) {
    if (is_floating) return false;
#if defined(_WIN32)
    return strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
#elif defined(__APPLE__)
    return strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
#else
    return strcmp(api, CLAP_WINDOW_API_X11) == 0;
#endif
}

static bool gui_get_preferred_api(const clap_plugin_t *, const char **api, bool *is_floating) {
    *is_floating = false;
#if defined(_WIN32)
    *api = CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    *api = CLAP_WINDOW_API_COCOA;
#else
    *api = CLAP_WINDOW_API_X11;
#endif
    return true;
}

static bool gui_create_cb(const clap_plugin_t *plugin, const char *, bool is_floating) {
    if (is_floating) return false;
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    p->host_params = static_cast<const clap_host_params_t *>(
        p->host->get_extension(p->host, CLAP_EXT_PARAMS));
    gui_create(p);
    return true;
}

static void gui_destroy_cb(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    gui_destroy(p);
}

static bool gui_set_scale_cb(const clap_plugin_t *plugin, double scale) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    gui_set_scale(p, scale);
    return true;
}

static bool gui_get_size(const clap_plugin_t *, uint32_t *width, uint32_t *height) {
    *width  = GUI_W;
    *height = GUI_H;
    return true;
}

static bool gui_can_resize(const clap_plugin_t *) { return false; }

static bool gui_get_resize_hints(const clap_plugin_t *, clap_gui_resize_hints_t *) {
    return false;
}

static bool gui_adjust_size(const clap_plugin_t *, uint32_t *, uint32_t *) { return true; }
static bool gui_set_size(const clap_plugin_t *, uint32_t, uint32_t) { return true; }

static bool gui_set_parent_cb(const clap_plugin_t *plugin, const clap_window_t *window) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    void *native = nullptr;
#if defined(_WIN32)
    native = window->win32;
#elif defined(__APPLE__)
    native = window->cocoa;
#else
    native = reinterpret_cast<void *>(static_cast<uintptr_t>(window->x11));
#endif
    return gui_set_parent(p, native);
}

static bool gui_set_transient(const clap_plugin_t *, const clap_window_t *) { return true; }
static void gui_suggest_title(const clap_plugin_t *, const char *) {}

static bool gui_show_cb(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    gui_show(p);
    return true;
}

static bool gui_hide_cb(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    gui_hide(p);
    return true;
}

static const clap_plugin_gui_t silvertune_gui = {
    .is_api_supported  = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create            = gui_create_cb,
    .destroy           = gui_destroy_cb,
    .set_scale         = gui_set_scale_cb,
    .get_size          = gui_get_size,
    .can_resize        = gui_can_resize,
    .get_resize_hints  = gui_get_resize_hints,
    .adjust_size       = gui_adjust_size,
    .set_size          = gui_set_size,
    .set_parent        = gui_set_parent_cb,
    .set_transient     = gui_set_transient,
    .suggest_title     = gui_suggest_title,
    .show              = gui_show_cb,
    .hide              = gui_hide_cb,
};

static const void *plugin_get_extension(const clap_plugin_t *, const char *id) {
    if (strcmp(id, CLAP_EXT_PARAMS) == 0)       return &silvertune_params;
    if (strcmp(id, CLAP_EXT_STATE) == 0)        return &silvertune_state;
    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)  return &silvertune_audio_ports;
    if (strcmp(id, CLAP_EXT_LATENCY) == 0)      return &silvertune_latency;
    if (strcmp(id, CLAP_EXT_GUI) == 0)          return &silvertune_gui;
    return nullptr;
}

static void plugin_on_main_thread(const clap_plugin_t *plugin) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    if (p->host_params)
        p->host_params->request_flush(p->host);
}

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

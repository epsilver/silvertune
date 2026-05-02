#include "silvertune.h"
#include <cmath>
#include <cstring>
#include <cstdio>

static const char *key_names[] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

static const char *scale_names[] = {
    "Major", "Minor", "Chromatic"
};

// --- params extension ---

static uint32_t params_count(const clap_plugin_t *) {
    return PARAM_COUNT;
}

static bool params_get_info(const clap_plugin_t *, uint32_t index, clap_param_info_t *info) {
    memset(info, 0, sizeof(*info));
    switch (index) {
    case PARAM_KEY:
        info->id = PARAM_KEY;
        info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE;
        strncpy(info->name, "Key", CLAP_NAME_SIZE);
        info->module[0] = '\0';
        info->min_value = 0;
        info->max_value = 11;
        info->default_value = 0;
        return true;

    case PARAM_SCALE:
        info->id = PARAM_SCALE;
        info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE;
        strncpy(info->name, "Scale", CLAP_NAME_SIZE);
        info->module[0] = '\0';
        info->min_value = 0;
        info->max_value = SCALE_COUNT - 1;
        info->default_value = 0;
        return true;

    case PARAM_WIDE:
        info->id = PARAM_WIDE;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        strncpy(info->name, "Wide", CLAP_NAME_SIZE);
        info->module[0] = '\0';
        info->min_value = 0.0;
        info->max_value = 1.0;
        info->default_value = 0.0;
        return true;

    case PARAM_SPEED:
        info->id = PARAM_SPEED;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        strncpy(info->name, "Speed", CLAP_NAME_SIZE);
        info->module[0] = '\0';
        info->min_value = 0.0;
        info->max_value = 100.0;
        info->default_value = 0.0;
        return true;

    case PARAM_HOLD:
        info->id = PARAM_HOLD;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        strncpy(info->name, "Hold", CLAP_NAME_SIZE);
        info->module[0] = '\0';
        info->min_value = 0.0;
        info->max_value = 200.0;
        info->default_value = 0.0;
        return true;

    default:
        return false;
    }
}

static bool params_get_value(const clap_plugin_t *plugin, clap_id param_id, double *value) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    switch (param_id) {
    case PARAM_KEY:   *value = p->param_key.load();   return true;
    case PARAM_SCALE: *value = p->param_scale.load(); return true;
    case PARAM_WIDE:  *value = p->param_wide.load();  return true;
    case PARAM_SPEED: *value = p->param_speed.load(); return true;
    case PARAM_HOLD:  *value = p->param_hold.load();  return true;
    default: return false;
    }
}

static bool params_value_to_text(const clap_plugin_t *, clap_id param_id, double value,
                                  char *buf, uint32_t buf_size) {
    switch (param_id) {
    case PARAM_KEY: {
        int idx = (int)std::lround(value);
        if (idx < 0 || idx > 11) return false;
        snprintf(buf, buf_size, "%s", key_names[idx]);
        return true;
    }
    case PARAM_SCALE: {
        int idx = (int)std::lround(value);
        if (idx < 0 || idx >= SCALE_COUNT) return false;
        snprintf(buf, buf_size, "%s", scale_names[idx]);
        return true;
    }
    case PARAM_WIDE:
        snprintf(buf, buf_size, "%.0f%%", value * 100.0);
        return true;
    case PARAM_SPEED:
    case PARAM_HOLD:
        if (value <= 0.0) snprintf(buf, buf_size, "0ms");
        else snprintf(buf, buf_size, "%.0fms", value);
        return true;
    default:
        return false;
    }
}

static bool params_text_to_value(const clap_plugin_t *, clap_id param_id,
                                  const char *text, double *value) {
    switch (param_id) {
    case PARAM_KEY:
        for (int i = 0; i < 12; ++i) {
            if (strcmp(text, key_names[i]) == 0) {
                *value = i;
                return true;
            }
        }
        return false;
    case PARAM_SCALE:
        for (int i = 0; i < SCALE_COUNT; ++i) {
            if (strcmp(text, scale_names[i]) == 0) {
                *value = i;
                return true;
            }
        }
        return false;
    case PARAM_WIDE: {
        double v;
        if (sscanf(text, "%lf", &v) == 1) {
            if (v > 1.0) v /= 100.0;
            *value = v;
            return true;
        }
        return false;
    }
    case PARAM_SPEED:
    case PARAM_HOLD: {
        double v;
        if (sscanf(text, "%lf", &v) == 1) { *value = v; return true; }
        return false;
    }
    default:
        return false;
    }
}

static void params_flush(const clap_plugin_t *plugin,
                          const clap_input_events_t *in,
                          const clap_output_events_t *) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; ++i) {
        auto *hdr = in->get(in, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID)
            continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            auto *ev = reinterpret_cast<const clap_event_param_value_t *>(hdr);
            switch (ev->param_id) {
            case PARAM_KEY:   p->param_key.store(ev->value);   break;
            case PARAM_SCALE: p->param_scale.store(ev->value); break;
            case PARAM_WIDE:  p->param_wide.store(ev->value);  break;
            case PARAM_SPEED: p->param_speed.store(ev->value); break;
            case PARAM_HOLD:  p->param_hold.store(ev->value);  break;
            }
        }
    }
}

extern const clap_plugin_params_t silvertune_params;
const clap_plugin_params_t silvertune_params = {
    .count          = params_count,
    .get_info       = params_get_info,
    .get_value      = params_get_value,
    .value_to_text  = params_value_to_text,
    .text_to_value  = params_text_to_value,
    .flush          = params_flush,
};

// --- state extension ---

static bool state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    double values[PARAM_COUNT] = {
        p->param_key.load(),
        p->param_scale.load(),
        p->param_wide.load(),
        p->param_speed.load(),
        p->param_hold.load(),
    };
    int64_t written = 0;
    int64_t total = sizeof(values);
    while (written < total) {
        int64_t n = stream->write(stream,
                                   reinterpret_cast<const char *>(values) + written,
                                   total - written);
        if (n < 0) return false;
        written += n;
    }
    return true;
}

static bool state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
    auto *p = static_cast<SilvertunePlugin *>(plugin->plugin_data);
    double values[PARAM_COUNT] = {};
    int64_t read_total = 0;
    int64_t total = (int64_t)sizeof(values);
    while (read_total < total) {
        int64_t n = stream->read(stream,
                                  reinterpret_cast<char *>(values) + read_total,
                                  total - read_total);
        if (n <= 0) break;
        read_total += n;
    }
    // Accept partial reads for forward/backward compat
    if (read_total < (int64_t)(4 * sizeof(double))) return false;
    p->param_key.store(values[PARAM_KEY]);
    p->param_scale.store(values[PARAM_SCALE]);
    p->param_wide.store(values[PARAM_WIDE]);
    p->param_speed.store(values[PARAM_SPEED]);
    if (read_total >= (int64_t)(5 * sizeof(double)))
        p->param_hold.store(values[PARAM_HOLD]);
    return true;
}

extern const clap_plugin_state_t silvertune_state;
const clap_plugin_state_t silvertune_state = {
    .save = state_save,
    .load = state_load,
};

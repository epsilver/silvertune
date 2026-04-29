#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include <clap/clap.h>
#include "yin.h"

// Parameter IDs
enum {
    PARAM_KEY   = 0,
    PARAM_SCALE = 1,
    PARAM_MIX   = 2,
    PARAM_SPEED = 3,
    PARAM_COUNT = 4
};

// Scale types
enum ScaleType {
    SCALE_MAJOR     = 0,
    SCALE_MINOR     = 1,
    SCALE_CHROMATIC = 2,
    SCALE_COUNT     = 3
};

// Scale logic
int quantize_to_scale(int midi_note, int root_key, ScaleType scale);
float midi_to_hz(float midi);
float hz_to_midi(float hz);

// Two-tap grain pitch shifter (per channel)
struct GrainShifter {
    static constexpr uint32_t BUF_SIZE = 4096;
    static constexpr uint32_t MASK = BUF_SIZE - 1;

    float buf[BUF_SIZE] = {};
    uint32_t write_pos = 0;
    double phase_a = 0.0;
    double phase_b = 0.5;
    uint32_t grain_size = 256;

    void reset() {
        for (auto &s : buf) s = 0.0f;
        write_pos = 0;
        phase_a = 0.0;
        phase_b = 0.5;
    }

    float process(float in, double pitch_ratio);
};

// Plugin instance data
struct SilvertunePlugin {
    const clap_plugin_t   *clap_plugin;
    const clap_host_t     *host;

    // Parameters (atomic for thread safety between main/audio)
    std::atomic<double> param_key{0.0};
    std::atomic<double> param_scale{0.0};
    std::atomic<double> param_mix{0.5};
    std::atomic<double> param_speed{0.0};

    // Audio state
    double sample_rate = 48000.0;
    uint32_t max_frames = 1024;

    // Pitch detection (YIN)
    YinDetector yin;
    float held_ratio = 1.0f;

    // Grain pitch shifters (one per channel)
    GrainShifter shifter[2];

    // Internal buffers
    std::vector<float> mono_buf;

    // Latency
    uint32_t total_latency = 0;
};

// CLAP callbacks
extern const clap_plugin_descriptor_t silvertune_descriptor;
extern const clap_plugin_params_t silvertune_params;
extern const clap_plugin_state_t  silvertune_state;

const clap_plugin_t *silvertune_create(const clap_host_t *host);
clap_process_status silvertune_process(SilvertunePlugin *p, const clap_process_t *process);

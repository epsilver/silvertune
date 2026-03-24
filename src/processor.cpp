#include "silvertune.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Grain pitch shifter ---

static inline float hann(double phase) {
    return 0.5f * (1.0f - std::cos(2.0 * M_PI * phase));
}

static inline float lerp_read(const float *buf, uint32_t write_pos, double delay, uint32_t mask) {
    double read_pos = static_cast<double>(write_pos) - delay;
    double wrapped = std::fmod(read_pos, static_cast<double>(mask + 1));
    if (wrapped < 0) wrapped += (mask + 1);

    uint32_t i0 = static_cast<uint32_t>(wrapped) & mask;
    uint32_t i1 = (i0 + 1) & mask;
    float frac = static_cast<float>(wrapped - std::floor(wrapped));
    return buf[i0] * (1.0f - frac) + buf[i1] * frac;
}

float GrainShifter::process(float in, double pitch_ratio) {
    buf[write_pos & MASK] = in;
    write_pos++;

    // To pitch UP (ratio > 1), delay must DECREASE so the read head catches
    // up to the write head. Sign: (1 - ratio).
    double phase_inc = (1.0 - pitch_ratio) / static_cast<double>(grain_size);

    phase_a += phase_inc;
    phase_b += phase_inc;

    phase_a -= std::floor(phase_a);
    phase_b -= std::floor(phase_b);

    double delay_a = phase_a * grain_size;
    double delay_b = phase_b * grain_size;

    constexpr double MIN_DELAY = 2.0;
    delay_a += MIN_DELAY;
    delay_b += MIN_DELAY;

    float sample_a = lerp_read(buf, write_pos, delay_a, MASK);
    float sample_b = lerp_read(buf, write_pos, delay_b, MASK);

    float gain_a = hann(phase_a);
    float gain_b = hann(phase_b);

    return sample_a * gain_a + sample_b * gain_b;
}

// --- Parameter event handling ---

static void process_events(SilvertunePlugin *p, const clap_input_events_t *in) {
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
            case PARAM_MIX:   p->param_mix.store(ev->value);   break;
            case PARAM_SPEED: p->param_speed.store(ev->value);  break;
            }
        }
    }
}

// --- Main process ---

clap_process_status silvertune_process(SilvertunePlugin *p, const clap_process_t *process) {
    if (process->audio_inputs_count == 0 || process->audio_outputs_count == 0)
        return CLAP_PROCESS_CONTINUE;

    const uint32_t frames = process->frames_count;
    const auto &in_buf  = process->audio_inputs[0];
    auto &out_buf = process->audio_outputs[0];
    const uint32_t num_channels = std::min(in_buf.channel_count, out_buf.channel_count);

    if (num_channels == 0 || frames == 0)
        return CLAP_PROCESS_CONTINUE;

    process_events(p, process->in_events);

    int root_key = static_cast<int>(p->param_key.load());
    auto scale = static_cast<ScaleType>(static_cast<int>(p->param_scale.load()));
    float mix = static_cast<float>(p->param_mix.load());
    float speed = static_cast<float>(p->param_speed.load());

    // Sum to mono for pitch detection
    for (uint32_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (uint32_t ch = 0; ch < num_channels; ++ch)
            sum += in_buf.data32[ch][i];
        p->mono_buf[i] = sum / static_cast<float>(num_channels);
    }

    // Feed samples into pitch detection accumulator
    for (uint32_t i = 0; i < frames; ++i) {
        p->pitch_accum[p->pitch_accum_pos++] = p->mono_buf[i];

        if (p->pitch_accum_pos >= p->pitch_hop_size) {
            for (uint32_t j = 0; j < p->pitch_hop_size; ++j)
                p->pitch_input->data[j] = p->pitch_accum[j];

            aubio_pitch_do(p->pitch_detector, p->pitch_input, p->pitch_output);
            p->current_pitch_hz = p->pitch_output->data[0];
            p->current_confidence = aubio_pitch_get_confidence(p->pitch_detector);
            p->pitch_accum_pos = 0;
        }
    }

    // Determine pitch ratio
    float pitch_ratio = 1.0f;

    if (p->current_pitch_hz > 50.0f && p->current_pitch_hz < 2000.0f &&
        p->current_confidence > 0.6f) {
        float detected_midi = hz_to_midi(p->current_pitch_hz);
        int nearest_midi = quantize_to_scale(
            static_cast<int>(std::round(detected_midi)), root_key, scale);
        float target_hz = midi_to_hz(static_cast<float>(nearest_midi));

        pitch_ratio = target_hz / p->current_pitch_hz;
        pitch_ratio = 1.0f + (pitch_ratio - 1.0f) * (1.0f - speed);
        pitch_ratio = std::clamp(pitch_ratio, 0.5f, 2.0f);
    }

    // Process each sample through the grain shifter
    for (uint32_t i = 0; i < frames; ++i) {
        for (uint32_t ch = 0; ch < num_channels; ++ch) {
            float dry = in_buf.data32[ch][i];
            float wet = p->shifter[ch].process(dry, static_cast<double>(pitch_ratio));
            out_buf.data32[ch][i] = dry * (1.0f - mix) + wet * mix;
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

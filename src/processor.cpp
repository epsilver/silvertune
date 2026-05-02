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
            case PARAM_WIDE:  p->param_wide.store(ev->value);   break;
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

    int root_key = (int)std::lround(p->param_key.load());
    auto scale = static_cast<ScaleType>((int)std::lround(p->param_scale.load()));
    float wide = static_cast<float>(p->param_wide.load());
    float speed = static_cast<float>(p->param_speed.load());

    // Sum to mono for pitch detection
    for (uint32_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (uint32_t ch = 0; ch < num_channels; ++ch)
            sum += in_buf.data32[ch][i];
        p->mono_buf[i] = sum / static_cast<float>(num_channels);
    }

    // Feed samples into YIN — run detection inline whenever buffer fills
    for (uint32_t i = 0; i < frames; ++i) {
        p->yin.push_sample(p->mono_buf[i]);
        if (p->yin.pending) {
            p->yin.run_detect();
            float hz   = p->yin.pitch_hz;
            float conf = p->yin.confidence;

            if (hz > 50.0f && hz < 2000.0f && conf > 0.5f) {
                float det_midi = hz_to_midi(hz);
                // Stay locked if within 40 cents of the locked note
                bool stays_locked = p->locked_midi >= 0.0f &&
                                    std::fabs(det_midi - p->locked_midi) < 0.4f;
                if (!stays_locked) p->locked_midi = det_midi;
                p->low_conf_count = 0;

                int det_note_int  = static_cast<int>(std::round(p->locked_midi));
                int nearest = quantize_to_scale(det_note_int, root_key, scale);
                float target_hz = midi_to_hz(static_cast<float>(nearest));
                float ratio = target_hz / hz;
                ratio = 1.0f + (ratio - 1.0f) * speed;
                p->held_ratio = std::clamp(ratio, 0.5f, 2.0f);

                // Update GUI display state
                p->gui_det.store(det_note_int,        std::memory_order_relaxed);
                p->gui_corr.store(nearest,             std::memory_order_relaxed);
                p->gui_det_midi.store(p->locked_midi,  std::memory_order_relaxed);
                p->gui_det_frame.fetch_add(1,          std::memory_order_relaxed);
            } else if (conf < 0.35f) {
                if (++p->low_conf_count >= 3) {
                    p->locked_midi    = -1.0f;
                    p->low_conf_count = 0;
                    p->held_ratio     = 1.0f;
                    p->gui_det.store(-1, std::memory_order_relaxed);
                    p->gui_corr.store(-1, std::memory_order_relaxed);
                    p->gui_det_midi.store(-1.0f, std::memory_order_relaxed);
                }
            }
        }
    }

    // Compute RMS for GUI metering
    {
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < frames; ++i)
            sum_sq += p->mono_buf[i] * p->mono_buf[i];
        float rms = std::sqrt(sum_sq / static_cast<float>(frames));
        p->gui_rms.store(rms, std::memory_order_relaxed);
    }

    float pitch_ratio = p->held_ratio;

    // Process each sample through the grain shifter + doubler
    for (uint32_t i = 0; i < frames; ++i) {
        for (uint32_t ch = 0; ch < num_channels; ++ch) {
            float dry = in_buf.data32[ch][i];
            float wet = p->shifter[ch].process(dry, static_cast<double>(pitch_ratio));
            float dbl = p->doubler[ch].process(dry, static_cast<double>(pitch_ratio) * DETUNE);
            out_buf.data32[ch][i] = wet + dbl * wide;
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

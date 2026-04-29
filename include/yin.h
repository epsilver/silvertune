#pragma once

#include <cstdint>

class YinDetector {
public:
    static constexpr uint32_t BUF_SIZE = 1024;
    static constexpr uint32_t HALF     = BUF_SIZE / 2;
    static constexpr uint32_t HOP_SIZE = 512;

    float pitch_hz   = 0.0f;
    float confidence = 0.0f;
    volatile bool pending = false;

    void init(float sample_rate);
    void push_sample(float sample);  // call from audio callback — fast, no detection
    void run_detect();               // call from main loop when pending is true

private:
    float sr_ = 48000.0f;
    float buf_[BUF_SIZE] = {};
    uint32_t pos_ = 0;
    float diff_[HALF] = {};

    void detect();
};

// Block of audio sent from the processor (audio thread) to the controller
// (UI thread) for the panel's wave display.
#pragma once

#include <cstdint>

namespace retro80 {

struct ScopeBlock {
    static constexpr uint32_t kMagic = 0x52385343;  // 'R8SC'
    static constexpr int kSamples = 1024;

    uint32_t magic = kMagic;
    int32_t activeVoices = 0;
    float sampleRate = 48000.0f;
    float pitchHz = 0.0f;  // lowest playing note, for a steady display
    float samples[kSamples];
};

} // namespace retro80

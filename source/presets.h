// Factory patches. Each patch starts from the parameter defaults and
// overrides a few values (plain units: Hz, seconds, 0..1, list index...).
#pragma once

#include "params.h"

#include <array>
#include <initializer_list>

namespace retro80 {

struct PatchValue {
    ParamId id;
    float plain;
};

struct FactoryPatch {
    const char* name;
    std::initializer_list<PatchValue> values;
};

inline const FactoryPatch kFactoryPatches[] = {
    {"Init", {}},

    {"Juno Brass",
     {{kOsc1Wave, 0}, {kMixOsc1, 1}, {kMixSub, 0.35f},
      {kCutoff, 650}, {kReso, 0.15f}, {kFiltEnvAmt, 0.62f}, {kFiltKeytrack, 0.6f}, {kFiltVel, 0.4f},
      {kFEnvA, 0.06f}, {kFEnvD, 0.55f}, {kFEnvS, 0.45f}, {kFEnvR, 0.35f},
      {kAEnvA, 0.03f}, {kAEnvD, 0.6f}, {kAEnvS, 0.85f}, {kAEnvR, 0.35f},
      {kChorus, 1}}},

    {"Dream Pad",
     {{kOsc1Wave, 2}, {kOsc1PW, 0.35f}, {kOsc1PWM, 0.45f}, {kMixOsc2, 0.6f}, {kOsc2Wave, 0}, {kOsc2Fine, 9},
      {kHpf, 120}, {kCutoff, 1800}, {kReso, 0.2f}, {kFiltEnvAmt, 0.15f}, {kFiltLfoAmt, 0.08f},
      {kFEnvA, 1.5f}, {kFEnvD, 2.0f}, {kFEnvS, 0.6f}, {kFEnvR, 2.0f},
      {kAEnvA, 0.9f}, {kAEnvD, 1.5f}, {kAEnvS, 0.9f}, {kAEnvR, 2.2f},
      {kLfoWave, 0}, {kLfoRate, 0.35f}, {kChorus, 2}, {kDrift, 0.45f}}},

    {"Bright Keys",
     {{kOsc1Wave, 1}, {kOsc1PW, 0.3f}, {kMixOsc2, 0.4f}, {kOsc2Wave, 2}, {kOsc2Semi, 12}, {kOsc2Fine, 3},
      {kCutoff, 900}, {kReso, 0.12f}, {kFiltEnvAmt, 0.55f}, {kFiltKeytrack, 0.8f}, {kFiltVel, 0.6f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.7f}, {kFEnvS, 0.25f}, {kFEnvR, 0.5f},
      {kAEnvA, 0.002f}, {kAEnvD, 1.6f}, {kAEnvS, 0.35f}, {kAEnvR, 0.5f}, {kAmpVel, 0.7f},
      {kChorus, 1}}},

    {"Strings 1983",
     {{kOsc1Wave, 0}, {kMixOsc2, 0.8f}, {kOsc2Wave, 0}, {kOsc2Fine, -8},
      {kHpf, 220}, {kCutoff, 3200}, {kReso, 0.05f}, {kFiltEnvAmt, 0.1f},
      {kFEnvA, 0.4f}, {kFEnvD, 1.0f}, {kFEnvS, 0.8f}, {kFEnvR, 1.0f},
      {kAEnvA, 0.45f}, {kAEnvD, 1.0f}, {kAEnvS, 1.0f}, {kAEnvR, 1.1f},
      {kLfoWave, 1}, {kLfoRate, 5.5f}, {kLfoDelay, 0.6f}, {kLfoPitch, 0.08f},
      {kChorus, 3}, {kDrift, 0.4f}}},

    {"Synthwave Lead",
     {{kVolume, 0.95f}, {kVoiceMode, 2}, {kUnisonVoices, 4}, {kUnisonDetune, 0.35f}, {kGlide, 0.06f},
      {kOsc1Wave, 0}, {kMixSub, 0.25f},
      {kCutoff, 2400}, {kReso, 0.3f}, {kFiltEnvAmt, 0.35f}, {kFiltKeytrack, 0.7f},
      {kFEnvA, 0.01f}, {kFEnvD, 0.5f}, {kFEnvS, 0.55f}, {kFEnvR, 0.3f},
      {kAEnvA, 0.005f}, {kAEnvD, 0.3f}, {kAEnvS, 0.9f}, {kAEnvR, 0.25f},
      {kLfoWave, 1}, {kLfoRate, 5.8f}, {kLfoDelay, 0.45f}, {kLfoPitch, 0.12f},
      {kChorus, 1}}},

    {"Square Solo",
     {{kVolume, 0.85f}, {kVoiceMode, 1}, {kGlide, 0.09f},
      {kOsc1Wave, 1}, {kOsc1PW, 0.42f}, {kMixSub, 0.4f}, {kMixOsc2, 0.3f}, {kOsc2Wave, 1}, {kOsc2Semi, 12}, {kOsc2Fine, 5},
      {kCutoff, 1500}, {kReso, 0.35f}, {kFiltEnvAmt, 0.4f}, {kFiltKeytrack, 0.7f},
      {kFEnvA, 0.005f}, {kFEnvD, 0.35f}, {kFEnvS, 0.5f}, {kFEnvR, 0.2f},
      {kAEnvA, 0.004f}, {kAEnvD, 0.3f}, {kAEnvS, 0.95f}, {kAEnvR, 0.18f},
      {kLfoWave, 0}, {kLfoRate, 6.2f}, {kLfoDelay, 0.5f}, {kLfoPitch, 0.1f},
      {kChorus, 0}}},

    {"Poly Pluck",
     {{kVolume, 0.95f}, {kOsc1Wave, 0}, {kMixOsc2, 0.5f}, {kOsc2Wave, 1}, {kOsc2Fine, -6},
      {kCutoff, 300}, {kReso, 0.35f}, {kFiltEnvAmt, 0.7f}, {kFiltKeytrack, 0.6f}, {kFiltVel, 0.5f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.28f}, {kFEnvS, 0.0f}, {kFEnvR, 0.3f},
      {kAEnvA, 0.001f}, {kAEnvD, 0.6f}, {kAEnvS, 0.0f}, {kAEnvR, 0.45f}, {kAmpVel, 0.6f},
      {kChorus, 3}}},

    {"Analog Bass",
     {{kVolume, 0.9f}, {kVoiceMode, 1}, {kGlide, 0.03f},
      {kOsc1Wave, 0}, {kMixSub, 0.8f}, {kMixOsc2, 0.4f}, {kOsc2Wave, 1}, {kOsc2Semi, -12}, {kOsc2Fine, 0},
      {kCutoff, 350}, {kReso, 0.3f}, {kFiltEnvAmt, 0.55f}, {kFiltKeytrack, 0.4f}, {kFiltVel, 0.4f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.3f}, {kFEnvS, 0.2f}, {kFEnvR, 0.12f},
      {kAEnvA, 0.001f}, {kAEnvD, 0.4f}, {kAEnvS, 0.9f}, {kAEnvR, 0.1f},
      {kChorus, 0}, {kDrift, 0.2f}}},
};

constexpr int kNumFactoryPatches = static_cast<int>(sizeof(kFactoryPatches) / sizeof(kFactoryPatches[0]));

// Normalized values for a whole patch (defaults + overrides).
inline std::array<double, kNumPatchParams> patchNormalized(int index) {
    std::array<double, kNumPatchParams> out{};
    for (int i = 0; i < kNumPatchParams; ++i) out[i] = defaultNormalized(kParamDefs[i]);
    if (index < 0 || index >= kNumFactoryPatches) return out;
    for (const PatchValue& v : kFactoryPatches[index].values)
        out[v.id] = toNormalized(kParamDefs[v.id], v.plain);
    return out;
}

} // namespace retro80

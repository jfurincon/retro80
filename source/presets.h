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
    const char* category;
};

// Browser columns, in order ("Init" is shown on its own).
inline const char* const kPatchCategories[] = {"Keys", "Pads", "Strings & Brass", "Leads", "Bass", "Arps & Plucks", "FX"};
constexpr int kNumPatchCategories = 7;

inline const FactoryPatch kFactoryPatches[] = {
    {"Init", {}, "Init"},

    {"Bright Keys",
     {{kRevMix, 0.15f}, {kRevDecay, 1.8f}, {kOsc1Wave, 1}, {kOsc1PW, 0.3f}, {kMixOsc2, 0.4f}, {kOsc2Wave, 2}, {kOsc2Semi, 12}, {kOsc2Fine, 3},
      {kCutoff, 900}, {kReso, 0.12f}, {kFiltEnvAmt, 0.55f}, {kFiltKeytrack, 0.8f}, {kFiltVel, 0.6f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.7f}, {kFEnvS, 0.25f}, {kFEnvR, 0.5f},
      {kAEnvA, 0.002f}, {kAEnvD, 1.6f}, {kAEnvS, 0.35f}, {kAEnvR, 0.5f}, {kAmpVel, 0.7f},
      {kChorus, 1}}, "Keys"},

    {"X-Mod Bell",
     {{kVolume, 0.85f}, {kOsc1Wave, 3}, {kOsc2Wave, 3}, {kOsc2Semi, 19}, {kOsc2Fine, 0}, {kMixOsc2, 0.0f},
      {kXMod, 0.42f},
      {kCutoff, 9000}, {kReso, 0.0f}, {kFiltEnvAmt, 0.0f}, {kFiltKeytrack, 0.3f},
      {kAEnvA, 0.001f}, {kAEnvD, 2.6f}, {kAEnvS, 0.0f}, {kAEnvR, 1.3f}, {kAmpVel, 0.6f},
      {kMod1Src, kSrcModEnv}, {kMod1Dst, kDstXMod}, {kMod1Amt, 0.4f}, {kMEnvD, 0.6f},
      {kChorus, 2}, {kRevMix, 0.35f}, {kRevDecay, 3.5f}, {kRevPreDelay, 0.025f}}, "Keys"},

    {"Glass EP",
     {{kOsc1Wave, 3}, {kOsc2Wave, 3}, {kOsc2Semi, 12}, {kOsc2Fine, 0}, {kMixOsc2, 0.45f}, {kXMod, 0.12f}, {kCutoff, 5200}, {kReso, 0}, {kFiltEnvAmt, 0.15f}, {kAEnvA, 0.002f}, {kAEnvD, 2.2f}, {kAEnvS, 0.25f}, {kAEnvR, 0.6f}, {kAmpVel, 0.75f}, {kMod1Src, kSrcVelocity}, {kMod1Dst, kDstXMod}, {kMod1Amt, 0.35f}, {kChorus, 2}, {kRevMix, 0.2f}, {kRevDecay, 2.2f}}, "Keys"},

    {"Chorus Piano",
     {{kOsc1Wave, 1}, {kOsc1PW, 0.35f}, {kMixOsc2, 0.3f}, {kOsc2Wave, 0}, {kOsc2Semi, 12}, {kOsc2Fine, 4}, {kCutoff, 1100}, {kFiltEnvAmt, 0.5f}, {kFiltKeytrack, 0.8f}, {kFiltVel, 0.6f}, {kFEnvA, 0.001f}, {kFEnvD, 0.9f}, {kFEnvS, 0.2f}, {kFEnvR, 0.5f}, {kAEnvA, 0.002f}, {kAEnvD, 2.0f}, {kAEnvS, 0.2f}, {kAEnvR, 0.5f}, {kAmpVel, 0.7f}, {kChorus, 3}, {kRevMix, 0.18f}, {kRevDecay, 2.4f}}, "Keys"},

    {"Clav Funk",
     {{kVolume, 1.0f}, {kOsc1Wave, 1}, {kOsc1PW, 0.12f}, {kMixSub, 0.15f}, {kCutoff, 700}, {kReso, 0.45f}, {kFiltEnvAmt, 0.6f}, {kFiltKeytrack, 0.7f}, {kFiltVel, 0.7f}, {kFEnvA, 0.001f}, {kFEnvD, 0.18f}, {kFEnvS, 0.1f}, {kFEnvR, 0.1f}, {kAEnvA, 0.001f}, {kAEnvD, 0.35f}, {kAEnvS, 0.4f}, {kAEnvR, 0.08f}, {kAmpVel, 0.7f}, {kChorus, 0}, {kDrift, 0.15f}}, "Keys"},

    {"Organ 85",
     {{kOsc1Wave, 4}, {kMixOsc2, 0.6f}, {kOsc2Wave, 2}, {kOsc2Semi, 12}, {kOsc2Fine, 0}, {kMixSub, 0.5f}, {kCutoff, 9000}, {kReso, 0}, {kFiltEnvAmt, 0}, {kAEnvA, 0.004f}, {kAEnvD, 0.1f}, {kAEnvS, 1.0f}, {kAEnvR, 0.06f}, {kAmpVel, 0}, {kLfoWave, 1}, {kLfoRate, 6.5f}, {kLfoPitch, 0.03f}, {kChorus, 2}, {kRevMix, 0.15f}, {kDrift, 0.1f}}, "Keys"},

    {"Toy Box",
     {{kVolume, 0.85f}, {kOsc1Wave, 4}, {kOsc2Wave, 3}, {kOsc2Semi, 24}, {kOsc2Fine, 0}, {kXMod, 0.55f}, {kCutoff, 12000}, {kReso, 0}, {kFiltEnvAmt, 0}, {kAEnvA, 0.001f}, {kAEnvD, 0.8f}, {kAEnvS, 0}, {kAEnvR, 0.6f}, {kMod1Src, kSrcModEnv}, {kMod1Dst, kDstXMod}, {kMod1Amt, 0.4f}, {kMEnvD, 0.25f}, {kChorus, 1}, {kDlyMix, 0.22f}, {kDlySync, 6}, {kDlyFeedback, 0.35f}, {kRevMix, 0.25f}}, "Keys"},

    {"Dream Pad",
     {{kRevMix, 0.32f}, {kRevDecay, 4.5f}, {kRevPreDelay, 0.03f}, {kOsc1Wave, 2}, {kOsc1PW, 0.35f}, {kOsc1PWM, 0.45f}, {kMixOsc2, 0.6f}, {kOsc2Wave, 0}, {kOsc2Fine, 9},
      {kHpf, 120}, {kCutoff, 1800}, {kReso, 0.2f}, {kFiltEnvAmt, 0.15f}, {kFiltLfoAmt, 0.08f},
      {kFEnvA, 1.5f}, {kFEnvD, 2.0f}, {kFEnvS, 0.6f}, {kFEnvR, 2.0f},
      {kAEnvA, 0.9f}, {kAEnvD, 1.5f}, {kAEnvS, 0.9f}, {kAEnvR, 2.2f},
      {kLfoWave, 0}, {kLfoRate, 0.35f}, {kChorus, 2}, {kDrift, 0.45f}}, "Pads"},

    {"Nebula Pad",
     {{kOsc1Wave, 2}, {kOsc1PW, 0.4f}, {kOsc1PWM, 0.35f}, {kMixOsc2, 0.7f}, {kOsc2Wave, 0}, {kOsc2Semi, -12}, {kOsc2Fine, 6},
      {kHpf, 90}, {kCutoff, 1400}, {kReso, 0.25f}, {kFiltEnvAmt, 0.2f},
      {kFEnvA, 2.0f}, {kFEnvD, 3.0f}, {kFEnvS, 0.5f}, {kFEnvR, 3.0f},
      {kAEnvA, 1.6f}, {kAEnvD, 2.0f}, {kAEnvS, 0.9f}, {kAEnvR, 3.5f},
      {kLfoWave, 0}, {kLfoRate, 0.3f},
      {kLfo2Wave, 1}, {kLfo2Rate, 0.12f},
      {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.25f},
      {kMod2Src, kSrcRandom}, {kMod2Dst, kDstPan}, {kMod2Amt, 0.7f},
      {kMod3Src, kSrcModWheel}, {kMod3Dst, kDstCutoff}, {kMod3Amt, 0.5f},
      {kChorus, 2}, {kDrift, 0.5f}, {kDlyMix, 0.15f}, {kDlySync, 9}, {kDlyFeedback, 0.5f},
      {kRevMix, 0.45f}, {kRevDecay, 7.0f}, {kRevDamp, 0.5f}, {kRevPreDelay, 0.04f}}, "Pads"},

    {"Warm Juno Pad",
     {{kOsc1Wave, 2}, {kOsc1PW, 0.45f}, {kOsc1PWM, 0.5f}, {kMixSub, 0.3f}, {kCutoff, 1600}, {kReso, 0.12f}, {kFiltEnvAmt, 0.12f}, {kFEnvA, 0.8f}, {kFEnvD, 2.0f}, {kFEnvS, 0.7f}, {kFEnvR, 1.5f}, {kAEnvA, 0.6f}, {kAEnvD, 1.0f}, {kAEnvS, 1.0f}, {kAEnvR, 1.6f}, {kLfoWave, 0}, {kLfoRate, 0.6f}, {kChorus, 3}, {kRevMix, 0.25f}, {kRevDecay, 3.5f}, {kDrift, 0.4f}}, "Pads"},

    {"Glass Pad",
     {{kOsc1Wave, 3}, {kMixOsc2, 0.7f}, {kOsc2Wave, 3}, {kOsc2Semi, 19}, {kOsc2Fine, 0}, {kHpf, 180}, {kCutoff, 7000}, {kReso, 0.05f}, {kFiltEnvAmt, 0}, {kAEnvA, 1.2f}, {kAEnvD, 2.0f}, {kAEnvS, 0.9f}, {kAEnvR, 3.0f}, {kLfo2Wave, 1}, {kLfo2Rate, 0.2f}, {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstOsc2Lvl}, {kMod1Amt, 0.4f}, {kChorus, 2}, {kRevMix, 0.45f}, {kRevDecay, 6.0f}, {kRevPreDelay, 0.05f}, {kDlyMix, 0.15f}, {kDlySync, 9}, {kDlyFeedback, 0.45f}}, "Pads"},

    {"Dark Choir",
     {{kVolume, 0.85f}, {kOsc1Wave, 0}, {kMixOsc2, 0.8f}, {kOsc2Wave, 0}, {kOsc2Fine, -11}, {kMixNoise, 0.12f}, {kHpf, 140}, {kCutoff, 900}, {kReso, 0.55f}, {kFiltEnvAmt, 0.1f}, {kFiltLfoAmt, 0.12f}, {kLfoWave, 1}, {kLfoRate, 0.25f}, {kAEnvA, 1.4f}, {kAEnvD, 2.0f}, {kAEnvS, 0.9f}, {kAEnvR, 2.4f}, {kChorus, 3}, {kRevMix, 0.4f}, {kRevDecay, 5.5f}, {kDrift, 0.5f}}, "Pads"},

    {"Sweep Pad",
     {{kVolume, 0.7f}, {kOsc1Wave, 0}, {kMixOsc2, 0.7f}, {kOsc2Wave, 1}, {kOsc2Fine, 6}, {kCutoff, 500}, {kReso, 0.45f}, {kFiltEnvAmt, 0}, {kLfo2Wave, 0}, {kLfo2Sync, 1}, {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.55f}, {kAEnvA, 0.8f}, {kAEnvD, 1.0f}, {kAEnvS, 1.0f}, {kAEnvR, 2.0f}, {kChorus, 1}, {kRevMix, 0.3f}, {kRevDecay, 4.0f}}, "Pads"},

    {"Sunset Swell",
     {{kOsc1Wave, 2}, {kOsc1PWM, 0.3f}, {kMixOsc2, 0.5f}, {kOsc2Semi, -12}, {kOsc2Wave, 0}, {kCutoff, 2200}, {kFiltEnvAmt, 0.25f}, {kFEnvA, 2.5f}, {kFEnvD, 2.0f}, {kFEnvS, 0.6f}, {kAEnvA, 2.2f}, {kAEnvD, 1.0f}, {kAEnvS, 1.0f}, {kAEnvR, 2.5f}, {kChorus, 2}, {kDlyMix, 0.25f}, {kDlySync, 10}, {kDlyFeedback, 0.5f}, {kRevMix, 0.35f}, {kRevDecay, 5.0f}}, "Pads"},

    {"Strings 1983",
     {{kRevMix, 0.25f}, {kRevDecay, 3.0f}, {kOsc1Wave, 0}, {kMixOsc2, 0.8f}, {kOsc2Wave, 0}, {kOsc2Fine, -8},
      {kHpf, 220}, {kCutoff, 3200}, {kReso, 0.05f}, {kFiltEnvAmt, 0.1f},
      {kFEnvA, 0.4f}, {kFEnvD, 1.0f}, {kFEnvS, 0.8f}, {kFEnvR, 1.0f},
      {kAEnvA, 0.45f}, {kAEnvD, 1.0f}, {kAEnvS, 1.0f}, {kAEnvR, 1.1f},
      {kLfoWave, 1}, {kLfoRate, 5.5f}, {kLfoDelay, 0.6f}, {kLfoPitch, 0.08f},
      {kChorus, 3}, {kDrift, 0.4f}}, "Strings & Brass"},

    {"Juno Brass",
     {{kRevMix, 0.12f}, {kRevDecay, 2.0f}, {kOsc1Wave, 0}, {kMixOsc1, 1}, {kMixSub, 0.35f},
      {kCutoff, 650}, {kReso, 0.15f}, {kFiltEnvAmt, 0.62f}, {kFiltKeytrack, 0.6f}, {kFiltVel, 0.4f},
      {kFEnvA, 0.06f}, {kFEnvD, 0.55f}, {kFEnvS, 0.45f}, {kFEnvR, 0.35f},
      {kAEnvA, 0.03f}, {kAEnvD, 0.6f}, {kAEnvS, 0.85f}, {kAEnvR, 0.35f},
      {kChorus, 1}}, "Strings & Brass"},

    {"Wheel Brass",
     {{kOsc1Wave, 0}, {kMixOsc2, 0.6f}, {kOsc2Wave, 0}, {kOsc2Fine, -9}, {kMixSub, 0.2f},
      {kCutoff, 500}, {kReso, 0.12f}, {kFiltEnvAmt, 0.55f}, {kFiltKeytrack, 0.6f}, {kFiltVel, 0.5f},
      {kFEnvA, 0.08f}, {kFEnvD, 0.6f}, {kFEnvS, 0.5f}, {kFEnvR, 0.4f},
      {kAEnvA, 0.04f}, {kAEnvD, 0.6f}, {kAEnvS, 0.85f}, {kAEnvR, 0.4f},
      {kMod1Src, kSrcModWheel}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.45f},
      {kMod2Src, kSrcAftertouch}, {kMod2Dst, kDstCutoff}, {kMod2Amt, 0.35f},
      {kMod3Src, kSrcVelocity}, {kMod3Dst, kDstOsc2Pitch}, {kMod3Amt, 0.0f},
      {kChorus, 1}, {kRevMix, 0.18f}, {kRevDecay, 2.2f}}, "Strings & Brass"},

    {"Lo-Fi Strings",
     {{kOsc1Wave, 1}, {kOsc1PW, 0.3f}, {kOsc1PWM, 0.55f}, {kMixOsc2, 0.6f}, {kOsc2Wave, 0}, {kOsc2Fine, -10}, {kHpf, 250}, {kCutoff, 2600}, {kFiltEnvAmt, 0.05f}, {kLfoWave, 0}, {kLfoRate, 0.8f}, {kAEnvA, 0.5f}, {kAEnvD, 1.0f}, {kAEnvS, 1.0f}, {kAEnvR, 1.2f}, {kChorus, 3}, {kDrift, 0.75f}, {kRevMix, 0.22f}}, "Strings & Brass"},

    {"Stab Brass",
     {{kOsc1Wave, 0}, {kMixOsc2, 0.7f}, {kOsc2Fine, -8}, {kCutoff, 450}, {kReso, 0.18f}, {kFiltEnvAmt, 0.7f}, {kFiltVel, 0.5f}, {kFEnvA, 0.015f}, {kFEnvD, 0.25f}, {kFEnvS, 0.25f}, {kFEnvR, 0.15f}, {kAEnvA, 0.008f}, {kAEnvD, 0.4f}, {kAEnvS, 0.6f}, {kAEnvR, 0.15f}, {kChorus, 1}, {kRevMix, 0.15f}}, "Strings & Brass"},

    {"Synthwave Lead",
     {{kDlyMix, 0.25f}, {kDlySync, 7}, {kDlyFeedback, 0.38f}, {kRevMix, 0.18f}, {kVolume, 0.95f}, {kVoiceMode, 2}, {kUnisonVoices, 4}, {kUnisonDetune, 0.35f}, {kGlide, 0.06f},
      {kOsc1Wave, 0}, {kMixSub, 0.25f},
      {kCutoff, 2400}, {kReso, 0.3f}, {kFiltEnvAmt, 0.35f}, {kFiltKeytrack, 0.7f},
      {kFEnvA, 0.01f}, {kFEnvD, 0.5f}, {kFEnvS, 0.55f}, {kFEnvR, 0.3f},
      {kAEnvA, 0.005f}, {kAEnvD, 0.3f}, {kAEnvS, 0.9f}, {kAEnvR, 0.25f},
      {kLfoWave, 1}, {kLfoRate, 5.8f}, {kLfoDelay, 0.45f}, {kLfoPitch, 0.12f},
      {kChorus, 1}}, "Leads"},

    {"Square Solo",
     {{kDlyMix, 0.2f}, {kDlySync, 6}, {kDlyFeedback, 0.3f}, {kRevMix, 0.1f}, {kVolume, 0.85f}, {kVoiceMode, 1}, {kGlide, 0.09f},
      {kOsc1Wave, 1}, {kOsc1PW, 0.42f}, {kMixSub, 0.4f}, {kMixOsc2, 0.3f}, {kOsc2Wave, 1}, {kOsc2Semi, 12}, {kOsc2Fine, 5},
      {kCutoff, 1500}, {kReso, 0.35f}, {kFiltEnvAmt, 0.4f}, {kFiltKeytrack, 0.7f},
      {kFEnvA, 0.005f}, {kFEnvD, 0.35f}, {kFEnvS, 0.5f}, {kFEnvR, 0.2f},
      {kAEnvA, 0.004f}, {kAEnvD, 0.3f}, {kAEnvS, 0.95f}, {kAEnvR, 0.18f},
      {kLfoWave, 0}, {kLfoRate, 6.2f}, {kLfoDelay, 0.5f}, {kLfoPitch, 0.1f},
      {kChorus, 0}}, "Leads"},

    {"Sync Lead",
     {{kVolume, 1.0f}, {kVoiceMode, 1}, {kGlide, 0.05f},
      {kOsc1Wave, 0}, {kMixOsc1, 0.25f}, {kMixOsc2, 1.0f}, {kOsc2Wave, 0}, {kOsc2Semi, 7}, {kOsc2Fine, 0},
      {kOscSync, 1}, {kOsc2Sweep, 0.45f}, {kMEnvA, 0.001f}, {kMEnvD, 0.7f}, {kMEnvS, 0.25f}, {kMEnvR, 0.3f},
      {kCutoff, 4200}, {kReso, 0.18f}, {kFiltEnvAmt, 0.2f}, {kFiltKeytrack, 0.6f},
      {kFEnvA, 0.005f}, {kFEnvD, 0.5f}, {kFEnvS, 0.6f},
      {kAEnvA, 0.004f}, {kAEnvD, 0.3f}, {kAEnvS, 0.9f}, {kAEnvR, 0.2f},
      {kLfoWave, 1}, {kLfoRate, 5.6f}, {kLfoDelay, 0.5f}, {kLfoPitch, 0.1f},
      {kChorus, 0}, {kDlyMix, 0.25f}, {kDlySync, 7}, {kDlyFeedback, 0.35f}, {kRevMix, 0.15f}}, "Leads"},

    {"Mono Saw Lead",
     {{kVolume, 0.9f}, {kVoiceMode, 1}, {kGlide, 0.07f}, {kOsc1Wave, 0}, {kMixSub, 0.45f}, {kCutoff, 1800}, {kReso, 0.35f}, {kFiltEnvAmt, 0.4f}, {kFEnvA, 0.003f}, {kFEnvD, 0.4f}, {kFEnvS, 0.5f}, {kAEnvS, 0.95f}, {kAEnvR, 0.15f}, {kLfoWave, 1}, {kLfoRate, 5.5f}, {kLfoDelay, 0.4f}, {kLfoPitch, 0.1f}, {kChorus, 0}, {kDlyMix, 0.18f}, {kDlySync, 7}}, "Leads"},

    {"Hollow Lead",
     {{kVolume, 0.8f}, {kVoiceMode, 1}, {kGlide, 0.04f}, {kOsc1Wave, 1}, {kOsc1PW, 0.2f}, {kMixOsc2, 0.5f}, {kOsc2Wave, 3}, {kOsc2Semi, 12}, {kOsc2Fine, 0}, {kCutoff, 3000}, {kReso, 0.2f}, {kFiltEnvAmt, 0.2f}, {kAEnvS, 0.9f}, {kAEnvR, 0.2f}, {kLfoWave, 1}, {kLfoRate, 5.0f}, {kLfoDelay, 0.3f}, {kLfoPitch, 0.08f}, {kChorus, 1}, {kDlyMix, 0.2f}, {kDlySync, 6}, {kRevMix, 0.15f}}, "Leads"},

    {"Theremin",
     {{kVolume, 0.8f}, {kVoiceMode, 1}, {kGlide, 0.28f}, {kOsc1Wave, 4}, {kCutoff, 6000}, {kReso, 0}, {kFiltEnvAmt, 0}, {kAEnvA, 0.08f}, {kAEnvS, 1.0f}, {kAEnvR, 0.4f}, {kLfoWave, 1}, {kLfoRate, 6.0f}, {kLfoDelay, 0.25f}, {kLfoPitch, 0.18f}, {kChorus, 0}, {kRevMix, 0.3f}, {kRevDecay, 3.0f}, {kDrift, 0.5f}}, "Leads"},

    {"Screamer",
     {{kVolume, 0.85f}, {kVoiceMode, 2}, {kUnisonVoices, 4}, {kUnisonDetune, 0.25f}, {kGlide, 0.04f}, {kMixOsc1, 0.3f}, {kMixOsc2, 1.0f}, {kOsc2Wave, 0}, {kOsc2Semi, 12}, {kOscSync, 1}, {kXMod, 0.15f}, {kOsc2Sweep, 0.3f}, {kMEnvD, 1.2f}, {kMEnvS, 0.4f}, {kCutoff, 5000}, {kReso, 0.25f}, {kFiltEnvAmt, 0.15f}, {kAEnvS, 0.95f}, {kAEnvR, 0.2f}, {kMod1Src, kSrcModWheel}, {kMod1Dst, kDstOsc2Pitch}, {kMod1Amt, 0.5f}, {kChorus, 0}, {kDlyMix, 0.22f}, {kDlySync, 7}, {kRevMix, 0.15f}}, "Leads"},

    {"Analog Bass",
     {{kVolume, 0.9f}, {kVoiceMode, 1}, {kGlide, 0.03f},
      {kOsc1Wave, 0}, {kMixSub, 0.8f}, {kMixOsc2, 0.4f}, {kOsc2Wave, 1}, {kOsc2Semi, -12}, {kOsc2Fine, 0},
      {kCutoff, 350}, {kReso, 0.3f}, {kFiltEnvAmt, 0.55f}, {kFiltKeytrack, 0.4f}, {kFiltVel, 0.4f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.3f}, {kFEnvS, 0.2f}, {kFEnvR, 0.12f},
      {kAEnvA, 0.001f}, {kAEnvD, 0.4f}, {kAEnvS, 0.9f}, {kAEnvR, 0.1f},
      {kChorus, 0}, {kDrift, 0.2f}}, "Bass"},

    {"Juno Sub Bass",
     {{kVolume, 0.9f}, {kVoiceMode, 1}, {kOsc1Wave, 1}, {kOsc1PW, 0.4f}, {kMixSub, 0.9f}, {kCutoff, 280}, {kReso, 0.15f}, {kFiltEnvAmt, 0.35f}, {kFiltKeytrack, 0.3f}, {kFEnvA, 0.001f}, {kFEnvD, 0.25f}, {kFEnvS, 0.3f}, {kAEnvA, 0.002f}, {kAEnvS, 1.0f}, {kAEnvR, 0.08f}, {kChorus, 0}, {kDrift, 0.1f}}, "Bass"},

    {"Funk Pluck Bass",
     {{kVolume, 1.0f}, {kVoiceMode, 1}, {kOsc1Wave, 0}, {kMixSub, 0.5f}, {kCutoff, 300}, {kReso, 0.55f}, {kFiltEnvAmt, 0.6f}, {kFiltVel, 0.6f}, {kFEnvA, 0.001f}, {kFEnvD, 0.15f}, {kFEnvS, 0.05f}, {kFEnvR, 0.1f}, {kAEnvA, 0.001f}, {kAEnvD, 0.35f}, {kAEnvS, 0.65f}, {kAEnvR, 0.06f}, {kChorus, 0}}, "Bass"},

    {"Reese 87",
     {{kVolume, 0.9f}, {kVoiceMode, 2}, {kUnisonVoices, 3}, {kUnisonDetune, 0.45f}, {kOsc1Wave, 0}, {kMixSub, 0.6f}, {kCutoff, 550}, {kReso, 0.2f}, {kFiltEnvAmt, 0.1f}, {kAEnvS, 1.0f}, {kAEnvR, 0.15f}, {kLfo2Wave, 1}, {kLfo2Rate, 0.3f}, {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.15f}, {kChorus, 0}}, "Bass"},

    {"Acid Line",
     {{kVolume, 1.0f}, {kVoiceMode, 1}, {kGlide, 0.06f}, {kOsc1Wave, 0}, {kCutoff, 350}, {kReso, 0.78f}, {kFiltEnvAmt, 0.6f}, {kFiltVel, 0.7f}, {kFiltKeytrack, 0.4f}, {kFEnvA, 0.001f}, {kFEnvD, 0.22f}, {kFEnvS, 0.05f}, {kAEnvA, 0.001f}, {kAEnvS, 0.9f}, {kAEnvR, 0.05f}, {kChorus, 0}, {kDlyMix, 0.15f}, {kDlySync, 6}}, "Bass"},

    {"Sync Bass",
     {{kVolume, 0.85f}, {kVoiceMode, 1}, {kMixOsc1, 0.3f}, {kMixOsc2, 0.9f}, {kOsc2Wave, 0}, {kOsc2Semi, 5}, {kOscSync, 1}, {kOsc2Sweep, 0.35f}, {kMEnvD, 0.3f}, {kMixSub, 0.6f}, {kCutoff, 900}, {kReso, 0.2f}, {kFiltEnvAmt, 0.35f}, {kFEnvD, 0.3f}, {kFEnvS, 0.3f}, {kAEnvS, 1.0f}, {kAEnvR, 0.08f}, {kChorus, 0}}, "Bass"},

    {"Arp Nightdrive",
     {{kVolume, 1.0f}, {kArpOn, 1}, {kArpMode, 2}, {kArpRate, 3}, {kArpOctaves, 2}, {kArpGate, 0.5f},
      {kOsc1Wave, 2}, {kOsc1PW, 0.3f}, {kMixOsc2, 0.35f}, {kOsc2Wave, 0}, {kOsc2Fine, 8},
      {kCutoff, 700}, {kReso, 0.3f}, {kFiltEnvAmt, 0.55f}, {kFiltKeytrack, 0.5f}, {kFiltVel, 0.3f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.22f}, {kFEnvS, 0.1f}, {kFEnvR, 0.2f},
      {kAEnvA, 0.001f}, {kAEnvD, 0.3f}, {kAEnvS, 0.5f}, {kAEnvR, 0.2f},
      {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.25f}, {kLfo2Wave, 1}, {kLfo2Sync, 3},
      {kChorus, 1}, {kDlyMix, 0.3f}, {kDlySync, 7}, {kDlyFeedback, 0.42f}, {kDlyPingPong, 1},
      {kRevMix, 0.2f}, {kRevDecay, 2.8f}}, "Arps & Plucks"},

    {"Poly Pluck",
     {{kVolume, 0.95f}, {kOsc1Wave, 0}, {kMixOsc2, 0.5f}, {kOsc2Wave, 1}, {kOsc2Fine, -6},
      {kCutoff, 300}, {kReso, 0.35f}, {kFiltEnvAmt, 0.7f}, {kFiltKeytrack, 0.6f}, {kFiltVel, 0.5f},
      {kFEnvA, 0.001f}, {kFEnvD, 0.28f}, {kFEnvS, 0.0f}, {kFEnvR, 0.3f},
      {kAEnvA, 0.001f}, {kAEnvD, 0.6f}, {kAEnvS, 0.0f}, {kAEnvR, 0.45f}, {kAmpVel, 0.6f},
      {kChorus, 3}}, "Arps & Plucks"},

    {"Arp Crystal",
     {{kVolume, 1.0f}, {kArpOn, 1}, {kArpMode, 0}, {kArpRate, 3}, {kArpOctaves, 3}, {kArpGate, 0.35f}, {kOsc1Wave, 3}, {kMixOsc2, 0.4f}, {kOsc2Wave, 3}, {kOsc2Semi, 12}, {kCutoff, 6000}, {kFiltEnvAmt, 0.1f}, {kAEnvA, 0.001f}, {kAEnvD, 0.4f}, {kAEnvS, 0.2f}, {kAEnvR, 0.3f}, {kChorus, 2}, {kDlyMix, 0.3f}, {kDlySync, 7}, {kDlyFeedback, 0.45f}, {kRevMix, 0.3f}}, "Arps & Plucks"},

    {"Arp Bassline",
     {{kVolume, 0.9f}, {kArpOn, 1}, {kArpMode, 1}, {kArpRate, 3}, {kArpOctaves, 2}, {kArpGate, 0.45f}, {kVoiceMode, 1}, {kOsc1Wave, 0}, {kMixSub, 0.5f}, {kCutoff, 400}, {kReso, 0.4f}, {kFiltEnvAmt, 0.5f}, {kFEnvA, 0.001f}, {kFEnvD, 0.18f}, {kFEnvS, 0.1f}, {kAEnvA, 0.001f}, {kAEnvS, 0.8f}, {kAEnvR, 0.05f}, {kChorus, 0}}, "Arps & Plucks"},

    {"Echo Pluck",
     {{kVolume, 1.0f}, {kOsc1Wave, 1}, {kOsc1PW, 0.3f}, {kMixOsc2, 0.4f}, {kOsc2Wave, 0}, {kCutoff, 600}, {kReso, 0.3f}, {kFiltEnvAmt, 0.6f}, {kFEnvA, 0.001f}, {kFEnvD, 0.2f}, {kFEnvS, 0}, {kAEnvA, 0.001f}, {kAEnvD, 0.45f}, {kAEnvS, 0}, {kAEnvR, 0.3f}, {kChorus, 1}, {kDlyMix, 0.4f}, {kDlySync, 7}, {kDlyFeedback, 0.55f}, {kDlyPingPong, 1}, {kRevMix, 0.2f}}, "Arps & Plucks"},

    {"Upside Arp",
     {{kVolume, 0.95f}, {kArpOn, 1}, {kArpMode, 0}, {kArpRate, 3}, {kArpOctaves, 2}, {kArpGate, 0.7f}, {kOsc1Wave, 0}, {kMixOsc2, 0.6f}, {kOsc2Fine, 10}, {kCutoff, 900}, {kReso, 0.35f}, {kFiltEnvAmt, 0.3f}, {kFEnvD, 0.3f}, {kFEnvS, 0.3f}, {kAEnvA, 0.002f}, {kAEnvS, 0.7f}, {kAEnvR, 0.15f}, {kLfo2Wave, 1}, {kLfo2Sync, 1}, {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.4f}, {kChorus, 1}, {kRevMix, 0.3f}, {kRevDecay, 3.5f}}, "Arps & Plucks"},

    {"Laser Zap",
     {{kVolume, 1.0f}, {kOsc1Wave, 4}, {kMod1Src, kSrcModEnv}, {kMod1Dst, kDstPitch}, {kMod1Amt, 1.0f}, {kMEnvA, 0.001f}, {kMEnvD, 0.25f}, {kMEnvS, 0}, {kCutoff, 9000}, {kFiltEnvAmt, 0}, {kAEnvA, 0.001f}, {kAEnvD, 0.3f}, {kAEnvS, 0}, {kAEnvR, 0.2f}, {kChorus, 0}, {kDlyMix, 0.3f}, {kDlySync, 6}, {kRevMix, 0.2f}}, "FX"},

    {"Wind Tunnel",
     {{kVolume, 1.0f}, {kMixOsc1, 0}, {kMixNoise, 1.0f}, {kCutoff, 1500}, {kReso, 0.6f}, {kFiltEnvAmt, 0}, {kFiltKeytrack, 1.0f}, {kLfo2Wave, 1}, {kLfo2Rate, 0.15f}, {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstCutoff}, {kMod1Amt, 0.5f}, {kMod2Src, kSrcRandom}, {kMod2Dst, kDstPan}, {kMod2Amt, 0.8f}, {kAEnvA, 1.5f}, {kAEnvS, 1.0f}, {kAEnvR, 2.5f}, {kChorus, 2}, {kRevMix, 0.4f}, {kRevDecay, 6.0f}}, "FX"},

    {"Sci-Fi Drone",
     {{kVolume, 0.8f}, {kOsc1Wave, 0}, {kMixOsc2, 0.8f}, {kOsc2Wave, 3}, {kOsc2Semi, 7}, {kXMod, 0.35f}, {kCutoff, 1200}, {kReso, 0.5f}, {kFiltEnvAmt, 0}, {kLfoWave, 4}, {kLfoRate, 3.0f}, {kFiltLfoAmt, 0.25f}, {kLfo2Wave, 1}, {kLfo2Rate, 0.07f}, {kMod1Src, kSrcLfo2}, {kMod1Dst, kDstXMod}, {kMod1Amt, 0.4f}, {kAEnvA, 2.0f}, {kAEnvS, 1.0f}, {kAEnvR, 3.0f}, {kChorus, 3}, {kRevMix, 0.45f}, {kRevDecay, 8.0f}, {kDlyMix, 0.2f}, {kDlySync, 10}}, "FX"},

    {"Bell Tower",
     {{kVolume, 0.8f}, {kOsc1Wave, 4}, {kOsc2Wave, 3}, {kOsc2Semi, 7}, {kOsc2Fine, 30}, {kXMod, 0.6f}, {kMixOsc2, 0.2f}, {kCutoff, 10000}, {kFiltEnvAmt, 0}, {kAEnvA, 0.001f}, {kAEnvD, 4.0f}, {kAEnvS, 0}, {kAEnvR, 3.0f}, {kRevMix, 0.5f}, {kRevDecay, 7.0f}, {kRevPreDelay, 0.06f}, {kChorus, 0}}, "FX"},
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

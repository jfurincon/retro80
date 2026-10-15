// Retro80 parameter table: the single source of truth for every parameter.
// Used by the DSP engine (plain values), the VST3 processor/controller
// (normalized 0..1 values) and the editor (layout + display text).
//
// IDs are stored in saved projects: never renumber or reuse an ID.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace retro80 {

enum ParamId : uint32_t {
    // Phase 1 kept ID 0 for volume.
    kVolume = 0,

    kOsc1Wave = 1,
    kOsc1PW,
    kOsc1PWM,
    kOsc2Wave,
    kOsc2Semi,
    kOsc2Fine,

    kMixOsc1,
    kMixOsc2,
    kMixSub,
    kMixNoise,

    kHpf,
    kCutoff,
    kReso,
    kFiltEnvAmt,
    kFiltLfoAmt,
    kFiltKeytrack,
    kFiltVel,

    kFEnvA,
    kFEnvD,
    kFEnvS,
    kFEnvR,

    kAEnvA,
    kAEnvD,
    kAEnvS,
    kAEnvR,
    kAmpVel,

    kLfoWave,
    kLfoRate,
    kLfoDelay,
    kLfoPitch,

    kVoiceMode,
    kUnisonVoices,
    kUnisonDetune,
    kGlide,
    kBendRange,
    kDrift,

    kChorus,

    // ---- Phase 3 (v0.3.0) ----
    kOscSync,
    kXMod,
    kOsc2Sweep,
    kLfoSync,

    kLfo2Wave,
    kLfo2Rate,
    kLfo2Sync,

    kMEnvA,
    kMEnvD,
    kMEnvS,
    kMEnvR,

    kMod1Src, kMod1Dst, kMod1Amt,
    kMod2Src, kMod2Dst, kMod2Amt,
    kMod3Src, kMod3Dst, kMod3Amt,
    kMod4Src, kMod4Dst, kMod4Amt,

    kArpOn,
    kArpMode,
    kArpRate,
    kArpOctaves,
    kArpGate,
    kArpLatch,

    kDlyMix,
    kDlyTime,
    kDlySync,
    kDlyFeedback,
    kDlyTone,
    kDlyPingPong,

    kRevMix,
    kRevDecay,
    kRevDamp,
    kRevPreDelay,

    kNumParams,

    // Hidden, MIDI-mapped (not part of patches).
    kPitchBend = 100,
    kModWheel = 101,
    // Factory program selector.
    kProgram = 102,
    kSustainPedal = 103,
    kAftertouch = 104,
};

constexpr int kNumModSlots = 4;

enum class Scale : uint8_t {
    Linear,  // min..max
    Log,     // min * (max/min)^n   (min > 0)
    Time,    // seconds, log-ish: min..max
    Int,     // integer steps min..max
    List,    // choice index 0..count-1
};

enum class DisplayUnit : uint8_t { None, Percent, Bipolar, Hz, Seconds, Semitones, Cents, Decibel, Volume, Count };

struct ParamDef {
    ParamId id;
    const char* name;
    const char* shortName;
    Scale scale;
    DisplayUnit unit;
    float min, max, def;  // plain values
    const char* const* labels = nullptr;
    int labelCount = 0;
};

// ---- choice labels ---------------------------------------------------------
inline const char* const kOsc1Waves[] = {"Saw", "Pulse", "Saw+Pulse", "Triangle"};
inline const char* const kOsc2Waves[] = {"Saw", "Pulse", "Triangle", "Sine"};
inline const char* const kLfoWaves[] = {"Triangle", "Sine", "Square", "Saw Down", "Random"};
inline const char* const kVoiceModes[] = {"Poly", "Mono", "Unison"};
inline const char* const kChorusModes[] = {"Off", "I", "II", "I+II"};
inline const char* const kOffOn[] = {"Off", "On"};

// Tempo divisions (length in quarter-note beats alongside).
inline const char* const kLfoSyncDivs[] = {"Off", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8",
                                          "1/16", "1/4 T", "1/8 T", "1/16 T", "1/4 .", "1/8 ."};
constexpr double kLfoSyncBeats[] = {0, 16, 8, 4, 2, 1, 0.5, 0.25, 2.0 / 3, 1.0 / 3, 1.0 / 6, 1.5, 0.75};

inline const char* const kDelaySyncDivs[] = {"Off", "1/32", "1/16 T", "1/16", "1/16 .", "1/8 T",
                                            "1/8", "1/8 .", "1/4 T", "1/4", "1/4 .", "1/2"};
constexpr double kDelaySyncBeats[] = {0, 0.125, 1.0 / 6, 0.25, 0.375, 1.0 / 3, 0.5, 0.75, 2.0 / 3, 1, 1.5, 2};

inline const char* const kArpRates[] = {"1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32"};
constexpr double kArpRateBeats[] = {1, 0.5, 1.0 / 3, 0.25, 1.0 / 6, 0.125};

inline const char* const kArpModes[] = {"Up", "Down", "Up/Down", "Random", "As Played"};

inline const char* const kModSources[] = {"Off", "LFO 1", "LFO 2", "Mod Env", "Velocity",
                                         "Mod Wheel", "Aftertouch", "Key", "Random"};
enum ModSource { kSrcOff, kSrcLfo1, kSrcLfo2, kSrcModEnv, kSrcVelocity, kSrcModWheel, kSrcAftertouch, kSrcKey, kSrcRandom };

inline const char* const kModDests[] = {"Off", "Pitch", "Osc 2 Pitch", "Pulse Width", "Cutoff", "Resonance", "Amp",
                                       "Pan", "Cross Mod", "Osc 1 Level", "Osc 2 Level", "Sub Level", "Noise Level"};
enum ModDest { kDstOff, kDstPitch, kDstOsc2Pitch, kDstPW, kDstCutoff, kDstReso, kDstAmp,
               kDstPan, kDstXMod, kDstOsc1Lvl, kDstOsc2Lvl, kDstSubLvl, kDstNoiseLvl, kNumModDests };

template <size_t N>
constexpr int countOf(const char* const (&)[N]) { return static_cast<int>(N); }

// ---- the table (order = kParamId order, checked at startup) -----------------
inline const ParamDef kParamDefs[] = {
    {kVolume, "Master Volume", "Master", Scale::Linear, DisplayUnit::Volume, 0.0f, 1.0f, 0.70710678f},

    {kOsc1Wave, "Osc 1 Wave", "Wave", Scale::List, DisplayUnit::None, 0, 3, 0, kOsc1Waves, countOf(kOsc1Waves)},
    {kOsc1PW, "Osc 1 Pulse Width", "PW", Scale::Linear, DisplayUnit::Percent, 0.05f, 0.5f, 0.5f},
    {kOsc1PWM, "Osc 1 PWM (LFO)", "PWM", Scale::Linear, DisplayUnit::Percent, 0.0f, 1.0f, 0.0f},
    {kOsc2Wave, "Osc 2 Wave", "Wave", Scale::List, DisplayUnit::None, 0, 3, 0, kOsc2Waves, countOf(kOsc2Waves)},
    {kOsc2Semi, "Osc 2 Semitones", "Semi", Scale::Int, DisplayUnit::Semitones, -24, 24, 0},
    {kOsc2Fine, "Osc 2 Fine", "Fine", Scale::Linear, DisplayUnit::Cents, -50, 50, 7},

    {kMixOsc1, "Osc 1 Level", "Osc 1", Scale::Linear, DisplayUnit::Percent, 0, 1, 1.0f},
    {kMixOsc2, "Osc 2 Level", "Osc 2", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.0f},
    {kMixSub, "Sub Level", "Sub", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.0f},
    {kMixNoise, "Noise Level", "Noise", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.0f},

    {kHpf, "High-Pass", "HPF", Scale::Log, DisplayUnit::Hz, 10, 2000, 10},
    {kCutoff, "Cutoff", "Cutoff", Scale::Log, DisplayUnit::Hz, 30, 18000, 6000},
    {kReso, "Resonance", "Reso", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.1f},
    {kFiltEnvAmt, "Filter Env Amount", "Env", Scale::Linear, DisplayUnit::Bipolar, -1, 1, 0.3f},
    {kFiltLfoAmt, "Filter LFO Amount", "LFO", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.0f},
    {kFiltKeytrack, "Filter Keytrack", "Key", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.5f},
    {kFiltVel, "Filter Velocity", "Vel", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.3f},

    {kFEnvA, "Filter Attack", "A", Scale::Time, DisplayUnit::Seconds, 0.001f, 10, 0.003f},
    {kFEnvD, "Filter Decay", "D", Scale::Time, DisplayUnit::Seconds, 0.005f, 10, 0.4f},
    {kFEnvS, "Filter Sustain", "S", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.4f},
    {kFEnvR, "Filter Release", "R", Scale::Time, DisplayUnit::Seconds, 0.005f, 10, 0.3f},

    {kAEnvA, "Amp Attack", "A", Scale::Time, DisplayUnit::Seconds, 0.001f, 10, 0.003f},
    {kAEnvD, "Amp Decay", "D", Scale::Time, DisplayUnit::Seconds, 0.005f, 10, 0.5f},
    {kAEnvS, "Amp Sustain", "S", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.8f},
    {kAEnvR, "Amp Release", "R", Scale::Time, DisplayUnit::Seconds, 0.005f, 10, 0.3f},
    {kAmpVel, "Amp Velocity", "Vel", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.5f},

    {kLfoWave, "LFO Wave", "Wave", Scale::List, DisplayUnit::None, 0, 4, 0, kLfoWaves, countOf(kLfoWaves)},
    {kLfoRate, "LFO Rate", "Rate", Scale::Log, DisplayUnit::Hz, 0.05f, 20, 5.0f},
    {kLfoDelay, "LFO Delay", "Delay", Scale::Time, DisplayUnit::Seconds, 0.0f, 5, 0.0f},
    {kLfoPitch, "LFO to Pitch", "Vibrato", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.0f},

    {kVoiceMode, "Voice Mode", "Mode", Scale::List, DisplayUnit::None, 0, 2, 0, kVoiceModes, countOf(kVoiceModes)},
    {kUnisonVoices, "Unison Voices", "Voices", Scale::Int, DisplayUnit::Count, 2, 8, 4},
    {kUnisonDetune, "Unison Detune", "Detune", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.3f},
    {kGlide, "Glide", "Glide", Scale::Time, DisplayUnit::Seconds, 0.0f, 2, 0.0f},
    {kBendRange, "Bend Range", "Bend", Scale::Int, DisplayUnit::Semitones, 1, 12, 2},
    {kDrift, "Analog Drift", "Drift", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.3f},

    {kChorus, "Chorus", "Chorus", Scale::List, DisplayUnit::None, 0, 3, 1, kChorusModes, countOf(kChorusModes)},

    {kOscSync, "Osc 2 Sync", "Sync", Scale::List, DisplayUnit::None, 0, 1, 0, kOffOn, countOf(kOffOn)},
    {kXMod, "Cross Mod (Osc 2 > Osc 1)", "X-Mod", Scale::Linear, DisplayUnit::Percent, 0, 1, 0},
    {kOsc2Sweep, "Mod Env > Osc 2 Pitch", "Sweep", Scale::Linear, DisplayUnit::Bipolar, -1, 1, 0},
    {kLfoSync, "LFO 1 Tempo Sync", "Sync", Scale::List, DisplayUnit::None, 0, 12, 0, kLfoSyncDivs, countOf(kLfoSyncDivs)},

    {kLfo2Wave, "LFO 2 Wave", "Wave", Scale::List, DisplayUnit::None, 0, 4, 1, kLfoWaves, countOf(kLfoWaves)},
    {kLfo2Rate, "LFO 2 Rate", "Rate", Scale::Log, DisplayUnit::Hz, 0.05f, 20, 0.5f},
    {kLfo2Sync, "LFO 2 Tempo Sync", "Sync", Scale::List, DisplayUnit::None, 0, 12, 0, kLfoSyncDivs, countOf(kLfoSyncDivs)},

    {kMEnvA, "Mod Env Attack", "A", Scale::Time, DisplayUnit::Seconds, 0.001f, 10, 0.001f},
    {kMEnvD, "Mod Env Decay", "D", Scale::Time, DisplayUnit::Seconds, 0.005f, 10, 0.3f},
    {kMEnvS, "Mod Env Sustain", "S", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.0f},
    {kMEnvR, "Mod Env Release", "R", Scale::Time, DisplayUnit::Seconds, 0.005f, 10, 0.3f},

    {kMod1Src, "Mod 1 Source", "Source", Scale::List, DisplayUnit::None, 0, 8, 0, kModSources, countOf(kModSources)},
    {kMod1Dst, "Mod 1 Destination", "Dest", Scale::List, DisplayUnit::None, 0, 12, 0, kModDests, countOf(kModDests)},
    {kMod1Amt, "Mod 1 Amount", "Amount", Scale::Linear, DisplayUnit::Bipolar, -1, 1, 0},
    {kMod2Src, "Mod 2 Source", "Source", Scale::List, DisplayUnit::None, 0, 8, 0, kModSources, countOf(kModSources)},
    {kMod2Dst, "Mod 2 Destination", "Dest", Scale::List, DisplayUnit::None, 0, 12, 0, kModDests, countOf(kModDests)},
    {kMod2Amt, "Mod 2 Amount", "Amount", Scale::Linear, DisplayUnit::Bipolar, -1, 1, 0},
    {kMod3Src, "Mod 3 Source", "Source", Scale::List, DisplayUnit::None, 0, 8, 0, kModSources, countOf(kModSources)},
    {kMod3Dst, "Mod 3 Destination", "Dest", Scale::List, DisplayUnit::None, 0, 12, 0, kModDests, countOf(kModDests)},
    {kMod3Amt, "Mod 3 Amount", "Amount", Scale::Linear, DisplayUnit::Bipolar, -1, 1, 0},
    {kMod4Src, "Mod 4 Source", "Source", Scale::List, DisplayUnit::None, 0, 8, 0, kModSources, countOf(kModSources)},
    {kMod4Dst, "Mod 4 Destination", "Dest", Scale::List, DisplayUnit::None, 0, 12, 0, kModDests, countOf(kModDests)},
    {kMod4Amt, "Mod 4 Amount", "Amount", Scale::Linear, DisplayUnit::Bipolar, -1, 1, 0},

    {kArpOn, "Arpeggiator", "Arp", Scale::List, DisplayUnit::None, 0, 1, 0, kOffOn, countOf(kOffOn)},
    {kArpMode, "Arp Mode", "Mode", Scale::List, DisplayUnit::None, 0, 4, 0, kArpModes, countOf(kArpModes)},
    {kArpRate, "Arp Rate", "Rate", Scale::List, DisplayUnit::None, 0, 5, 3, kArpRates, countOf(kArpRates)},
    {kArpOctaves, "Arp Octaves", "Octaves", Scale::Int, DisplayUnit::Count, 1, 4, 1},
    {kArpGate, "Arp Gate", "Gate", Scale::Linear, DisplayUnit::Percent, 0.05f, 1, 0.6f},
    {kArpLatch, "Arp Latch", "Latch", Scale::List, DisplayUnit::None, 0, 1, 0, kOffOn, countOf(kOffOn)},

    {kDlyMix, "Delay Mix", "Mix", Scale::Linear, DisplayUnit::Percent, 0, 1, 0},
    {kDlyTime, "Delay Time", "Time", Scale::Time, DisplayUnit::Seconds, 0.01f, 2, 0.375f},
    {kDlySync, "Delay Tempo Sync", "Sync", Scale::List, DisplayUnit::None, 0, 11, 7, kDelaySyncDivs, countOf(kDelaySyncDivs)},
    {kDlyFeedback, "Delay Feedback", "Feedback", Scale::Linear, DisplayUnit::Percent, 0, 0.95f, 0.35f},
    {kDlyTone, "Delay Tone", "Tone", Scale::Log, DisplayUnit::Hz, 500, 16000, 4500},
    {kDlyPingPong, "Delay Ping-Pong", "Ping-Pong", Scale::List, DisplayUnit::None, 0, 1, 1, kOffOn, countOf(kOffOn)},

    {kRevMix, "Reverb Mix", "Mix", Scale::Linear, DisplayUnit::Percent, 0, 1, 0},
    {kRevDecay, "Reverb Decay", "Decay", Scale::Log, DisplayUnit::Seconds, 0.3f, 12, 2.5f},
    {kRevDamp, "Reverb Damping", "Damp", Scale::Linear, DisplayUnit::Percent, 0, 1, 0.4f},
    {kRevPreDelay, "Reverb Pre-Delay", "Pre-Dly", Scale::Linear, DisplayUnit::Seconds, 0, 0.2f, 0.02f},
};

constexpr int kNumPatchParams = static_cast<int>(kNumParams);
static_assert(sizeof(kParamDefs) / sizeof(kParamDefs[0]) == kNumParams, "param table out of sync");

inline const ParamDef& paramDef(uint32_t id) { return kParamDefs[id]; }

// Time scale: skewed so the knob center is ~0.3 s for a 1 ms..10 s range.
constexpr float kTimeSkew = 3.0f;

inline double toPlain(const ParamDef& d, double n) {
    n = std::clamp(n, 0.0, 1.0);
    switch (d.scale) {
        case Scale::Linear: return d.min + n * (d.max - d.min);
        case Scale::Log: return d.min * std::pow(double(d.max) / d.min, n);
        case Scale::Time: return d.min + std::pow(n, double(kTimeSkew)) * (d.max - d.min);
        case Scale::Int: return std::round(d.min + n * (d.max - d.min));
        case Scale::List: return std::round(n * (d.labelCount - 1));
    }
    return d.def;
}

inline double toNormalized(const ParamDef& d, double plain) {
    double n = 0;
    switch (d.scale) {
        case Scale::Linear:
        case Scale::Int: n = (plain - d.min) / (double(d.max) - d.min); break;
        case Scale::Log: n = std::log(plain / d.min) / std::log(double(d.max) / d.min); break;
        case Scale::Time: n = std::pow(std::max(0.0, (plain - d.min) / (double(d.max) - d.min)), 1.0 / kTimeSkew); break;
        case Scale::List: n = d.labelCount > 1 ? plain / (d.labelCount - 1) : 0; break;
    }
    return std::clamp(n, 0.0, 1.0);
}

inline int stepCount(const ParamDef& d) {
    if (d.scale == Scale::List) return d.labelCount - 1;
    if (d.scale == Scale::Int) return static_cast<int>(d.max - d.min);
    return 0;
}

inline double defaultNormalized(const ParamDef& d) { return toNormalized(d, d.def); }

// Display text for a plain value.
inline void formatValue(const ParamDef& d, double v, char* out, size_t size) {
    if (d.scale == Scale::List) {
        const int i = std::clamp(static_cast<int>(v), 0, d.labelCount - 1);
        std::snprintf(out, size, "%s", d.labels[i]);
        return;
    }
    switch (d.unit) {
        case DisplayUnit::Percent: std::snprintf(out, size, "%.0f%%", v * 100); break;
        case DisplayUnit::Bipolar: std::snprintf(out, size, "%+.0f%%", v * 100); break;
        case DisplayUnit::Hz:
            if (v >= 1000) std::snprintf(out, size, "%.2f kHz", v / 1000);
            else if (v >= 10) std::snprintf(out, size, "%.0f Hz", v);
            else std::snprintf(out, size, "%.2f Hz", v);
            break;
        case DisplayUnit::Seconds:
            if (v < 0.0005) std::snprintf(out, size, "0 ms");
            else if (v < 1) std::snprintf(out, size, "%.0f ms", v * 1000);
            else std::snprintf(out, size, "%.2f s", v);
            break;
        case DisplayUnit::Semitones: std::snprintf(out, size, "%+.0f st", v); break;
        case DisplayUnit::Cents: std::snprintf(out, size, "%+.0f ct", v); break;
        case DisplayUnit::Volume: {
            const double g = 2.0 * v * v;
            if (g < 1e-5) std::snprintf(out, size, "-inf dB");
            else { const double db = 20 * std::log10(g); std::snprintf(out, size, "%.1f dB", std::fabs(db) < 0.05 ? 0.0 : db); }
            break;
        }
        case DisplayUnit::Count: std::snprintf(out, size, "%.0f", v); break;
        default: std::snprintf(out, size, "%.2f", v); break;
    }
}

inline bool strcasecmp_portable(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
        if (x != y) return false;
    }
    return *a == *b;
}

// Parse user-typed text back to a plain value (for hosts that let you type).
inline bool parseValue(const ParamDef& d, const char* text, double& plain) {
    if (d.scale == Scale::List) {
        for (int i = 0; i < d.labelCount; ++i)
            if (strcasecmp_portable(text, d.labels[i])) { plain = i; return true; }
        return false;
    }
    double x = 0;
    if (std::sscanf(text, "%lf", &x) != 1) return false;
    switch (d.unit) {
        case DisplayUnit::Percent: case DisplayUnit::Bipolar: x /= 100.0; break;
        case DisplayUnit::Hz: if (std::strstr(text, "k")) x *= 1000.0; break;
        case DisplayUnit::Seconds: if (std::strstr(text, "ms")) x /= 1000.0; break;
        case DisplayUnit::Volume: x = std::sqrt(std::pow(10.0, x / 20.0) / 2.0); break;
        default: break;
    }
    plain = std::clamp(x, double(std::min(d.min, d.max)), double(std::max(d.min, d.max)));
    return true;
}

} // namespace retro80

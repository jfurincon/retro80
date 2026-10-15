// Retro80 DSP engine: plain C++, no plugin-SDK dependencies, so it can be
// tested offline on any machine.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace retro80 {

// PolyBLEP residual: smooths the discontinuity of a naive saw so it
// doesn't alias (t = phase in [0,1), dt = phase increment).
inline float polyBlep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

struct Voice {
    enum class Stage { Off, Attack, Sustain, Release };

    Stage stage = Stage::Off;
    int note = -1;
    float phase = 0.0f;
    float inc = 0.0f;
    float env = 0.0f;
    float velocity = 0.0f;
    uint64_t age = 0;

    bool active() const { return stage != Stage::Off; }
};

class Engine {
public:
    static constexpr int kMaxVoices = 8;

    void setSampleRate(double sr) {
        sampleRate_ = static_cast<float>(sr);
        // Short linear attack (avoids clicks), exponential release.
        attackStep_ = 1.0f / (0.004f * sampleRate_);           // 4 ms
        releaseCoef_ = std::exp(-1.0f / (0.25f * sampleRate_)); // ~250 ms tail
        gainSmooth_ = 1.0f - std::exp(-1.0f / (0.02f * sampleRate_)); // 20 ms
        reset();
    }

    void reset() {
        for (auto& v : voices_) v = Voice{};
        gainCurrent_ = gainTarget_;
    }

    // Linear gain target; smoothed internally so knob moves don't zipper.
    void setGain(float g) { gainTarget_ = g; }

    void noteOn(int note, float velocity) {
        Voice& v = pickVoice(note);
        v.note = note;
        v.velocity = velocity;
        v.inc = midiToHz(note) / sampleRate_;
        if (!v.active()) {
            v.phase = 0.0f;
            v.env = 0.0f;
        }
        v.stage = Voice::Stage::Attack;
        v.age = ++ageCounter_;
    }

    void noteOff(int note) {
        for (auto& v : voices_)
            if (v.note == note &&
                (v.stage == Voice::Stage::Attack || v.stage == Voice::Stage::Sustain))
                v.stage = Voice::Stage::Release;
    }

    void allNotesOff() {
        for (auto& v : voices_)
            if (v.active()) v.stage = Voice::Stage::Release;
    }

    bool anyVoiceActive() const {
        for (auto& v : voices_)
            if (v.active()) return true;
        return false;
    }

    // Adds nothing: overwrites outL/outR with numSamples of output.
    void render(float* outL, float* outR, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float sum = 0.0f;
            for (auto& v : voices_) {
                if (!v.active()) continue;
                sum += tickVoice(v);
            }
            gainCurrent_ += (gainTarget_ - gainCurrent_) * gainSmooth_;
            const float s = sum * kVoiceLevel * gainCurrent_;
            outL[i] = s;
            outR[i] = s;
        }
    }

private:
    static constexpr float kVoiceLevel = 0.18f; // headroom for 8 voices

    static float midiToHz(int note) {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    float tickVoice(Voice& v) {
        switch (v.stage) {
            case Voice::Stage::Attack:
                v.env += attackStep_;
                if (v.env >= 1.0f) {
                    v.env = 1.0f;
                    v.stage = Voice::Stage::Sustain;
                }
                break;
            case Voice::Stage::Release:
                v.env *= releaseCoef_;
                if (v.env < 1.0e-4f) {
                    v = Voice{};
                    return 0.0f;
                }
                break;
            default:
                break;
        }

        // Band-limited sawtooth.
        float saw = 2.0f * v.phase - 1.0f;
        saw -= polyBlep(v.phase, v.inc);
        v.phase += v.inc;
        if (v.phase >= 1.0f) v.phase -= 1.0f;

        return saw * v.env * v.velocity;
    }

    // Same note retriggers its own voice; otherwise a free voice; otherwise
    // the quietest releasing voice; otherwise the oldest voice.
    Voice& pickVoice(int note) {
        for (auto& v : voices_)
            if (v.active() && v.note == note) return v;
        for (auto& v : voices_)
            if (!v.active()) return v;
        Voice* best = nullptr;
        for (auto& v : voices_)
            if (v.stage == Voice::Stage::Release && (!best || v.env < best->env)) best = &v;
        if (best) return *best;
        best = &voices_[0];
        for (auto& v : voices_)
            if (v.age < best->age) best = &v;
        return *best;
    }

    std::array<Voice, kMaxVoices> voices_{};
    float sampleRate_ = 44100.0f;
    float attackStep_ = 0.0f;
    float releaseCoef_ = 0.0f;
    float gainSmooth_ = 0.0f;
    float gainTarget_ = 1.0f;
    float gainCurrent_ = 1.0f;
    uint64_t ageCounter_ = 0;
};

} // namespace retro80

// Small DSP building blocks used by the Retro80 engine.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace retro80 {

constexpr float kPi = 3.14159265358979f;

// ---------------------------------------------------------------- utilities
inline float fastTanh(float x) {
    // Rational approximation, exact limits at +-3.
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// tan(x) for 0 <= x < ~1.2 (filter prewarping).
inline float fastTan(float x) {
    const float x2 = x * x;
    return x * (15.0f - x2) / (15.0f - 6.0f * x2);
}

struct Rng {
    uint32_t s = 0x9E3779B9u;
    Rng(uint32_t seed = 0x9E3779B9u) : s(seed ? seed : 1u) {}
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float bipolar() { return static_cast<float>(next()) * (2.0f / 4294967295.0f) - 1.0f; }
    float unipolar() { return static_cast<float>(next()) * (1.0f / 4294967295.0f); }
};

// One-pole smoother toward a target (used for every zipper-prone control).
struct Smoother {
    float value = 0.0f, target = 0.0f, coef = 1.0f;
    void setTime(float seconds, float sampleRate) {
        coef = seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * sampleRate));
    }
    void snap(float v) { value = target = v; }
    float tick() { value += (target - value) * coef; return value; }
};

// PolyBLEP residual for a unit step at phase 0 (t in [0,1), dt = increment).
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

// ---------------------------------------------------------------- envelope
// Analog-style ADSR: exponential segments with a curved (RC-like) attack.
struct EnvCoefs {
    float attackCoef = 0, attackBase = 0;
    float decayCoef = 0, decayBase = 0;
    float releaseCoef = 0, releaseBase = 0;
    float sustain = 1;

    static float coefFor(float seconds, float ratio, float sampleRate) {
        const float samples = std::max(1.0f, seconds * sampleRate);
        return std::exp(-std::log((1.0f + ratio) / ratio) / samples);
    }

    void set(float a, float d, float s, float r, float sampleRate) {
        constexpr float kAttackRatio = 0.3f;      // gentle curve, like a charging capacitor
        constexpr float kDecayRatio = 0.0001f;    // near-exponential tails
        sustain = s;
        attackCoef = coefFor(a, kAttackRatio, sampleRate);
        attackBase = (1.0f + kAttackRatio) * (1.0f - attackCoef);
        decayCoef = coefFor(d, kDecayRatio, sampleRate);
        decayBase = (sustain - kDecayRatio) * (1.0f - decayCoef);
        releaseCoef = coefFor(r, kDecayRatio, sampleRate);
        releaseBase = -kDecayRatio * (1.0f - releaseCoef);
    }
};

struct Envelope {
    enum class Stage : uint8_t { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    float value = 0.0f;

    void gateOn() { stage = Stage::Attack; }  // restarts from the current value (no click)
    void gateOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void kill() { stage = Stage::Idle; value = 0.0f; }
    bool idle() const { return stage == Stage::Idle; }

    float tick(const EnvCoefs& c) {
        switch (stage) {
            case Stage::Idle: break;
            case Stage::Attack:
                value = c.attackBase + value * c.attackCoef;
                if (value >= 1.0f) { value = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                value = c.decayBase + value * c.decayCoef;
                if (value <= c.sustain) { value = c.sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                value += (c.sustain - value) * 0.002f;  // follow sustain knob smoothly
                break;
            case Stage::Release:
                value = c.releaseBase + value * c.releaseCoef;
                if (value <= 0.0f) { value = 0.0f; stage = Stage::Idle; }
                break;
        }
        return value;
    }
};

// ---------------------------------------------------------------- filters
// 4-pole (24 dB/oct) zero-delay-feedback ladder low-pass with a saturating
// feedback path, after Zavalishin's TPT structure. Self-oscillates at k ~ 4.
struct LadderFilter {
    float s[4] = {0, 0, 0, 0};

    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }

    // g = tan(pi * fc / fs), k = resonance feedback (0..~4.2)
    float process(float x, float g, float k) {
        const float G = g / (1.0f + g);
        const float oneMinusG = 1.0f - G;
        // Linear prediction of the 4th stage output.
        const float S1 = s[0] * oneMinusG, S2 = s[1] * oneMinusG;
        const float S3 = s[2] * oneMinusG, S4 = s[3] * oneMinusG;
        const float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
        const float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;
        const float y4est = (G4 * x + sigma) / (1.0f + k * G4);

        // Saturating input/feedback stage (where the analog "warmth" lives),
        // with partial bass-loss compensation as resonance rises.
        const float u = fastTanh(x * (1.0f + 0.35f * k) - k * y4est);

        float in = u;
        for (int i = 0; i < 4; ++i) {
            const float v = (in - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            in = y;
        }
        return in;
    }
};

// One-pole TPT high-pass.
struct OnePoleHP {
    float s = 0.0f;
    void reset() { s = 0.0f; }
    float process(float x, float G) {  // G = g / (1 + g)
        const float v = (x - s) * G;
        const float lp = v + s;
        s = lp + v;
        return x - lp;
    }
};

// One-pole TPT low-pass.
struct OnePoleLP {
    float s = 0.0f;
    void reset() { s = 0.0f; }
    float process(float x, float G) {
        const float v = (x - s) * G;
        const float lp = v + s;
        s = lp + v;
        return lp;
    }
};

// ---------------------------------------------------------------- 2x decimator
// Linear-phase half-band FIR (Kaiser window, ~90 dB stop band).
class HalfbandDecimator {
public:
    static constexpr int kTaps = 71;
    static constexpr int kCenter = kTaps / 2;
    static constexpr int kLatency = kCenter / 2;  // in output samples (rounded down)

    HalfbandDecimator() {
        // Kaiser-windowed sinc, cutoff = fs/4.
        const double beta = 8.96;
        auto bessel0 = [](double x) {
            double sum = 1, term = 1;
            for (int k = 1; k < 30; ++k) {
                term *= (x / (2 * k)) * (x / (2 * k));
                sum += term;
            }
            return sum;
        };
        double total = 0;
        double h[kTaps];
        for (int n = 0; n < kTaps; ++n) {
            const int m = n - kCenter;
            const double sinc = m == 0 ? 0.5 : std::sin(0.5 * 3.141592653589793 * m) / (3.141592653589793 * m);
            const double r = double(m) / kCenter;
            const double w = bessel0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / bessel0(beta);
            h[n] = (m != 0 && (m % 2 == 0)) ? 0.0 : sinc * w;
            total += h[n];
        }
        numOdd_ = 0;
        for (int n = 0; n < kTaps; ++n) {
            const int m = n - kCenter;
            if (m == 0) center_ = static_cast<float>(h[n] / total);
            else if (m % 2 != 0) { oddTap_[numOdd_] = static_cast<float>(h[n] / total); oddIdx_[numOdd_] = n; ++numOdd_; }
        }
        reset();
    }

    void reset() {
        std::fill(std::begin(buf_), std::end(buf_), 0.0f);
        pos_ = 0;
    }

    // Takes two oversampled inputs, returns one output sample.
    float process(float a, float b) {
        push(a);
        push(b);
        // Newest sample is at pos_-1; tap n refers to sample delayed by (kTaps-1-n).
        const float* x = &buf_[pos_];  // x[0] = oldest of the last kTaps samples
        float acc = center_ * x[kCenter];
        for (int i = 0; i < numOdd_; ++i) acc += oddTap_[i] * x[oddIdx_[i]];
        return acc;
    }

private:
    void push(float v) {
        // Mirrored ring buffer so the last kTaps samples are always contiguous.
        buf_[pos_] = v;
        buf_[pos_ + kTaps] = v;
        if (++pos_ >= kTaps) pos_ = 0;
    }

    float buf_[2 * kTaps];
    int pos_ = 0;
    float center_ = 0.5f;
    float oddTap_[kTaps];
    int oddIdx_[kTaps];
    int numOdd_ = 0;
};

// ---------------------------------------------------------------- chorus
// Juno-60 style BBD chorus: one delay line per side, modulated by a triangle
// LFO in opposite phase, dark (BBD-like) wet path, dry + wet summed.
class JunoChorus {
public:
    void prepare(float sampleRate) {
        sr_ = sampleRate;
        const int size = static_cast<int>(sampleRate * 0.02f) + 8;  // 20 ms
        bufL_.assign(size, 0.0f);
        bufR_.assign(size, 0.0f);
        pos_ = 0;
        wet_.setTime(0.03f, sampleRate);
        center_.setTime(0.05f, sampleRate);
        depth_.setTime(0.05f, sampleRate);
        rate_.setTime(0.05f, sampleRate);
        const float g = fastTan(kPi * 7500.0f / sampleRate);
        lpG_ = g / (1.0f + g);
        setMode(mode_, true);
    }

    void reset() {
        std::fill(bufL_.begin(), bufL_.end(), 0.0f);
        std::fill(bufR_.begin(), bufR_.end(), 0.0f);
        for (auto* f : {&lp1L_, &lp2L_, &lp1R_, &lp2R_}) f->reset();
    }

    void setMode(int mode, bool snap = false) {
        mode_ = mode;
        float rate = 0.513f, lo = 1.66f, hi = 5.35f;  // mode I
        if (mode == 2) rate = 0.863f;                  // mode II
        if (mode == 3) { rate = 9.75f; lo = 3.3f; hi = 3.7f; }  // I+II
        const float on = mode == 0 ? 0.0f : 1.0f;
        const float c = 0.5f * (lo + hi) * 0.001f * sr_;
        const float d = 0.5f * (hi - lo) * 0.001f * sr_;
        wet_.target = on;
        if (mode != 0) {  // keep the last timing while fading out
            center_.target = c; depth_.target = d; rate_.target = rate;
        }
        if (snap) { wet_.snap(on); center_.snap(c); depth_.snap(d); rate_.snap(rate); }
    }

    void process(float& l, float& r) {
        const float wet = wet_.tick();
        const float center = center_.tick(), depth = depth_.tick(), rate = rate_.tick();
        const int size = static_cast<int>(bufL_.size());
        bufL_[pos_] = l;
        bufR_[pos_] = r;

        if (wet > 1.0e-4f) {
            phase_ += rate / sr_;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            const float tri = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;  // -1..1
            const float wl = lp2L_.process(lp1L_.process(read(bufL_, center + depth * tri, size), lpG_), lpG_);
            const float wr = lp2R_.process(lp1R_.process(read(bufR_, center - depth * tri, size), lpG_), lpG_);
            const float dryGain = 1.0f - 0.25f * wet;  // keep level roughly even
            l = l * dryGain + wl * wet * 0.85f;
            r = r * dryGain + wr * wet * 0.85f;
        }
        if (++pos_ >= size) pos_ = 0;
    }

private:
    float read(const std::vector<float>& buf, float delay, int size) const {
        float rp = static_cast<float>(pos_) - delay;
        while (rp < 0) rp += size;
        const int i1 = static_cast<int>(rp);
        const float f = rp - i1;
        const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
        const float y0 = buf[i0], y1 = buf[i1 % size], y2 = buf[i2], y3 = buf[i3];
        // 4-point Hermite interpolation.
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    float sr_ = 48000.0f;
    std::vector<float> bufL_, bufR_;
    int pos_ = 0;
    int mode_ = 1;
    float phase_ = 0.0f;
    float lpG_ = 0.3f;
    Smoother wet_, center_, depth_, rate_;
    OnePoleLP lp1L_, lp2L_, lp1R_, lp2R_;
};

} // namespace retro80

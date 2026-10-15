// Retro80 effects: stereo delay and FDN reverb (plain C++).
#pragma once

#include "building_blocks.h"

#include <array>
#include <cmath>
#include <vector>

namespace retro80 {

// 4-point Hermite read from a circular buffer, `delay` samples behind `writePos`.
inline float hermiteRead(const std::vector<float>& buf, int writePos, float delay) {
    const int size = static_cast<int>(buf.size());
    float rp = static_cast<float>(writePos) - delay;
    while (rp < 0) rp += size;
    int i1 = static_cast<int>(rp);
    const float f = rp - i1;
    i1 %= size;
    const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
    const float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

// ---------------------------------------------------------------- delay
// Stereo / ping-pong delay with a darkening, gently saturating feedback path.
// Time changes glide like a tape delay instead of clicking.
class StereoDelay {
public:
    void prepare(float sampleRate) {
        sr_ = sampleRate;
        const int size = static_cast<int>(sampleRate * 2.6f) + 8;
        bufL_.assign(size, 0.0f);
        bufR_.assign(size, 0.0f);
        pos_ = 0;
        time_.setTime(0.12f, sampleRate);
        mix_.setTime(0.03f, sampleRate);
        const float g = fastTan(kPi * 90.0f / sampleRate);
        hpG_ = g / (1.0f + g);
        time_.snap(targetSamples_);
        mix_.snap(mix_.target);
    }

    void reset() {
        std::fill(bufL_.begin(), bufL_.end(), 0.0f);
        std::fill(bufR_.begin(), bufR_.end(), 0.0f);
        lpL_.reset(); lpR_.reset(); hpL_.reset(); hpR_.reset();
    }

    void setParams(float mix, float seconds, float feedback, float toneHz, bool pingPong) {
        mix_.target = mix;
        targetSamples_ = std::clamp(seconds * sr_, 16.0f, static_cast<float>(bufL_.size()) - 8.0f);
        time_.target = targetSamples_;
        feedback_ = std::clamp(feedback, 0.0f, 0.95f);
        const float g = fastTan(kPi * std::min(toneHz, 0.45f * sr_) / sr_);
        lpG_ = g / (1.0f + g);
        pingPong_ = pingPong;
    }

    void process(float& l, float& r) {
        const float mix = mix_.tick();
        const float d = time_.tick();
        const float wl = hermiteRead(bufL_, pos_, d);
        const float wr = hermiteRead(bufR_, pos_, d);

        // Feedback path: high-pass (no mud) -> tone low-pass -> soft saturation.
        const float fl = fastTanh(lpL_.process(hpL_.process(wl, hpG_), lpG_) * feedback_ * 1.1f) / 1.1f;
        const float fr = fastTanh(lpR_.process(hpR_.process(wr, hpG_), lpG_) * feedback_ * 1.1f) / 1.1f;

        if (pingPong_) {
            bufL_[pos_] = 0.5f * (l + r) + fr;  // input enters left, bounces right
            bufR_[pos_] = fl;
        } else {
            bufL_[pos_] = l + fl;
            bufR_[pos_] = r + fr;
        }
        if (++pos_ >= static_cast<int>(bufL_.size())) pos_ = 0;

        l += wl * mix;
        r += wr * mix;
    }

private:
    float sr_ = 48000.0f;
    std::vector<float> bufL_, bufR_;
    int pos_ = 0;
    Smoother time_, mix_;
    float targetSamples_ = 18000.0f;
    float feedback_ = 0.35f, lpG_ = 0.2f, hpG_ = 0.01f;
    bool pingPong_ = true;
    OnePoleLP lpL_, lpR_;
    OnePoleHP hpL_, hpR_;
};

// ---------------------------------------------------------------- reverb
// 8-line feedback delay network (Householder mixing) with input diffusion,
// damping, gentle delay modulation for a smooth 80s-hall tail, and pre-delay.
class FdnReverb {
public:
    static constexpr int kLines = 8;

    void prepare(float sampleRate) {
        sr_ = sampleRate;
        static const float lineMs[kLines] = {43.7f, 53.9f, 61.3f, 68.9f, 77.1f, 83.9f, 92.3f, 101.9f};
        for (int i = 0; i < kLines; ++i) {
            len_[i] = lineMs[i] * 0.001f * sampleRate;
            lines_[i].assign(static_cast<int>(len_[i] + 64), 0.0f);
            pos_[i] = 0;
            modPhase_[i] = i / float(kLines);
        }
        static const float apMs[4] = {4.77f, 3.59f, 12.73f, 9.31f};
        for (int i = 0; i < 4; ++i) {
            apLen_[i] = std::max(1, static_cast<int>(apMs[i] * 0.001f * sampleRate));
            ap_[i].assign(apLen_[i], 0.0f);
            apPos_[i] = 0;
        }
        pre_.assign(static_cast<int>(0.25f * sampleRate) + 8, 0.0f);
        prePos_ = 0;
        mix_.setTime(0.03f, sampleRate);
        mix_.snap(mix_.target);
        modRate_ = 0.6f / sampleRate;
        update();
    }

    void reset() {
        for (auto& l : lines_) std::fill(l.begin(), l.end(), 0.0f);
        for (auto& a : ap_) std::fill(a.begin(), a.end(), 0.0f);
        std::fill(pre_.begin(), pre_.end(), 0.0f);
        for (auto& d : damp_) d.reset();
    }

    void setParams(float mix, float decaySeconds, float damping, float preDelaySeconds) {
        mix_.target = mix;
        decay_ = decaySeconds;
        damping_ = damping;
        preDelay_ = std::clamp(preDelaySeconds * sr_, 1.0f, static_cast<float>(pre_.size()) - 4.0f);
        update();
    }

    bool active() const { return mix_.value > 1.0e-4f || mix_.target > 0.0f; }

    void process(float& l, float& r) {
        const float mix = mix_.tick();
        if (mix < 1.0e-5f && mix_.target <= 0.0f) return;

        // Pre-delay.
        pre_[prePos_] = 0.5f * (l + r);
        int rp = prePos_ - static_cast<int>(preDelay_);
        if (rp < 0) rp += static_cast<int>(pre_.size());
        float x = pre_[rp];
        if (++prePos_ >= static_cast<int>(pre_.size())) prePos_ = 0;

        // Input diffusion (series all-passes).
        for (int i = 0; i < 4; ++i) {
            const float d = ap_[i][apPos_[i]];
            const float v = x + 0.62f * d;
            ap_[i][apPos_[i]] = v;
            x = d - 0.62f * v;
            if (++apPos_[i] >= apLen_[i]) apPos_[i] = 0;
        }

        // Read lines (first four slowly modulated).
        float y[kLines];
        float sum = 0.0f;
        for (int i = 0; i < kLines; ++i) {
            float delay = len_[i];
            if (i < 4) {
                modPhase_[i] += modRate_ * (1.0f + 0.13f * i);
                if (modPhase_[i] >= 1.0f) modPhase_[i] -= 1.0f;
                delay += modDepth_ * std::sin(2.0f * kPi * modPhase_[i]);
            }
            y[i] = readLinear(i, delay);
            y[i] = damp_[i].process(y[i], dampG_) * gain_[i];
            sum += y[i];
        }

        // Householder feedback matrix + input, write back.
        const float h = sum * (2.0f / kLines);
        for (int i = 0; i < kLines; ++i) {
            lines_[i][pos_[i]] = y[i] - h + x * 0.35f;
            if (++pos_[i] >= static_cast<int>(lines_[i].size())) pos_[i] = 0;
        }

        // Decorrelated stereo taps.
        const float wl = (y[0] - y[1] + y[2] - y[3] + y[4] + y[5] - y[6] - y[7]) * 0.35f;
        const float wr = (y[0] + y[1] - y[2] - y[3] - y[4] + y[5] + y[6] - y[7]) * 0.35f;
        l += wl * mix;
        r += wr * mix;
    }

private:
    float readLinear(int i, float delay) const {
        const auto& buf = lines_[i];
        const int size = static_cast<int>(buf.size());
        float rp = static_cast<float>(pos_[i]) - delay;
        while (rp < 0) rp += size;
        const int a = static_cast<int>(rp) % size;
        const int b = (a + 1) % size;
        const float f = rp - std::floor(rp);
        return buf[a] + (buf[b] - buf[a]) * f;
    }

    void update() {
        for (int i = 0; i < kLines; ++i)
            gain_[i] = std::pow(10.0f, -3.0f * (len_[i] / sr_) / std::max(0.1f, decay_));
        const float cutoff = 18000.0f * std::pow(1500.0f / 18000.0f, damping_);
        const float g = fastTan(kPi * std::min(cutoff, 0.45f * sr_) / sr_);
        dampG_ = g / (1.0f + g);
        modDepth_ = 0.0009f * sr_;  // ~0.9 ms of wobble
    }

    float sr_ = 48000.0f;
    std::array<std::vector<float>, kLines> lines_;
    std::array<float, kLines> len_{}, gain_{}, modPhase_{};
    std::array<int, kLines> pos_{};
    std::array<OnePoleLP, kLines> damp_{};
    std::array<std::vector<float>, 4> ap_;
    std::array<int, 4> apLen_{}, apPos_{};
    std::vector<float> pre_;
    int prePos_ = 0;
    Smoother mix_;
    float decay_ = 2.5f, damping_ = 0.4f, preDelay_ = 960.0f, dampG_ = 0.5f, modDepth_ = 40.0f, modRate_ = 0.0f;
};

} // namespace retro80

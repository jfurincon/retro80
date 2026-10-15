// Retro80 synth engine: plain C++, no plugin-SDK dependencies, so it can be
// tested offline on any machine.
//
// Signal flow per voice (runs at 2x the host rate to keep highs clean):
//   OSC1 (saw/pulse/saw+pulse/tri, PWM) + OSC2 (saw/pulse/tri/sine)
//   + SUB (square, -1 oct) + NOISE  ->  24 dB ladder LPF  ->  1-pole HPF  ->  VCA
// Voices are summed, decimated back to the host rate, then Juno chorus + volume.
#pragma once

#include "building_blocks.h"
#include "../params.h"

#include <array>
#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#include <pmmintrin.h>
#include <xmmintrin.h>
#define RETRO80_HAS_SSE 1
#endif

namespace retro80 {

class Engine {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kOversample = 2;
    static constexpr int kLatency = HalfbandDecimator::kLatency;

    Engine() {
        for (int i = 0; i < kNumPatchParams; ++i) p_[i] = kParamDefs[i].def;
        for (int i = 0; i < kMaxVoices; ++i) voices_[i].rng = Rng(0x1234567u + 7919u * i);
    }

    // ------------------------------------------------------------------ setup
    void setSampleRate(double sr) {
        sr_ = static_cast<float>(sr);
        osr_ = sr_ * kOversample;
        for (auto* s : {&sCutoff_, &sReso_, &sMix1_, &sMix2_, &sMixSub_, &sMixNoise_, &sPw_, &sHpf_,
                        &sVolume_, &sEnvAmt_, &sLfoAmt_, &sPwm_})
            s->setTime(0.015f, sr_);
        chorus_.prepare(sr_);
        updateAll();
        reset();
    }

    void reset() {
        for (auto& v : voices_) v.hardReset();
        decL_.reset();
        decR_.reset();
        chorus_.reset();
        heldCount_ = 0;
        std::fill(std::begin(keyDown_), std::end(keyDown_), false);
        std::fill(std::begin(sustained_), std::end(sustained_), false);
        stackSize_ = 0;
        snapSmoothers();
    }

    // ------------------------------------------------------------------ params
    void setParam(uint32_t id, double plain) {
        if (id >= static_cast<uint32_t>(kNumPatchParams)) return;
        p_[id] = static_cast<float>(plain);
        switch (id) {
            case kFEnvA: case kFEnvD: case kFEnvS: case kFEnvR:
            case kAEnvA: case kAEnvD: case kAEnvS: case kAEnvR:
                updateEnvelopes();
                break;
            case kVoiceMode:
                if (static_cast<int>(plain) != mode_) {
                    mode_ = static_cast<int>(plain);
                    releaseAll();
                    stackSize_ = 0;
                }
                break;
            case kChorus: chorus_.setMode(static_cast<int>(plain)); break;
            case kGlide: updateGlide(); break;
            default: break;
        }
        updateTargets();
    }

    float param(uint32_t id) const { return p_[id]; }

    void setPitchBend(float bipolar) { bend_ = std::clamp(bipolar, -1.0f, 1.0f); }
    void setModWheel(float v) { modWheel_ = std::clamp(v, 0.0f, 1.0f); }
    void setSustainPedal(bool down) {
        if (sustainPedal_ && !down) {
            sustainPedal_ = false;
            for (int n = 0; n < 128; ++n)
                if (sustained_[n]) { sustained_[n] = false; releaseNote(n); }
        }
        sustainPedal_ = down;
    }

    // ------------------------------------------------------------------ notes
    void noteOn(int note, float velocity) {
        if (note < 0 || note > 127) return;
        velocity = std::clamp(velocity, 0.0f, 1.0f);
        if (heldCount_ == 0 && !anyGate()) lfoFade_ = 0.0f;  // Juno-style LFO delay restarts
        if (!keyDown_[note]) { keyDown_[note] = true; ++heldCount_; }
        sustained_[note] = false;

        if (mode_ == 0) {
            Voice& v = pickPolyVoice(note);
            startVoice(v, note, velocity, 0.0f, 0.0f, 1.0f, /*glideFrom*/ -1.0f);
        } else {
            const bool legato = stackSize_ > 0;
            pushStack(note);
            const int n = monoVoiceCount();
            if (legato && voices_[0].gate) {
                for (int i = 0; i < n; ++i) { voices_[i].targetSemi = float(note); voices_[i].note = note; }
                if (p_[kGlide] <= 0.0f) for (int i = 0; i < n; ++i) voices_[i].curSemi = float(note);
            } else {
                const float from = (p_[kGlide] > 0.0f && lastNote_ >= 0) ? float(lastNote_) : -1.0f;
                for (int i = 0; i < n; ++i) {
                    const float spread = n > 1 ? (2.0f * i / (n - 1) - 1.0f) : 0.0f;
                    const float detune = spread * p_[kUnisonDetune] * 30.0f;  // cents
                    const float pan = spread * 0.85f;
                    startVoice(voices_[i], note, velocity, detune, pan, 1.0f / std::sqrt(float(n)), from);
                }
                for (int i = n; i < kMaxVoices; ++i)
                    if (voices_[i].gate) voices_[i].release();
            }
        }
        lastNote_ = note;
    }

    void noteOff(int note) {
        if (note < 0 || note > 127) return;
        if (keyDown_[note]) { keyDown_[note] = false; --heldCount_; }
        if (sustainPedal_) { sustained_[note] = true; return; }
        releaseNote(note);
    }

    void allNotesOff() {
        std::fill(std::begin(keyDown_), std::end(keyDown_), false);
        std::fill(std::begin(sustained_), std::end(sustained_), false);
        heldCount_ = 0;
        stackSize_ = 0;
        releaseAll();
    }

    bool anyVoiceActive() const {
        for (auto& v : voices_) if (v.active) return true;
        return false;
    }

    // ------------------------------------------------------------------ audio
    // Overwrites outL/outR with numSamples of output.
    void render(float* outL, float* outR, int numSamples) {
#ifdef RETRO80_HAS_SSE
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr(csr | 0x8040);  // flush denormals to zero (FTZ + DAZ)
#endif
        for (int i = 0; i < numSamples; ++i) {
            // Per host-sample controls.
            const float cutoff = sCutoff_.tick(), reso = sReso_.tick();
            const float m1 = sMix1_.tick(), m2 = sMix2_.tick(), ms = sMixSub_.tick(), mn = sMixNoise_.tick();
            const float pw = sPw_.tick(), pwm = sPwm_.tick(), hpfG = sHpf_.tick();
            const float vol = sVolume_.tick(), envAmt = sEnvAmt_.tick(), lfoAmt = sLfoAmt_.tick();
            const float lfo = tickLfo();

            Ctl c;
            c.k = reso * reso * 0.1f + reso * 4.05f;  // more resolution near the top
            c.k = std::min(c.k, 4.15f);
            c.mix1 = m1; c.mix2 = m2; c.mixSub = ms; c.mixNoise = mn;
            c.pw = std::clamp(pw + pwm * 0.42f * lfo, 0.04f, 0.96f);
            c.hpG = hpfG;
            c.cutoffOct = std::log2(cutoff);
            c.envOct = envAmt * 8.0f;
            c.lfoOct = lfoAmt * 3.0f * lfo;
            c.vibratoSemi = lfo * p_[kLfoPitch] * 2.0f + lfoRaw_ * modWheel_ * 0.5f;
            c.bendSemi = bend_ * p_[kBendRange];

            float accL[kOversample] = {0, 0}, accR[kOversample] = {0, 0};
            for (auto& v : voices_) {
                if (!v.active) continue;
                updateVoiceControl(v, c);
                for (int o = 0; o < kOversample; ++o) {
                    const float s = tickVoice(v, c);
                    accL[o] += s * v.gainL;
                    accR[o] += s * v.gainR;
                }
            }

            float l = decL_.process(accL[0], accL[1]);
            float r = decR_.process(accR[0], accR[1]);
            chorus_.process(l, r);

            const float g = 2.0f * vol * vol * kMasterLevel;
            l *= g;
            r *= g;
            if (!std::isfinite(l) || !std::isfinite(r)) { reset(); l = r = 0.0f; }
            outL[i] = std::clamp(l, -4.0f, 4.0f);
            outR[i] = std::clamp(r, -4.0f, 4.0f);
        }
#ifdef RETRO80_HAS_SSE
        _mm_setcsr(csr);
#endif
    }

private:
    static constexpr float kMasterLevel = 0.32f;

    struct Voice {
        bool active = false, gate = false;
        int note = -1;
        float velocity = 1.0f;
        float curSemi = 60.0f, targetSemi = 60.0f;
        float detuneCents = 0.0f;
        float gainL = 0.7071f, gainR = 0.7071f;
        float p1 = 0.0f, p2 = 0.0f, subSign = 1.0f;
        float inc1 = 0.0f, inc2 = 0.0f;
        float filterG = 0.1f;
        Envelope fenv, aenv;
        LadderFilter filter;
        OnePoleHP hpf;
        // analog drift
        float driftPitch = 0.0f, driftCut = 0.0f, walk = 0.0f, walkTarget = 0.0f;
        int walkCounter = 0;
        uint64_t age = 0;
        Rng rng;

        void release() { gate = false; fenv.gateOff(); aenv.gateOff(); }
        void hardReset() {
            active = gate = false; note = -1;
            fenv.kill(); aenv.kill(); filter.reset(); hpf.reset();
        }
    };

    struct Ctl {
        float k, mix1, mix2, mixSub, mixNoise, pw, hpG;
        float cutoffOct, envOct, lfoOct, vibratoSemi, bendSemi;
    };

    // ------------------------------------------------------------------ voices
    void startVoice(Voice& v, int note, float vel, float detuneCents, float pan, float gain, float glideFrom) {
        const bool wasActive = v.active;
        v.active = true;
        v.gate = true;
        v.note = note;
        v.velocity = vel;
        v.targetSemi = float(note);
        v.curSemi = glideFrom >= 0.0f ? glideFrom : float(note);
        v.detuneCents = detuneCents;
        const float angle = (pan + 1.0f) * 0.25f * kPi;  // constant-power pan
        v.gainL = std::cos(angle) * gain * 1.41421356f * 0.7071f;
        v.gainR = std::sin(angle) * gain * 1.41421356f * 0.7071f;
        if (!wasActive) {
            // Free-running oscillators like a real poly: random start phase.
            v.p1 = v.rng.unipolar();
            v.p2 = v.rng.unipolar();
            v.subSign = 1.0f;
            v.filter.reset();
            v.hpf.reset();
            v.walk = v.walkTarget = 0.0f;
        }
        // Each note lands slightly differently, like an old analog.
        v.driftPitch = v.rng.bipolar();
        v.driftCut = v.rng.bipolar();
        v.age = ++ageCounter_;
        v.fenv.gateOn();
        v.aenv.gateOn();
    }

    void updateVoiceControl(Voice& v, const Ctl& c) {
        // Glide (mono/unison only), in the pitch domain.
        if (mode_ != 0 && glideCoef_ < 1.0f) v.curSemi += (v.targetSemi - v.curSemi) * glideCoef_;
        else v.curSemi = v.targetSemi;

        // Slow random wander per voice (~0.5 s steps, smoothed).
        if (--v.walkCounter <= 0) { v.walkCounter = static_cast<int>(sr_ * 0.5f); v.walkTarget = v.rng.bipolar(); }
        v.walk += (v.walkTarget - v.walk) * walkCoef_;

        const float drift = p_[kDrift];
        const float driftCents = drift * (4.0f * v.driftPitch + 5.0f * v.walk);
        const float semi = v.curSemi + c.bendSemi + c.vibratoSemi + (v.detuneCents + driftCents) * 0.01f;
        const float hz1 = 440.0f * std::exp2((semi - 69.0f) * (1.0f / 12.0f));
        v.inc1 = std::min(hz1 / osr_, 0.45f);
        const float semi2 = semi + p_[kOsc2Semi] + p_[kOsc2Fine] * 0.01f;
        v.inc2 = std::min(440.0f * std::exp2((semi2 - 69.0f) * (1.0f / 12.0f)) / osr_, 0.45f);

        // Filter cutoff in octaves.
        const float key = p_[kFiltKeytrack] * (v.curSemi - 60.0f) * (1.0f / 12.0f);
        const float vel = p_[kFiltVel] * (v.velocity - 0.75f) * 4.0f;
        const float oct = c.cutoffOct + c.envOct * v.fenv.value + c.lfoOct + key + vel + drift * 0.12f * v.driftCut;
        const float fc = std::clamp(std::exp2(oct), 16.0f, 0.42f * osr_);
        v.filterG = fastTan(kPi * fc / osr_);
    }

    float tickVoice(Voice& v, const Ctl& c) {
        const float fe = v.fenv.tick(fenvCoefs_);
        (void)fe;
        const float ae = v.aenv.tick(aenvCoefs_);
        if (v.aenv.idle()) { v.hardReset(); return 0.0f; }

        // ---- OSC 1
        const float t1 = v.p1, dt1 = v.inc1;
        float o1 = 0.0f;
        const int w1 = static_cast<int>(p_[kOsc1Wave]);
        if (w1 == 0 || w1 == 2) o1 += 2.0f * t1 - 1.0f - polyBlep(t1, dt1);
        if (w1 == 1 || w1 == 2) o1 += pulse(t1, dt1, c.pw);
        if (w1 == 2) o1 *= 0.7f;
        if (w1 == 3) o1 = 2.0f * std::fabs(2.0f * t1 - 1.0f) - 1.0f;

        // ---- SUB (square one octave down, flips on every OSC1 cycle)
        const float subEdge = (t1 < dt1) ? v.subSign : -v.subSign;
        const float sub = v.subSign + subEdge * polyBlep(t1, dt1);

        // ---- OSC 2
        const float t2 = v.p2, dt2 = v.inc2;
        float o2 = 0.0f;
        switch (static_cast<int>(p_[kOsc2Wave])) {
            case 0: o2 = 2.0f * t2 - 1.0f - polyBlep(t2, dt2); break;
            case 1: o2 = pulse(t2, dt2, 0.5f); break;
            case 2: o2 = 2.0f * std::fabs(2.0f * t2 - 1.0f) - 1.0f; break;
            default: o2 = std::sin(2.0f * kPi * t2); break;
        }

        // ---- advance phases
        v.p1 += dt1;
        if (v.p1 >= 1.0f) { v.p1 -= 1.0f; v.subSign = -v.subSign; }
        v.p2 += dt2;
        if (v.p2 >= 1.0f) v.p2 -= 1.0f;

        const float noise = v.rng.bipolar();
        const float mix = o1 * c.mix1 + o2 * c.mix2 + sub * c.mixSub + noise * c.mixNoise * 0.7f;

        // ---- filters + VCA
        float y = v.filter.process(mix * 0.55f, v.filterG, c.k) * (1.0f / 0.55f);
        y = v.hpf.process(y, c.hpG);
        const float velGain = 1.0f - p_[kAmpVel] * (1.0f - v.velocity);
        return y * ae * velGain * kVoiceLevel;
    }

    static float pulse(float t, float dt, float pw) {
        float s = t < pw ? 1.0f : -1.0f;
        s += polyBlep(t, dt);
        float t2 = t - pw;
        if (t2 < 0.0f) t2 += 1.0f;
        s -= polyBlep(t2, dt);
        return s - (2.0f * pw - 1.0f);  // remove DC so width changes don't thump
    }

    Voice& pickPolyVoice(int note) {
        // Re-strike the same key on its own voice (like a real poly).
        for (auto& v : voices_) if (v.active && v.note == note) return v;
        for (auto& v : voices_) if (!v.active) return v;
        Voice* best = nullptr;  // quietest releasing voice
        for (auto& v : voices_)
            if (!v.gate && (!best || v.aenv.value < best->aenv.value)) best = &v;
        if (best) return *best;
        best = &voices_[0];  // otherwise the oldest
        for (auto& v : voices_) if (v.age < best->age) best = &v;
        return *best;
    }

    int monoVoiceCount() const {
        return mode_ == 2 ? std::clamp(static_cast<int>(p_[kUnisonVoices]), 2, 8) : 1;
    }

    void releaseNote(int note) {
        if (mode_ == 0) {
            for (auto& v : voices_) if (v.gate && v.note == note) v.release();
            return;
        }
        const bool wasCurrent = stackSize_ > 0 && stack_[stackSize_ - 1] == note;
        removeFromStack(note);
        const int n = monoVoiceCount();
        if (stackSize_ == 0) {
            for (int i = 0; i < kMaxVoices; ++i) if (voices_[i].gate) voices_[i].release();
        } else if (wasCurrent) {
            const int back = stack_[stackSize_ - 1];  // last-note priority, legato
            for (int i = 0; i < n; ++i) {
                voices_[i].targetSemi = float(back);
                voices_[i].note = back;
                if (p_[kGlide] <= 0.0f) voices_[i].curSemi = float(back);
            }
            lastNote_ = back;
        }
    }

    void releaseAll() { for (auto& v : voices_) if (v.active) v.release(); }

    bool anyGate() const {
        for (auto& v : voices_) if (v.gate) return true;
        return false;
    }

    void pushStack(int note) {
        removeFromStack(note);
        if (stackSize_ < static_cast<int>(stack_.size())) stack_[stackSize_++] = note;
    }
    void removeFromStack(int note) {
        int w = 0;
        for (int r = 0; r < stackSize_; ++r) if (stack_[r] != note) stack_[w++] = stack_[r];
        stackSize_ = w;
    }

    // ------------------------------------------------------------------ LFO
    float tickLfo() {
        const float rate = p_[kLfoRate];
        lfoPhase_ += rate / sr_;
        bool wrapped = false;
        if (lfoPhase_ >= 1.0f) { lfoPhase_ -= 1.0f; wrapped = true; }
        const float t = lfoPhase_;
        float v = 0.0f;
        switch (static_cast<int>(p_[kLfoWave])) {
            case 0: v = 1.0f - 4.0f * std::fabs(t - 0.5f); break;
            case 1: v = std::sin(2.0f * kPi * t); break;
            case 2: v = t < 0.5f ? 1.0f : -1.0f; break;
            case 3: v = 1.0f - 2.0f * t; break;
            default:
                if (wrapped) shTarget_ = lfoRng_.bipolar();
                v = shTarget_;
                break;
        }
        // Soften square/S&H/saw edges slightly (~2 ms) so they don't click.
        lfoSmoothed_ += (v - lfoSmoothed_) * lfoEdgeCoef_;
        lfoRaw_ = lfoSmoothed_;

        // Delay: fade-in after the first key of a phrase.
        const float delay = p_[kLfoDelay];
        if (delay <= 0.001f) lfoFade_ = 1.0f;
        else lfoFade_ = std::min(1.0f, lfoFade_ + 1.0f / (delay * sr_));
        return lfoSmoothed_ * lfoFade_;
    }

    // ------------------------------------------------------------------ param plumbing
    void updateTargets() {
        sCutoff_.target = p_[kCutoff];
        sReso_.target = p_[kReso];
        sMix1_.target = p_[kMixOsc1];
        sMix2_.target = p_[kMixOsc2];
        sMixSub_.target = p_[kMixSub];
        sMixNoise_.target = p_[kMixNoise];
        sPw_.target = p_[kOsc1PW];
        sPwm_.target = p_[kOsc1PWM];
        const float g = fastTan(kPi * std::min(p_[kHpf], 0.4f * osr_) / osr_);
        sHpf_.target = g / (1.0f + g);
        sVolume_.target = p_[kVolume];
        sEnvAmt_.target = p_[kFiltEnvAmt];
        sLfoAmt_.target = p_[kFiltLfoAmt];
    }

    void snapSmoothers() {
        updateTargets();
        for (auto* s : {&sCutoff_, &sReso_, &sMix1_, &sMix2_, &sMixSub_, &sMixNoise_, &sPw_, &sHpf_,
                        &sVolume_, &sEnvAmt_, &sLfoAmt_, &sPwm_})
            s->snap(s->target);
    }

    void updateEnvelopes() {
        fenvCoefs_.set(p_[kFEnvA], p_[kFEnvD], p_[kFEnvS], p_[kFEnvR], osr_);
        aenvCoefs_.set(p_[kAEnvA], p_[kAEnvD], p_[kAEnvS], p_[kAEnvR], osr_);
    }

    void updateGlide() {
        const float t = p_[kGlide];
        glideCoef_ = t <= 0.0005f ? 1.0f : 1.0f - std::exp(-1.0f / (t * sr_ * 0.3f));
    }

    void updateAll() {
        updateEnvelopes();
        updateGlide();
        updateTargets();
        mode_ = static_cast<int>(p_[kVoiceMode]);
        chorus_.setMode(static_cast<int>(p_[kChorus]), true);
        walkCoef_ = 1.0f - std::exp(-1.0f / (0.4f * sr_));
        lfoEdgeCoef_ = 1.0f - std::exp(-1.0f / (0.002f * sr_));
    }

    static constexpr float kVoiceLevel = 0.5f;

    float sr_ = 48000.0f, osr_ = 96000.0f;
    std::array<float, kNumPatchParams> p_{};
    std::array<Voice, kMaxVoices> voices_{};
    EnvCoefs fenvCoefs_, aenvCoefs_;
    Smoother sCutoff_, sReso_, sMix1_, sMix2_, sMixSub_, sMixNoise_, sPw_, sPwm_, sHpf_, sVolume_, sEnvAmt_, sLfoAmt_;
    HalfbandDecimator decL_, decR_;
    JunoChorus chorus_;

    int mode_ = 0;
    float glideCoef_ = 1.0f, walkCoef_ = 0.0f, lfoEdgeCoef_ = 1.0f;
    float bend_ = 0.0f, modWheel_ = 0.0f;
    bool sustainPedal_ = false;
    bool keyDown_[128] = {};
    bool sustained_[128] = {};
    int heldCount_ = 0;
    std::array<int, 32> stack_{};
    int stackSize_ = 0;
    int lastNote_ = -1;
    uint64_t ageCounter_ = 0;

    float lfoPhase_ = 0.0f, lfoFade_ = 1.0f, lfoSmoothed_ = 0.0f, lfoRaw_ = 0.0f, shTarget_ = 0.0f;
    Rng lfoRng_{0xBADC0DEu};
};

} // namespace retro80

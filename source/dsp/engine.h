// Retro80 synth engine: plain C++, no plugin-SDK dependencies, so it can be
// tested offline on any machine.
//
// Per voice (2x oversampled):
//   OSC1 (saw/pulse/saw+pulse/tri, PWM, cross-mod from OSC2)
//   OSC2 (saw/pulse/tri/sine, hard sync to OSC1, envelope sweep)
//   + SUB + NOISE -> 24 dB ladder LPF -> HPF -> VCA (+ mod matrix on most stages)
// Then: decimate -> chorus -> delay -> reverb -> master volume.
#pragma once

#include "arp.h"
#include "building_blocks.h"
#include "fx.h"
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

class Engine : private Arpeggiator::Output {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kOversample = 2;
    static constexpr int kLatency = HalfbandDecimator::kLatency;
    static constexpr int kScopeSize = 2048;

    Engine() {
        for (int i = 0; i < kNumPatchParams; ++i) p_[i] = kParamDefs[i].def;
        for (int i = 0; i < kMaxVoices; ++i) voices_[i].rng = Rng(0x1234567u + 7919u * i);
        updateMatrix();
    }

    // ------------------------------------------------------------------ setup
    void setSampleRate(double sr) {
        sr_ = static_cast<float>(sr);
        osr_ = sr_ * kOversample;
        for (auto* s : smoothers()) s->setTime(0.015f, sr_);
        chorus_.prepare(sr_);
        delay_.prepare(sr_);
        reverb_.prepare(sr_);
        updateAll();
        reset();
    }

    void reset() {
        for (auto& v : voices_) v.hardReset();
        decL_.reset();
        decR_.reset();
        chorus_.reset();
        delay_.reset();
        reverb_.reset();
        heldCount_ = 0;
        std::fill(std::begin(keyDown_), std::end(keyDown_), false);
        std::fill(std::begin(sustained_), std::end(sustained_), false);
        stackSize_ = 0;
        arp_.clearAll(*this);
        snapSmoothers();
    }

    // ------------------------------------------------------------------ params
    void setParam(uint32_t id, double plain) {
        if (id >= static_cast<uint32_t>(kNumPatchParams)) return;
        p_[id] = static_cast<float>(plain);
        switch (id) {
            case kFEnvA: case kFEnvD: case kFEnvS: case kFEnvR:
            case kAEnvA: case kAEnvD: case kAEnvS: case kAEnvR:
            case kMEnvA: case kMEnvD: case kMEnvS: case kMEnvR:
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
            case kArpOn: {
                const bool on = static_cast<int>(plain) != 0;
                if (on != arpOn_) {  // switching modes: start clean, no stuck notes
                    arp_.clearAll(*this);
                    std::fill(std::begin(keyDown_), std::end(keyDown_), false);
                    std::fill(std::begin(sustained_), std::end(sustained_), false);
                    heldCount_ = 0;
                    stackSize_ = 0;
                    releaseAll();
                }
                arpOn_ = on;
                break;
            }
            case kArpMode: case kArpRate: case kArpOctaves: case kArpGate: case kArpLatch:
                updateArp();
                break;
            case kDlyMix: case kDlyTime: case kDlySync: case kDlyFeedback: case kDlyTone: case kDlyPingPong:
                updateDelay();
                break;
            case kRevMix: case kRevDecay: case kRevDamp: case kRevPreDelay:
                updateReverb();
                break;
            default:
                if (id >= kMod1Src && id <= kMod4Amt) updateMatrix();
                break;
        }
        updateTargets();
    }

    float param(uint32_t id) const { return p_[id]; }

    void setPitchBend(float bipolar) { bend_ = std::clamp(bipolar, -1.0f, 1.0f); }
    void setModWheel(float v) { modWheel_ = std::clamp(v, 0.0f, 1.0f); }
    void setAftertouch(float v) { aftertouch_ = std::clamp(v, 0.0f, 1.0f); }
    void setPolyPressure(int note, float v) {
        for (auto& voice : voices_) if (voice.active && voice.note == note) voice.pressure = std::clamp(v, 0.0f, 1.0f);
    }
    void setSustainPedal(bool down) {
        if (sustainPedal_ && !down) {
            sustainPedal_ = false;
            for (int n = 0; n < 128; ++n)
                if (sustained_[n]) { sustained_[n] = false; voiceNoteOff(n); }
        }
        sustainPedal_ = down;
    }

    // Host tempo/position, once per audio block (before render).
    void setTransport(double bpm, double ppq, bool playing) {
        tempo_ = std::clamp(bpm > 0 ? bpm : 120.0, 20.0, 999.0);
        if (playing) ppq_ = ppq;  // follow the song; when stopped keep our own clock running
        playing_ = playing;
        updateDelay();
    }

    // ------------------------------------------------------------------ notes
    void noteOn(int note, float velocity) {
        if (note < 0 || note > 127) return;
        velocity = std::clamp(velocity, 0.0f, 1.0f);
        if (arpOn_) { arp_.press(note, velocity); return; }
        if (!keyDown_[note]) { keyDown_[note] = true; ++heldCount_; }
        sustained_[note] = false;
        voiceNoteOn(note, velocity);
    }

    void noteOff(int note) {
        if (note < 0 || note > 127) return;
        if (arpOn_) { arp_.release(note, *this); return; }
        if (keyDown_[note]) { keyDown_[note] = false; --heldCount_; }
        if (sustainPedal_) { sustained_[note] = true; return; }
        voiceNoteOff(note);
    }

    void allNotesOff() {
        std::fill(std::begin(keyDown_), std::end(keyDown_), false);
        std::fill(std::begin(sustained_), std::end(sustained_), false);
        heldCount_ = 0;
        stackSize_ = 0;
        arp_.clearAll(*this);
        releaseAll();
    }

    bool anyVoiceActive() const {
        for (auto& v : voices_) if (v.active) return true;
        return false;
    }

    // ------------------------------------------------------------------ scope tap
    // Ring buffer of the voice mix (before effects) for the panel's wave display.
    const float* scopeBuffer() const { return scope_.data(); }
    int scopeWritePos() const { return scopePos_; }
    int activeVoices() const {
        int n = 0;
        for (auto& v : voices_) if (v.active && v.gate) ++n;
        return n;
    }
    // Pitch of the lowest held voice, used to lock the display to the waveform.
    float lowestActiveHz() const {
        float lo = 0.0f;
        for (auto& v : voices_)
            if (v.active && v.gate) {
                const float hz = 440.0f * std::exp2((v.curSemi - 69.0f) / 12.0f);
                if (lo == 0.0f || hz < lo) lo = hz;
            }
        return lo;
    }

    // ------------------------------------------------------------------ audio
    void render(float* outL, float* outR, int numSamples) {
#ifdef RETRO80_HAS_SSE
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr(csr | 0x8040);  // flush denormals to zero (FTZ + DAZ)
#endif
        const double beatsPerSample = tempo_ / 60.0 / sr_;
        for (int i = 0; i < numSamples; ++i) {
            ppq_ += beatsPerSample;
            if (arpOn_) arp_.tick(ppq_, playing_, *this);

            Ctl c;
            c.cutoff = sCutoff_.tick();
            c.reso = sReso_.tick();
            c.mix1 = sMix1_.tick(); c.mix2 = sMix2_.tick(); c.mixSub = sMixSub_.tick(); c.mixNoise = sMixNoise_.tick();
            const float pw = sPw_.tick(), pwm = sPwm_.tick();
            c.hpG = sHpf_.tick();
            const float vol = sVolume_.tick(), envAmt = sEnvAmt_.tick(), lfoAmt = sLfoAmt_.tick();
            c.xmod = sXMod_.tick();
            c.sweep = sSweep_.tick();

            const float lfo1 = tickLfo(lfo1_, p_[kLfoWave], p_[kLfoRate], p_[kLfoSync], true);
            const float lfo2 = tickLfo(lfo2_, p_[kLfo2Wave], p_[kLfo2Rate], p_[kLfo2Sync], false);
            c.lfo1 = lfo1;
            c.lfo2 = lfo2;
            c.pw = pw + pwm * 0.42f * lfo1;
            c.cutoffOct = std::log2(c.cutoff);
            c.envOct = envAmt * 8.0f;
            c.lfoOct = lfoAmt * 3.0f * lfo1;
            c.vibratoSemi = lfo1 * p_[kLfoPitch] * 2.0f + lfo1_.raw * modWheel_ * 0.5f;
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

            scope_[scopePos_] = 0.5f * (l + r);
            if (++scopePos_ >= kScopeSize) scopePos_ = 0;

            chorus_.process(l, r);
            delay_.process(l, r);
            reverb_.process(l, r);

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
    static constexpr float kVoiceLevel = 0.5f;

    struct Lfo {
        float phase = 0.0f, smoothed = 0.0f, raw = 0.0f, fade = 1.0f, sh = 0.0f;
        Rng rng{0xBADC0DEu};
    };

    struct Voice {
        bool active = false, gate = false;
        int note = -1;
        float velocity = 1.0f, pressure = 0.0f, randomVal = 0.0f;
        float curSemi = 60.0f, targetSemi = 60.0f;
        float detuneCents = 0.0f, basePan = 0.0f, baseGain = 1.0f;
        float gainL = 0.7071f, gainR = 0.7071f;
        // oscillators
        float p1 = 0.0f, p2 = 0.0f, subSign = 1.0f;
        float inc1 = 0.0f, inc2 = 0.0f;
        float osc2Prev = 0.0f, osc2Out = 0.0f;
        // per-voice modulated controls (updated once per host sample)
        float filterG = 0.1f, k = 0.0f, pw = 0.5f, ampMul = 1.0f, xmod = 0.0f;
        float mix1 = 1.0f, mix2 = 0.0f, mixSub = 0.0f, mixNoise = 0.0f;
        Envelope fenv, aenv, menv;
        LadderFilter filter;
        OnePoleHP hpf;
        // analog drift
        float driftPitch = 0.0f, driftCut = 0.0f, walk = 0.0f, walkTarget = 0.0f;
        int walkCounter = 0;
        uint64_t age = 0;
        Rng rng;

        void release() { gate = false; fenv.gateOff(); aenv.gateOff(); menv.gateOff(); }
        void hardReset() {
            active = gate = false; note = -1;
            fenv.kill(); aenv.kill(); menv.kill(); filter.reset(); hpf.reset();
            osc2Prev = osc2Out = 0.0f;
        }
    };

    struct Ctl {
        float cutoff, reso, mix1, mix2, mixSub, mixNoise, pw, hpG, xmod, sweep;
        float cutoffOct, envOct, lfoOct, vibratoSemi, bendSemi, lfo1, lfo2;
    };

    struct MatrixSlot { int src, dst; float amt; };

    // ------------------------------------------------------------------ arp -> voices
    void arpNoteOn(int note, float velocity) override { voiceNoteOn(note, velocity); }
    void arpNoteOff(int note) override { voiceNoteOff(note); }

    // ------------------------------------------------------------------ voice allocation
    void voiceNoteOn(int note, float velocity) {
        if (!anyGate()) lfo1_.fade = 0.0f;  // Juno-style LFO delay restarts on a new phrase

        if (mode_ == 0) {
            Voice& v = pickPolyVoice(note);
            startVoice(v, note, velocity, 0.0f, 0.0f, 1.0f, -1.0f);
        } else {
            const bool legato = stackSize_ > 0 && voices_[0].gate;
            pushStack(note);
            const int n = monoVoiceCount();
            if (legato) {
                for (int i = 0; i < n; ++i) { voices_[i].targetSemi = float(note); voices_[i].note = note; }
                if (p_[kGlide] <= 0.0f) for (int i = 0; i < n; ++i) voices_[i].curSemi = float(note);
            } else {
                const float from = (p_[kGlide] > 0.0f && lastNote_ >= 0) ? float(lastNote_) : -1.0f;
                for (int i = 0; i < n; ++i) {
                    const float spread = n > 1 ? (2.0f * i / (n - 1) - 1.0f) : 0.0f;
                    startVoice(voices_[i], note, velocity, spread * p_[kUnisonDetune] * 30.0f, spread * 0.85f,
                               1.0f / std::sqrt(float(n)), from);
                }
                for (int i = n; i < kMaxVoices; ++i)
                    if (voices_[i].gate) voices_[i].release();
            }
        }
        lastNote_ = note;
    }

    void voiceNoteOff(int note) {
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

    void startVoice(Voice& v, int note, float vel, float detuneCents, float pan, float gain, float glideFrom) {
        const bool wasActive = v.active;
        v.active = true;
        v.gate = true;
        v.note = note;
        v.velocity = vel;
        v.pressure = 0.0f;
        v.randomVal = v.rng.bipolar();
        v.targetSemi = float(note);
        v.curSemi = glideFrom >= 0.0f ? glideFrom : float(note);
        v.detuneCents = detuneCents;
        v.basePan = pan;
        v.baseGain = gain;
        setPan(v, pan);
        if (!wasActive) {
            // Free-running oscillators like a real poly: random start phase.
            v.p1 = v.rng.unipolar();
            v.p2 = v.rng.unipolar();
            v.subSign = 1.0f;
            v.osc2Prev = v.osc2Out = 0.0f;
            v.filter.reset();
            v.hpf.reset();
            v.walk = v.walkTarget = 0.0f;
        }
        v.driftPitch = v.rng.bipolar();
        v.driftCut = v.rng.bipolar();
        v.age = ++ageCounter_;
        v.fenv.gateOn();
        v.aenv.gateOn();
        v.menv.gateOn();
    }

    static void setPan(Voice& v, float pan) {
        const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;  // constant power
        v.gainL = std::cos(angle) * v.baseGain;
        v.gainR = std::sin(angle) * v.baseGain;
    }

    // ------------------------------------------------------------------ per host sample, per voice
    void updateVoiceControl(Voice& v, const Ctl& c) {
        if (mode_ != 0 && glideCoef_ < 1.0f) v.curSemi += (v.targetSemi - v.curSemi) * glideCoef_;
        else v.curSemi = v.targetSemi;

        if (--v.walkCounter <= 0) { v.walkCounter = static_cast<int>(sr_ * 0.5f); v.walkTarget = v.rng.bipolar(); }
        v.walk += (v.walkTarget - v.walk) * walkCoef_;

        // ---- modulation matrix
        float mPitch = 0, mOsc2 = 0, mPw = 0, mCut = 0, mReso = 0, mAmp = 0, mPan = 0, mX = 0;
        float m1 = 0, m2 = 0, mSub = 0, mNoise = 0;
        for (int s = 0; s < numSlots_; ++s) {
            const MatrixSlot& slot = slots_[s];
            float src = 0.0f;
            switch (slot.src) {
                case kSrcLfo1: src = c.lfo1; break;
                case kSrcLfo2: src = c.lfo2; break;
                case kSrcModEnv: src = v.menv.value; break;
                case kSrcVelocity: src = v.velocity; break;
                case kSrcModWheel: src = modWheel_; break;
                case kSrcAftertouch: src = std::max(aftertouch_, v.pressure); break;
                case kSrcKey: src = (v.curSemi - 60.0f) / 24.0f; break;
                case kSrcRandom: src = v.randomVal; break;
                default: break;
            }
            const float m = src * slot.amt;
            switch (slot.dst) {
                case kDstPitch: mPitch += m * 12.0f; break;
                case kDstOsc2Pitch: mOsc2 += m * 24.0f; break;
                case kDstPW: mPw += m * 0.45f; break;
                case kDstCutoff: mCut += m * 5.0f; break;
                case kDstReso: mReso += m; break;
                case kDstAmp: mAmp += m; break;
                case kDstPan: mPan += m; break;
                case kDstXMod: mX += m; break;
                case kDstOsc1Lvl: m1 += m; break;
                case kDstOsc2Lvl: m2 += m; break;
                case kDstSubLvl: mSub += m; break;
                case kDstNoiseLvl: mNoise += m; break;
                default: break;
            }
        }

        const float drift = p_[kDrift];
        const float driftCents = drift * (4.0f * v.driftPitch + 5.0f * v.walk);
        const float semi = v.curSemi + c.bendSemi + c.vibratoSemi + mPitch + (v.detuneCents + driftCents) * 0.01f;
        v.inc1 = std::min(440.0f * std::exp2((semi - 69.0f) * (1.0f / 12.0f)) / osr_, 0.45f);
        const float semi2 = semi + p_[kOsc2Semi] + p_[kOsc2Fine] * 0.01f + c.sweep * 36.0f * v.menv.value + mOsc2;
        v.inc2 = std::clamp(440.0f * std::exp2((semi2 - 69.0f) * (1.0f / 12.0f)) / osr_, 1.0e-6f, 0.45f);

        const float key = p_[kFiltKeytrack] * (v.curSemi - 60.0f) * (1.0f / 12.0f);
        const float vel = p_[kFiltVel] * (v.velocity - 0.75f) * 4.0f;
        const float oct = c.cutoffOct + c.envOct * v.fenv.value + c.lfoOct + key + vel + drift * 0.12f * v.driftCut + mCut;
        const float fc = std::clamp(std::exp2(oct), 16.0f, 0.42f * osr_);
        v.filterG = fastTan(kPi * fc / osr_);

        const float reso = std::clamp(c.reso + mReso, 0.0f, 1.0f);
        v.k = std::min(reso * reso * 0.1f + reso * 4.05f, 4.15f);
        v.pw = std::clamp(c.pw + mPw, 0.04f, 0.96f);
        v.ampMul = std::max(0.0f, 1.0f + mAmp);
        v.xmod = std::clamp(c.xmod + mX, 0.0f, 1.0f);
        v.mix1 = std::max(0.0f, c.mix1 + m1);
        v.mix2 = std::max(0.0f, c.mix2 + m2);
        v.mixSub = std::max(0.0f, c.mixSub + mSub);
        v.mixNoise = std::max(0.0f, c.mixNoise + mNoise);
        if (panModulated_) setPan(v, v.basePan + mPan);
    }

    // ------------------------------------------------------------------ per oversampled sample
    static float naive2(int wave, float t) {
        switch (wave) {
            case 0: return 2.0f * t - 1.0f;
            case 1: return t < 0.5f ? 1.0f : -1.0f;
            case 2: return 2.0f * std::fabs(2.0f * t - 1.0f) - 1.0f;
            default: return std::sin(2.0f * kPi * t);
        }
    }

    float tickVoice(Voice& v, const Ctl& c) {
        v.fenv.tick(fenvCoefs_);
        v.menv.tick(menvCoefs_);
        const float ae = v.aenv.tick(aenvCoefs_);
        if (v.aenv.idle()) { v.hardReset(); return 0.0f; }

        // ---- OSC 1 (cross-modulated by OSC 2)
        float dt1 = v.inc1;
        if (v.xmod > 0.001f) dt1 = std::min(dt1 * std::exp2(v.xmod * v.osc2Out * 2.5f), 0.45f);
        const float t1 = v.p1;
        float o1 = 0.0f;
        const int w1 = static_cast<int>(p_[kOsc1Wave]);
        if (w1 == 0 || w1 == 2) o1 += 2.0f * t1 - 1.0f - polyBlep(t1, dt1);
        if (w1 == 1 || w1 == 2) o1 += pulse(t1, dt1, v.pw);
        if (w1 == 2) o1 *= 0.7f;
        if (w1 == 3) o1 = 2.0f * std::fabs(2.0f * t1 - 1.0f) - 1.0f;

        // ---- SUB (square one octave down)
        const float subEdge = (t1 < dt1) ? v.subSign : -v.subSign;
        const float sub = v.subSign + subEdge * polyBlep(t1, dt1);

        v.p1 += dt1;
        bool wrapped1 = false;
        float d1 = 0.0f;
        if (v.p1 >= 1.0f) {
            v.p1 -= 1.0f;
            v.subSign = -v.subSign;
            wrapped1 = true;
            d1 = std::min(v.p1 / dt1, 1.0f);
        }

        // ---- OSC 2 (one sample behind, so its BLEP corrections can fix the
        // previous sample too: that is what makes hard sync clean)
        const float o2 = tickOsc2(v, static_cast<int>(p_[kOsc2Wave]), p_[kOscSync] > 0.5f && wrapped1, d1);
        v.osc2Out = o2;

        const float noise = v.rng.bipolar();
        const float mix = o1 * v.mix1 + o2 * v.mix2 + sub * v.mixSub + noise * v.mixNoise * 0.7f;

        float y = v.filter.process(mix * 0.55f, v.filterG, v.k) * (1.0f / 0.55f);
        y = v.hpf.process(y, c.hpG);
        const float velGain = 1.0f - p_[kAmpVel] * (1.0f - v.velocity);
        return y * ae * velGain * v.ampMul * kVoiceLevel;
    }

    static float tickOsc2(Voice& v, int wave, bool syncNow, float d1) {
        const float dt = v.inc2;
        const float pOld = v.p2;
        float corrPrev = 0.0f, corrCur = 0.0f;
        // A jump J that happened d samples ago (0 < d <= 1).
        auto blep = [&](float d, float J) {
            d = std::clamp(d, 0.0f, 1.0f);
            corrCur += J * 0.5f * (-(1.0f - d) * (1.0f - d));
            corrPrev += J * 0.5f * (d * d);
        };
        const bool hasEdges = wave == 0 || wave == 1;

        if (syncNow) {
            const float tReset = 1.0f - d1;  // when (within this sample) OSC1 wrapped
            float pAt = pOld + dt * tReset;
            float segStart = pOld, tSeg = 0.0f;
            if (pAt >= 1.0f) {  // OSC2 wrapped on its own first
                const float tw = (1.0f - pOld) / dt;
                if (hasEdges) blep(1.0f - tw, wave == 0 ? -2.0f : 2.0f);
                pAt -= 1.0f;
                segStart = 0.0f;
                tSeg = tw;
            }
            if (wave == 1 && segStart < 0.5f && pAt >= 0.5f) blep(1.0f - (tSeg + (0.5f - segStart) / dt), -2.0f);
            blep(d1, naive2(wave, 0.0f) - naive2(wave, pAt));  // the sync reset itself
            v.p2 = dt * d1;
        } else {
            v.p2 = pOld + dt;
            bool wrapped = false;
            if (v.p2 >= 1.0f) {
                v.p2 -= 1.0f;
                wrapped = true;
                if (hasEdges) blep(v.p2 / dt, wave == 0 ? -2.0f : 2.0f);
            }
            if (wave == 1 && !wrapped && pOld < 0.5f && v.p2 >= 0.5f) blep((v.p2 - 0.5f) / dt, -2.0f);
        }

        const float cur = naive2(wave, v.p2) + corrCur;
        const float out = v.osc2Prev + corrPrev;
        v.osc2Prev = cur;
        return out;
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
        for (auto& v : voices_) if (v.active && v.note == note) return v;
        for (auto& v : voices_) if (!v.active) return v;
        Voice* best = nullptr;
        for (auto& v : voices_)
            if (!v.gate && (!best || v.aenv.value < best->aenv.value)) best = &v;
        if (best) return *best;
        best = &voices_[0];
        for (auto& v : voices_) if (v.age < best->age) best = &v;
        return *best;
    }

    int monoVoiceCount() const {
        return mode_ == 2 ? std::clamp(static_cast<int>(p_[kUnisonVoices]), 2, 8) : 1;
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

    // ------------------------------------------------------------------ LFOs
    float tickLfo(Lfo& l, float waveF, float rateHz, float syncIndexF, bool withDelay) {
        const int syncIndex = static_cast<int>(syncIndexF);
        const double beats = (syncIndex > 0 && syncIndex < countOf(kLfoSyncDivs)) ? kLfoSyncBeats[syncIndex] : 0.0;
        bool wrapped = false;
        if (beats > 0.0 && playing_) {
            // Locked to the song position.
            const float ph = static_cast<float>(ppq_ / beats - std::floor(ppq_ / beats));
            wrapped = ph < l.phase;
            l.phase = ph;
        } else {
            const float rate = beats > 0.0 ? static_cast<float>(tempo_ / 60.0 / beats) : rateHz;
            l.phase += rate / sr_;
            if (l.phase >= 1.0f) { l.phase -= 1.0f; wrapped = true; }
        }
        const float t = l.phase;
        float v = 0.0f;
        switch (static_cast<int>(waveF)) {
            case 0: v = 1.0f - 4.0f * std::fabs(t - 0.5f); break;
            case 1: v = std::sin(2.0f * kPi * t); break;
            case 2: v = t < 0.5f ? 1.0f : -1.0f; break;
            case 3: v = 1.0f - 2.0f * t; break;
            default:
                if (wrapped) l.sh = l.rng.bipolar();
                v = l.sh;
                break;
        }
        l.smoothed += (v - l.smoothed) * lfoEdgeCoef_;  // soften hard edges (~2 ms)
        l.raw = l.smoothed;
        if (!withDelay) return l.smoothed;
        const float delay = p_[kLfoDelay];
        if (delay <= 0.001f) l.fade = 1.0f;
        else l.fade = std::min(1.0f, l.fade + 1.0f / (delay * sr_));
        return l.smoothed * l.fade;
    }

    // ------------------------------------------------------------------ param plumbing
    std::array<Smoother*, 14> smoothers() {
        return {&sCutoff_, &sReso_, &sMix1_, &sMix2_, &sMixSub_, &sMixNoise_, &sPw_, &sPwm_, &sHpf_,
                &sVolume_, &sEnvAmt_, &sLfoAmt_, &sXMod_, &sSweep_};
    }

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
        sXMod_.target = p_[kXMod];
        sSweep_.target = p_[kOsc2Sweep];
    }

    void snapSmoothers() {
        updateTargets();
        for (auto* s : smoothers()) s->snap(s->target);
    }

    void updateEnvelopes() {
        fenvCoefs_.set(p_[kFEnvA], p_[kFEnvD], p_[kFEnvS], p_[kFEnvR], osr_);
        aenvCoefs_.set(p_[kAEnvA], p_[kAEnvD], p_[kAEnvS], p_[kAEnvR], osr_);
        menvCoefs_.set(p_[kMEnvA], p_[kMEnvD], p_[kMEnvS], p_[kMEnvR], osr_);
    }

    void updateGlide() {
        const float t = p_[kGlide];
        glideCoef_ = t <= 0.0005f ? 1.0f : 1.0f - std::exp(-1.0f / (t * sr_ * 0.3f));
    }

    void updateMatrix() {
        numSlots_ = 0;
        panModulated_ = false;
        for (int s = 0; s < kNumModSlots; ++s) {
            const int src = static_cast<int>(p_[kMod1Src + 3 * s]);
            const int dst = static_cast<int>(p_[kMod1Dst + 3 * s]);
            const float amt = p_[kMod1Amt + 3 * s];
            if (src == kSrcOff || dst == kDstOff || std::fabs(amt) < 1.0e-4f) continue;
            slots_[numSlots_++] = {src, dst, amt};
            if (dst == kDstPan) panModulated_ = true;
        }
        if (!panModulated_) for (auto& v : voices_) setPan(v, v.basePan);
    }

    void updateArp() {
        Arpeggiator::Settings s;
        s.mode = static_cast<int>(p_[kArpMode]);
        s.stepBeats = kArpRateBeats[std::clamp(static_cast<int>(p_[kArpRate]), 0, countOf(kArpRates) - 1)];
        s.octaves = std::clamp(static_cast<int>(p_[kArpOctaves]), 1, 4);
        s.gate = p_[kArpGate];
        s.latch = p_[kArpLatch] > 0.5f;
        arp_.setSettings(s);
    }

    void updateDelay() {
        const int sync = static_cast<int>(p_[kDlySync]);
        float seconds = p_[kDlyTime];
        if (sync > 0 && sync < countOf(kDelaySyncDivs))
            seconds = static_cast<float>(kDelaySyncBeats[sync] * 60.0 / tempo_);
        delay_.setParams(p_[kDlyMix], std::min(seconds, 2.5f), p_[kDlyFeedback], p_[kDlyTone], p_[kDlyPingPong] > 0.5f);
    }

    void updateReverb() { reverb_.setParams(p_[kRevMix], p_[kRevDecay], p_[kRevDamp], p_[kRevPreDelay]); }

    void updateAll() {
        updateEnvelopes();
        updateGlide();
        updateTargets();
        updateMatrix();
        updateArp();
        updateDelay();
        updateReverb();
        mode_ = static_cast<int>(p_[kVoiceMode]);
        arpOn_ = p_[kArpOn] > 0.5f;
        chorus_.setMode(static_cast<int>(p_[kChorus]), true);
        walkCoef_ = 1.0f - std::exp(-1.0f / (0.4f * sr_));
        lfoEdgeCoef_ = 1.0f - std::exp(-1.0f / (0.002f * sr_));
    }

    float sr_ = 48000.0f, osr_ = 96000.0f;
    std::array<float, kNumPatchParams> p_{};
    std::array<Voice, kMaxVoices> voices_{};
    EnvCoefs fenvCoefs_, aenvCoefs_, menvCoefs_;
    Smoother sCutoff_, sReso_, sMix1_, sMix2_, sMixSub_, sMixNoise_, sPw_, sPwm_, sHpf_, sVolume_, sEnvAmt_,
        sLfoAmt_, sXMod_, sSweep_;
    HalfbandDecimator decL_, decR_;
    JunoChorus chorus_;
    StereoDelay delay_;
    FdnReverb reverb_;
    Arpeggiator arp_;

    std::array<MatrixSlot, kNumModSlots> slots_{};
    int numSlots_ = 0;
    bool panModulated_ = false;

    int mode_ = 0;
    bool arpOn_ = false;
    float glideCoef_ = 1.0f, walkCoef_ = 0.0f, lfoEdgeCoef_ = 1.0f;
    float bend_ = 0.0f, modWheel_ = 0.0f, aftertouch_ = 0.0f;
    bool sustainPedal_ = false;
    bool keyDown_[128] = {};
    bool sustained_[128] = {};
    int heldCount_ = 0;
    std::array<int, 32> stack_{};
    int stackSize_ = 0;
    int lastNote_ = -1;
    uint64_t ageCounter_ = 0;

    double tempo_ = 120.0, ppq_ = 0.0;
    bool playing_ = false;

    Lfo lfo1_, lfo2_{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, Rng(0xC0FFEEu)};

    std::array<float, kScopeSize> scope_{};
    int scopePos_ = 0;
};

} // namespace retro80

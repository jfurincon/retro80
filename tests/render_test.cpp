// Offline checks for the Retro80 engine:
//   - renders every factory patch playing a short phrase (one demo WAV)
//   - reports peak level / non-finite samples per patch
//   - measures aliasing on a very high saw note
//   - measures CPU cost (worst case: 16-voice pad)
//
// usage: render_test <out_dir>
#include "dsp/engine.h"
#include "presets.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace retro80;

namespace {

constexpr int kSr = 48000;

void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
    FILE* f = std::fopen(path.c_str(), "wb");
    const uint32_t n = static_cast<uint32_t>(l.size());
    const uint32_t dataBytes = n * 4;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32(kSr); u32(kSr * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (float s : {l[i], r[i]}) {
            const int16_t v = static_cast<int16_t>(std::lrint(std::fmax(-1.0f, std::fmin(1.0f, s)) * 32767.0f));
            std::fwrite(&v, 2, 1, f);
        }
    std::fclose(f);
}

void applyPatch(Engine& e, int index) {
    const auto norm = patchNormalized(index);
    for (int i = 0; i < kNumPatchParams; ++i) e.setParam(i, toPlain(kParamDefs[i], norm[i]));
}

struct Ev { double t; int note; float vel; };  // vel 0 = note off

// Renders events with odd block sizes, like a host.
void play(Engine& e, const std::vector<Ev>& evs, double length, std::vector<float>& L, std::vector<float>& R) {
    const size_t start = L.size();
    const size_t total = static_cast<size_t>(length * kSr);
    L.resize(start + total);
    R.resize(start + total);
    size_t pos = 0, next = 0;
    while (pos < total) {
        size_t until = std::min(total, pos + 233);
        while (next < evs.size() && static_cast<size_t>(evs[next].t * kSr) <= pos) {
            if (evs[next].vel > 0) e.noteOn(evs[next].note, evs[next].vel);
            else e.noteOff(evs[next].note);
            ++next;
        }
        if (next < evs.size()) until = std::min(until, std::max(pos + 1, static_cast<size_t>(evs[next].t * kSr)));
        e.render(&L[start + pos], &R[start + pos], static_cast<int>(until - pos));
        pos = until;
    }
}

void chord(std::vector<Ev>& ev, double t, double dur, std::initializer_list<int> notes, float vel) {
    for (int n : notes) ev.push_back({t, n, vel});
    for (int n : notes) ev.push_back({t + dur, n, 0});
}

std::vector<Ev> sortEv(std::vector<Ev> ev) {
    std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
    return ev;
}

std::vector<Ev> chordPhrase() {  // Cmaj7 - Am7 - Fmaj7 - G6, 1.1 s each
    std::vector<Ev> ev;
    chord(ev, 0.0, 1.0, {48, 55, 59, 64}, 0.8f);
    chord(ev, 1.1, 1.0, {45, 55, 60, 64}, 0.7f);
    chord(ev, 2.2, 1.0, {41, 57, 60, 64}, 0.75f);
    chord(ev, 3.3, 1.2, {43, 55, 59, 64, 67}, 0.85f);
    return sortEv(ev);
}

std::vector<Ev> keysPhrase() {  // rhythmic stabs + arpeggio
    std::vector<Ev> ev;
    const double s = 0.18;
    for (int i = 0; i < 8; ++i) chord(ev, i * s * 2, s, {57, 60, 64}, i % 2 ? 0.6f : 0.9f);
    const int arp[] = {57, 60, 64, 69, 72, 69, 64, 60, 57, 60, 64, 69, 76, 72, 69, 64};
    for (int i = 0; i < 16; ++i) chord(ev, 3.0 + i * 0.12, 0.1, {arp[i]}, 0.55f + 0.3f * (i % 4 == 0));
    return sortEv(ev);
}

std::vector<Ev> leadPhrase() {  // legato synthwave line (overlapping notes -> glide)
    std::vector<Ev> ev;
    const int notes[] = {69, 72, 76, 74, 72, 71, 72, 69, 76, 79, 81};
    const double dur[] = {0.4, 0.4, 0.8, 0.2, 0.2, 0.4, 0.4, 0.8, 0.4, 0.4, 1.4};
    double t = 0;
    for (int i = 0; i < 11; ++i) {
        ev.push_back({t, notes[i], 0.85f});
        ev.push_back({t + dur[i] + 0.03, notes[i], 0});  // slight overlap = legato
        t += dur[i];
    }
    return sortEv(ev);
}

std::vector<Ev> bassPhrase() {
    std::vector<Ev> ev;
    const int notes[] = {33, 45, 33, 45, 36, 48, 31, 43};
    for (int bar = 0; bar < 2; ++bar)
        for (int i = 0; i < 8; ++i) chord(ev, (bar * 8 + i) * 0.21, 0.16, {notes[i]}, i % 2 ? 0.7f : 0.95f);
    return sortEv(ev);
}

double aliasingDb(int wave, int note) {
    Engine e;
    e.setSampleRate(kSr);
    applyPatch(e, 0);
    e.setParam(kOsc1Wave, wave);
    e.setParam(kCutoff, 18000);
    e.setParam(kReso, 0);
    e.setParam(kFiltEnvAmt, 0);
    e.setParam(kFiltKeytrack, 0);
    e.setParam(kFiltVel, 0);
    e.setParam(kDrift, 0);
    e.setParam(kChorus, 0);
    e.reset();
    std::vector<float> L, R;
    play(e, {{0.0, note, 1.0f}}, 0.5, L, R);
    const int N = 16384;
    const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    double harm = 0, total = 0;
    for (int k = 1; k < N / 2; ++k) {
        double re = 0, im = 0;
        for (int i = 0; i < N; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1));
            const double s = L[kSr / 10 + i] * w;
            re += s * std::cos(2 * M_PI * double(k) * i / N);
            im -= s * std::sin(2 * M_PI * double(k) * i / N);
        }
        const double p = re * re + im * im, f = double(k) * kSr / N;
        if (f > 20000) continue;  // only what we can hear
        total += p;
        const double h = f / f0;
        if (std::fabs(h - std::round(h)) * f0 < 4.0 * kSr / N) harm += p;
    }
    return 10 * std::log10(harm / std::fmax(total - harm, 1e-30));
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    int failures = 0;

    // ---- every factory patch, joined into one demo file
    std::vector<float> DL, DR;
    std::printf("%-16s %8s %8s %6s\n", "patch", "peak", "rms dB", "ok");
    for (int p = 0; p < kNumFactoryPatches; ++p) {
        Engine e;
        e.setSampleRate(kSr);
        applyPatch(e, p);
        e.reset();
        const std::string name = kFactoryPatches[p].name;
        std::vector<Ev> ev;
        if (name == "Synthwave Lead" || name == "Square Solo") ev = leadPhrase();
        else if (name == "Analog Bass") ev = bassPhrase();
        else if (name == "Bright Keys" || name == "Poly Pluck") ev = keysPhrase();
        else ev = chordPhrase();
        std::vector<float> L, R;
        double len = 0;
        for (auto& x : ev) len = std::max(len, x.t);
        play(e, ev, len + 2.5, L, R);
        float peak = 0; double sum = 0; bool finite = true;
        for (size_t i = 0; i < L.size(); ++i) {
            peak = std::max({peak, std::fabs(L[i]), std::fabs(R[i])});
            sum += L[i] * L[i];
            if (!std::isfinite(L[i]) || !std::isfinite(R[i])) finite = false;
        }
        const bool ok = finite && peak < 1.0f;
        if (!ok) ++failures;
        std::printf("%-16s %8.3f %8.1f %6s\n", name.c_str(), peak, 10 * std::log10(sum / L.size() + 1e-20), ok ? "yes" : "NO");
        DL.insert(DL.end(), L.begin(), L.end());
        DR.insert(DR.end(), R.begin(), R.end());
        DL.insert(DL.end(), kSr / 3, 0.0f);
        DR.insert(DR.end(), kSr / 3, 0.0f);
    }
    writeWav(dir + "/retro80_demo.wav", DL, DR);

    // ---- aliasing (higher = cleaner). Phase 1 measured 23.6 dB on C8 saw.
    std::printf("aliasing, dB below harmonics (audible band):\n");
    std::printf("  saw   C7 %.1f   C8 %.1f\n", aliasingDb(0, 96), aliasingDb(0, 108));
    std::printf("  pulse C7 %.1f   C8 %.1f\n", aliasingDb(1, 96), aliasingDb(1, 108));

    // ---- CPU: 16 notes of the PWM pad (2 osc + sub + chorus) for 10 s
    {
        Engine e;
        e.setSampleRate(kSr);
        applyPatch(e, 2);
        e.setParam(kMixSub, 0.5f);
        e.setParam(kMixNoise, 0.1f);
        e.reset();
        std::vector<Ev> ev;
        for (int n = 0; n < 16; ++n) ev.push_back({0.0, 40 + n * 3, 0.8f});
        std::vector<float> L, R;
        const auto t0 = std::chrono::steady_clock::now();
        play(e, ev, 10.0, L, R);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("CPU: 16 voices, 10 s of audio rendered in %.3f s (%.1f%% of one core)\n", secs, secs * 10.0);
    }
    return failures ? 1 : 0;
}

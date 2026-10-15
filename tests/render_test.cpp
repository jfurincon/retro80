// Offline check of the DSP engine: plays a chord + melody, writes a WAV,
// and reports peak level, NaNs/denormal-free output, and aliasing energy.
#include "dsp/engine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
    FILE* f = std::fopen(path, "wb");
    const uint32_t n = static_cast<uint32_t>(l.size());
    const uint32_t dataBytes = n * 2 * 2;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    for (uint32_t i = 0; i < n; ++i) {
        for (float s : {l[i], r[i]}) {
            const float c = std::fmax(-1.0f, std::fmin(1.0f, s));
            const int16_t v = static_cast<int16_t>(std::lrint(c * 32767.0f));
            std::fwrite(&v, 2, 1, f);
        }
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "render_test.wav";
    const int sr = 48000;
    retro80::Engine eng;
    eng.setSampleRate(sr);
    eng.setGain(1.0f);

    std::vector<float> L, R;
    auto run = [&](double seconds) {
        const int n = static_cast<int>(seconds * sr);
        const size_t start = L.size();
        L.resize(start + n); R.resize(start + n);
        // Render in odd block sizes like a host would.
        int done = 0;
        while (done < n) {
            const int blk = std::min(n - done, 237);
            eng.render(&L[start + done], &R[start + done], blk);
            done += blk;
        }
    };

    // Chord: C minor 7 (keys/pads use case).
    for (int note : {48, 55, 58, 63}) eng.noteOn(note, 0.8f);
    run(1.5);
    for (int note : {48, 55, 58, 63}) eng.noteOff(note);
    run(0.6);
    // Lead line with fast retriggers, then 10 notes at once (voice stealing).
    for (int note : {72, 75, 79, 82, 84, 82, 79, 75}) { eng.noteOn(note, 0.9f); run(0.15); eng.noteOff(note); }
    for (int i = 0; i < 10; ++i) eng.noteOn(60 + i * 2, 0.7f);
    run(1.0);
    eng.allNotesOff();
    run(1.5);
    // High note to check aliasing: C8 (~4186 Hz).
    eng.noteOn(108, 1.0f);
    const size_t hiStart = L.size();
    run(1.0);
    eng.noteOff(108);
    run(1.0);

    float peak = 0; bool bad = false;
    for (float s : L) { peak = std::fmax(peak, std::fabs(s)); if (!std::isfinite(s)) bad = true; }
    std::printf("samples=%zu peak=%.3f (%.1f dBFS) finite=%s tail_end=%.2e\n",
                L.size(), peak, 20 * std::log10(peak), bad ? "NO" : "yes", std::fabs(L.back()));

    // Aliasing check on C8: energy NOT at harmonics of f0 vs total (DFT on 8192 samples).
    const int N = 8192; const double f0 = 440.0 * std::pow(2.0, (108 - 69) / 12.0);
    double harm = 0, total = 0;
    for (int k = 1; k < N / 2; ++k) {
        double re = 0, im = 0;
        for (int i = 0; i < N; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1));
            const double s = L[hiStart + sr / 10 + i] * w;
            re += s * std::cos(2 * M_PI * k * i / N); im -= s * std::sin(2 * M_PI * k * i / N);
        }
        const double p = re * re + im * im, f = static_cast<double>(k) * sr / N;
        total += p;
        const double h = f / f0; if (std::fabs(h - std::round(h)) * f0 < 3.0 * sr / N) harm += p;
    }
    std::printf("C8 aliasing: %.1f dB below harmonics\n", 10 * std::log10(harm / std::fmax(total - harm, 1e-30)));
    writeWav(out, L, R, sr);
    return bad ? 1 : 0;
}

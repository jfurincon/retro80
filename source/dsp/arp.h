// Retro80 arpeggiator: turns held keys into a tempo-synced note sequence.
// It only decides *when* to start/stop which note; the engine plays them.
#pragma once

#include "building_blocks.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace retro80 {

class Arpeggiator {
public:
    enum Mode { Up, Down, UpDown, Random, AsPlayed };

    struct Settings {
        int mode = Up;
        double stepBeats = 0.25;
        int octaves = 1;
        float gate = 0.6f;
        bool latch = false;
    };

    // Callbacks into the engine.
    struct Output {
        virtual void arpNoteOn(int note, float velocity) = 0;
        virtual void arpNoteOff(int note) = 0;
        virtual ~Output() = default;
    };

    void setSettings(const Settings& s) {
        const bool latchOff = settings_.latch && !s.latch;
        settings_ = s;
        rebuild();
        if (latchOff) {  // drop latched notes that are no longer physically held
            int w = 0;
            for (int i = 0; i < count_; ++i) if (physical_[notes_[i]]) { notes_[w] = notes_[i]; vel_[w] = vel_[i]; ++w; }
            count_ = w;
            rebuild();
        }
    }

    bool running() const { return count_ > 0; }

    void press(int note, float velocity) {
        const bool wasPhysicalEmpty = physicalCount_ == 0;
        if (!physical_[note]) { physical_[note] = true; ++physicalCount_; }
        if (settings_.latch && wasPhysicalEmpty) count_ = 0;  // new chord replaces the latched one
        for (int i = 0; i < count_; ++i) if (notes_[i] == note) { vel_[i] = velocity; return; }
        if (count_ < kMaxHeld) { notes_[count_] = note; vel_[count_] = velocity; ++count_; }
        rebuild();
        if (count_ == 1 && !sounding()) startPending_ = true;  // first key: play right away
    }

    void release(int note, Output& out) {
        if (physical_[note]) { physical_[note] = false; --physicalCount_; }
        if (settings_.latch) return;
        int w = 0;
        for (int i = 0; i < count_; ++i) if (notes_[i] != note) { notes_[w] = notes_[i]; vel_[w] = vel_[i]; ++w; }
        count_ = w;
        rebuild();
        if (count_ == 0) stop(out);
    }

    void stop(Output& out) {
        if (playing_ >= 0) { out.arpNoteOff(playing_); playing_ = -1; }
        count_ = 0;
        index_ = -1;
        startPending_ = false;
        rebuild();
    }

    void clearAll(Output& out) {
        std::fill(std::begin(physical_), std::end(physical_), false);
        physicalCount_ = 0;
        stop(out);
    }

    // Called once per sample with the current song position (in beats).
    void tick(double ppq, bool hostPlaying, Output& out) {
        if (count_ == 0) return;
        const double step = settings_.stepBeats;

        if (startPending_) {
            startPending_ = false;
            index_ = -1;
            // Align following steps to the host grid when the song is playing.
            nextStepPpq_ = hostPlaying ? (std::floor(ppq / step + 1e-6) + 1.0) * step : ppq + step;
            trigger(ppq, out);
            return;
        }
        // Host jumped (loop, seek): realign to the grid.
        if (ppq + 1e-9 < nextStepPpq_ - 2.0 * step || ppq > nextStepPpq_ + 2.0 * step)
            nextStepPpq_ = (std::floor(ppq / step) + 1.0) * step;

        if (playing_ >= 0 && settings_.gate < 0.995f && ppq >= offPpq_) {
            out.arpNoteOff(playing_);
            playing_ = -1;
        }
        if (ppq >= nextStepPpq_) {
            nextStepPpq_ += step;
            trigger(ppq, out);
        }
    }

    int currentNote() const { return playing_; }

private:
    static constexpr int kMaxHeld = 32;
    static constexpr int kMaxPattern = kMaxHeld * 4 * 2;

    bool sounding() const { return playing_ >= 0; }

    void trigger(double ppq, Output& out) {
        if (patternLen_ == 0) return;
        if (settings_.mode == Random) index_ = static_cast<int>(rng_.next() % static_cast<uint32_t>(patternLen_));
        else index_ = (index_ + 1) % patternLen_;
        const int note = pattern_[index_];
        const float vel = patternVel_[index_];
        // Release first unless it is the same key held legato at 100% gate.
        if (playing_ >= 0) out.arpNoteOff(playing_);
        out.arpNoteOn(note, vel);
        playing_ = note;
        offPpq_ = ppq + settings_.stepBeats * settings_.gate;
    }

    void rebuild() {
        // Base order: sorted ascending, or as played.
        std::array<int, kMaxHeld> order{};
        for (int i = 0; i < count_; ++i) order[i] = i;
        if (settings_.mode != AsPlayed)
            std::sort(order.begin(), order.begin() + count_, [&](int a, int b) { return notes_[a] < notes_[b]; });

        int n = 0;
        for (int o = 0; o < settings_.octaves; ++o)
            for (int i = 0; i < count_; ++i) {
                const int note = notes_[order[i]] + 12 * o;
                if (note > 127) continue;
                pattern_[n] = note;
                patternVel_[n] = vel_[order[i]];
                ++n;
            }
        if (settings_.mode == Down) {
            std::reverse(pattern_.begin(), pattern_.begin() + n);
            std::reverse(patternVel_.begin(), patternVel_.begin() + n);
        } else if (settings_.mode == UpDown && n > 2) {
            for (int i = n - 2; i >= 1; --i) {  // back down without repeating the ends
                pattern_[n + (n - 2 - i)] = pattern_[i];
                patternVel_[n + (n - 2 - i)] = patternVel_[i];
            }
            n = n + (n - 2);
        }
        patternLen_ = n;
        if (patternLen_ > 0 && index_ >= patternLen_) index_ %= patternLen_;
    }

    Settings settings_;
    std::array<int, kMaxHeld> notes_{};
    std::array<float, kMaxHeld> vel_{};
    int count_ = 0;
    bool physical_[128] = {};
    int physicalCount_ = 0;

    std::array<int, kMaxPattern> pattern_{};
    std::array<float, kMaxPattern> patternVel_{};
    int patternLen_ = 0;
    int index_ = -1;
    int playing_ = -1;
    double nextStepPpq_ = 0.0, offPpq_ = 0.0;
    bool startPending_ = false;
    Rng rng_{0xA5A5F00Du};
};

} // namespace retro80

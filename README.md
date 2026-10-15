# Retro80

An 80s-style polysynth (keys, pads, leads) as a VST3 instrument, built on the Steinberg VST3 SDK.

**Status: Phase 2 (core engine) — v0.2.2**

## Sound engine

- **Oscillators:** OSC 1 (Saw, Pulse, Saw+Pulse, Triangle) with pulse width and LFO PWM; OSC 2 (Saw, Pulse, Triangle, Sine) with semitone and fine tune; square sub one octave down; white noise.
- **Filter:** 24 dB/oct resonant ladder low-pass with saturating feedback (self-oscillates at full resonance), plus a 6 dB high-pass. Envelope (bipolar), LFO, keyboard tracking and velocity modulation.
- **Envelopes:** analog-style ADSR for filter and amp, amp velocity sensitivity.
- **LFO:** Triangle, Sine, Square, Saw Down, Random; Juno-style delayed fade-in; vibrato amount.
- **Voices:** Poly (16 voices), Mono (legato, last-note priority), Unison (2–8 stacked voices, detuned and spread in stereo). Glide, pitch bend range, sustain pedal, mod wheel vibrato.
- **Analog drift:** small per-note and slowly wandering pitch/cutoff differences per voice.
- **Chorus:** Juno-60 style BBD chorus, modes I, II and I+II.
- Voices run at 2x oversampling with band-limited oscillators to keep high notes clean.

## Factory patches

Init, Juno Brass, Dream Pad, Bright Keys, Strings 1983, Synthwave Lead, Square Solo, Poly Pluck, Analog Bass.
Use the arrows at the top right of the panel.

## Panel tips

- **Size:** pick 100–200% from the zoom menu at the top (default 125%). Your choice is remembered for new instances (`%APPDATA%\Retro80\settings.txt`).
- **Patches:** arrows to step, or click the patch name for the full list.
- **Wave / Mode / Chorus:** click to open a dropdown (wave menus show the shapes; the Pulse icon follows the PW knob).
- Drag knobs up/down; hold **Shift** for fine control.
- **Double-click** (or Ctrl+click) a knob to reset it.
- Mouse wheel nudges the control under the cursor.
- Mouse wheel over a selector steps through its options.

## Install in FL Studio (Windows)

1. Unzip and copy the `Retro80.vst3` folder into `C:\Program Files\Common Files\VST3\` (replace the old one; close FL first).
2. In FL Studio: **Options → Manage plugins → Find installed plugins** (rescan).
3. Add it from the Channel Rack (**+** → Retro80).

## Build

Windows build from Linux (needs `mingw-w64`, `cmake`, `ninja`):

```
git submodule update --init --recursive
./scripts/build-windows.sh
```

Output: `build-win/VST3/Release/Retro80.vst3`.

## Layout

- `source/params.h` – every parameter (ranges, defaults, display text); IDs are saved in projects, never renumber
- `source/presets.h` – factory patches
- `source/dsp/` – the synth engine (plain C++, no SDK dependencies)
- `source/processor.*` – audio/MIDI processing (VST3 component)
- `source/controller.*` – parameters, MIDI mapping, program list (VST3 edit controller)
- `source/editor/` – Windows control panel (Win32 + GDI+)
- `tests/render_test.cpp` – offline renders, aliasing and CPU checks
- `tests/editor_snapshot.cpp` – opens the panel in a test host and saves screenshots

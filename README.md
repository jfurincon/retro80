# Retro80

An 80s-style polysynth (keys, pads, leads) as a VST3 instrument, built on the Steinberg VST3 SDK.

**Status: Phase 1 (pipeline test).** 8-voice polyphonic band-limited saw, simple envelope, Volume knob. No custom interface yet.

## Install in FL Studio (Windows)

1. Unzip and copy the `Retro80.vst3` folder into `C:\Program Files\Common Files\VST3\`.
2. In FL Studio: **Options → Manage plugins → Find installed plugins** (rescan).
3. Add it from the Channel Rack (**+** → Retro80), or from the Plugin Database under Generators.

## Build

Windows build from Linux (needs `mingw-w64`, `cmake`, `ninja`):

```
git submodule update --init --recursive
./scripts/build-windows.sh
```

Output: `build-win/VST3/Release/Retro80.vst3`.

## Layout

- `source/dsp/` – the synth engine (plain C++, no SDK dependencies)
- `source/processor.*` – audio/MIDI processing (VST3 component)
- `source/controller.*` – parameters (VST3 edit controller)
- `tests/render_test.cpp` – offline render of the engine to a WAV

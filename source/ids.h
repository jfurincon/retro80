#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

#define RETRO80_VERSION_STR "0.1.0"
#define RETRO80_NAME "Retro80"
#define RETRO80_VENDOR "Retro80"
#define RETRO80_URL ""
#define RETRO80_EMAIL ""

namespace retro80 {

static const Steinberg::FUID kProcessorUID(0xF3BB2EE5, 0x3C324091, 0x87FECBCA, 0x75109279);
static const Steinberg::FUID kControllerUID(0x5CCA1E27, 0x86AE46E6, 0xAA5DB56C, 0xA3702535);

enum ParamIds : Steinberg::Vst::ParamID {
    kParamVolume = 0,
};

// Volume knob: normalized 0..1 -> linear gain = 2 * n^2
// (0 -> silence, 0.7071 -> 0 dB, 1.0 -> +6 dB).
inline float volumeToGain(double normalized) {
    return static_cast<float>(2.0 * normalized * normalized);
}
constexpr double kVolumeDefault = 0.70710678;

} // namespace retro80

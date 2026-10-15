#include "controller.h"
#include "ids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace retro80 {

namespace {

// Shows the volume knob in dB instead of 0..1.
class VolumeParameter : public Parameter {
public:
    VolumeParameter()
        : Parameter(STR16("Volume"), kParamVolume, STR16("dB"), kVolumeDefault, 0,
                    ParameterInfo::kCanAutomate) {}

    void toString(ParamValue normalized, String128 string) const override {
        const float gain = volumeToGain(normalized);
        char text[32];
        if (gain <= 1.0e-5f)
            std::snprintf(text, sizeof(text), "-inf");
        else
            std::snprintf(text, sizeof(text), "%.1f", 20.0 * std::log10(gain));
        UString(string, 128).fromAscii(text);
    }

    bool fromString(const TChar* string, ParamValue& normalized) const override {
        UString wrapper(const_cast<TChar*>(string), tstrlen(string));
        double db = 0;
        if (!wrapper.scanFloat(db)) return false;
        const double gain = std::pow(10.0, db / 20.0);
        normalized = std::clamp(std::sqrt(gain / 2.0), 0.0, 1.0);
        return true;
    }
};

} // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk) return result;
    parameters.addParameter(new VolumeParameter);
    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer streamer(state, kLittleEndian);
    double v = 0;
    if (!streamer.readDouble(v)) return kResultFalse;
    setParamNormalized(kParamVolume, std::clamp(v, 0.0, 1.0));
    return kResultOk;
}

} // namespace retro80

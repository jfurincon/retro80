#include "processor.h"
#include "presets.h"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace retro80 {

Processor::Processor() {
    setControllerClass(kControllerUID);
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;

    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    addEventInput(STR16("MIDI In"), 16);
    applyPatchToEngine();
    return kResultOk;
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                 SpeakerArrangement* outputs, int32 numOuts) {
    // Instrument: no audio inputs, one stereo output.
    if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo)
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    return kResultFalse;
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize) {
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    engine_.setSampleRate(setup.sampleRate);
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    if (state) engine_.reset();
    return AudioEffect::setActive(state);
}

void Processor::applyPatchToEngine() {
    for (int i = 0; i < kNumPatchParams; ++i) engine_.setParam(i, toPlain(kParamDefs[i], patch_[i]));
}

void Processor::handleParameterChanges(IParameterChanges* changes) {
    if (!changes) return;
    const int32 count = changes->getParameterCount();
    for (int32 i = 0; i < count; ++i) {
        IParamValueQueue* queue = changes->getParameterData(i);
        if (!queue || queue->getPointCount() <= 0) continue;
        int32 offset = 0;
        ParamValue value = 0;
        if (queue->getPoint(queue->getPointCount() - 1, offset, value) != kResultTrue) continue;

        const ParamID id = queue->getParameterId();
        if (id < static_cast<ParamID>(kNumPatchParams)) {
            patch_[id] = value;
            engine_.setParam(id, toPlain(kParamDefs[id], value));
        } else if (id == kPitchBend) {
            engine_.setPitchBend(static_cast<float>(value * 2.0 - 1.0));
        } else if (id == kModWheel) {
            engine_.setModWheel(static_cast<float>(value));
        } else if (id == kSustainPedal) {
            engine_.setSustainPedal(value >= 0.5);
        } else if (id == kProgram) {
            const int index = static_cast<int>(std::lround(value * (kNumFactoryPatches - 1)));
            const auto p = patchNormalized(index);
            std::copy(p.begin(), p.end(), patch_.begin());
            applyPatchToEngine();
        }
    }
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    handleParameterChanges(data.inputParameterChanges);

    if (data.numOutputs < 1 || data.numSamples <= 0) return kResultOk;  // parameter flush

    AudioBusBuffers& out = data.outputs[0];
    if (out.numChannels < 2) return kResultOk;
    float* left = out.channelBuffers32[0];
    float* right = out.channelBuffers32[1];

    // Render in slices between events so notes start on the exact sample.
    int32 rendered = 0;
    IEventList* events = data.inputEvents;
    const int32 numEvents = events ? events->getEventCount() : 0;

    for (int32 e = 0; e < numEvents; ++e) {
        Event ev{};
        if (events->getEvent(e, ev) != kResultOk) continue;

        const int32 at = std::clamp(ev.sampleOffset, rendered, data.numSamples);
        if (at > rendered) {
            engine_.render(left + rendered, right + rendered, at - rendered);
            rendered = at;
        }

        switch (ev.type) {
            case Event::kNoteOnEvent:
                if (ev.noteOn.velocity <= 0.0f)
                    engine_.noteOff(ev.noteOn.pitch);
                else
                    engine_.noteOn(ev.noteOn.pitch, ev.noteOn.velocity);
                break;
            case Event::kNoteOffEvent:
                engine_.noteOff(ev.noteOff.pitch);
                break;
            default:
                break;
        }
    }

    if (rendered < data.numSamples)
        engine_.render(left + rendered, right + rendered, data.numSamples - rendered);

    out.silenceFlags = 0;
    return kResultOk;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
    PatchArray p;
    if (!readState(state, p)) return kResultFalse;
    patch_ = p;
    applyPatchToEngine();
    return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
    return writeState(state, patch_) ? kResultOk : kResultFalse;
}

} // namespace retro80

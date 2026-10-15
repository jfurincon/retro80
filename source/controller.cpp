#include "controller.h"
#include "ids.h"
#include "presets.h"
#include "state.h"

#if defined(_WIN32)
#include <windows.h>
#include "editor/editor.h"
#endif

#include "pluginterfaces/base/ustring.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace retro80 {

namespace {

void toString128(const char* ascii, String128 out) {
    UString(out, 128).fromAscii(ascii);
}

// One parameter class for the whole table: ranges, steps and display text all
// come from params.h, so the host, the editor and the DSP always agree.
class TableParameter : public Parameter {
public:
    explicit TableParameter(const ParamDef& d) : def_(d) {
        String128 title, shortTitle;
        toString128(d.name, title);
        toString128(d.shortName, shortTitle);
        UString(info.title, 128).assign(title);
        UString(info.shortTitle, 128).assign(shortTitle);
        info.units[0] = 0;
        info.id = d.id;
        info.stepCount = stepCount(d);
        info.defaultNormalizedValue = defaultNormalized(d);
        info.unitId = kRootUnitId;
        info.flags = ParameterInfo::kCanAutomate | (d.scale == Scale::List ? ParameterInfo::kIsList : 0);
        setNormalized(info.defaultNormalizedValue);
    }

    void toString(ParamValue normalized, String128 string) const override {
        char text[64];
        formatValue(def_, retro80::toPlain(def_, normalized), text, sizeof(text));
        toString128(text, string);
    }

    bool fromString(const TChar* string, ParamValue& normalized) const override {
        char text[64] = {};
        UString(const_cast<TChar*>(string), tstrlen(string)).toAscii(text, sizeof(text));
        double plain = 0;
        if (!parseValue(def_, text, plain)) return false;
        normalized = retro80::toNormalized(def_, plain);
        return true;
    }

    ParamValue toPlain(ParamValue normalized) const override { return retro80::toPlain(def_, normalized); }
    ParamValue toNormalized(ParamValue plain) const override { return retro80::toNormalized(def_, plain); }

private:
    const ParamDef& def_;
};

} // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk) return result;

    // Factory patches as a VST3 program list on the root unit.
    addUnit(new Unit(STR16("Root"), kRootUnitId, kNoParentUnitId, kProgram));
    auto* programs = new ProgramList(STR16("Factory"), kProgram, kRootUnitId);
    for (int i = 0; i < kNumFactoryPatches; ++i) {
        String128 name;
        toString128(kFactoryPatches[i].name, name);
        programs->addProgram(name);
    }
    addProgramList(programs);
    parameters.addParameter(programs->getParameter());

    for (int i = 0; i < kNumPatchParams; ++i) parameters.addParameter(new TableParameter(kParamDefs[i]));

    // MIDI-mapped performance controls (hidden from the automation list).
    parameters.addParameter(STR16("Pitch Bend"), nullptr, 0, 0.5,
                            ParameterInfo::kCanAutomate | ParameterInfo::kIsHidden, kPitchBend);
    parameters.addParameter(STR16("Mod Wheel"), nullptr, 0, 0.0,
                            ParameterInfo::kCanAutomate | ParameterInfo::kIsHidden, kModWheel);
    parameters.addParameter(STR16("Sustain Pedal"), nullptr, 1, 0.0,
                            ParameterInfo::kCanAutomate | ParameterInfo::kIsHidden, kSustainPedal);
    parameters.addParameter(STR16("Aftertouch"), nullptr, 0, 0.0,
                            ParameterInfo::kCanAutomate | ParameterInfo::kIsHidden, retro80::kAftertouch);
    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
    PatchArray p;
    if (!readState(state, p)) return kResultFalse;
    for (int i = 0; i < kNumPatchParams; ++i) setParamNormalized(i, p[i]);
    return kResultOk;
}

void Controller::loadFactoryPatch(int index) {
    const auto p = patchNormalized(index);
    loadingPatch_ = true;
    for (int i = 0; i < kNumPatchParams; ++i) setParamNormalized(i, p[i]);
    loadingPatch_ = false;
    if (componentHandler) componentHandler->restartComponent(kParamValuesChanged);
}

tresult PLUGIN_API Controller::setParamNormalized(ParamID tag, ParamValue value) {
    const tresult r = EditControllerEx1::setParamNormalized(tag, value);
    if (tag == kProgram && !loadingPatch_)
        loadFactoryPatch(static_cast<int>(std::lround(value * (kNumFactoryPatches - 1))));
#if defined(_WIN32)
    if (editor_) editor_->paramChanged(tag);
#endif
    return r;
}

void Controller::editorPerformEdit(ParamID id, ParamValue v) {
    setParamNormalized(id, v);
    performEdit(id, v);
}

tresult PLUGIN_API Controller::notify(IMessage* message) {
    if (dataExchangeReceiver_.onMessage(message)) return kResultTrue;
    return EditControllerEx1::notify(message);
}

static uint64 nowMs() {
#if defined(_WIN32)
    return GetTickCount64();
#else
    return 0;
#endif
}

void PLUGIN_API Controller::onDataExchangeBlocksReceived(DataExchangeUserContextID, uint32 numBlocks,
                                                         DataExchangeBlock* blocks, TBool) {
    if (numBlocks == 0) return;
    const DataExchangeBlock& b = blocks[numBlocks - 1];  // only the newest matters
    if (b.size < sizeof(ScopeBlock)) return;
    const auto* sb = static_cast<const ScopeBlock*>(b.data);
    if (sb->magic != ScopeBlock::kMagic) return;
    {
        std::lock_guard<std::mutex> lock(scopeMutex_);
        scope_ = *sb;
        hasScope_ = true;
        scopeTimeMs_ = nowMs();
    }
#if defined(_WIN32)
    if (editor_) editor_->scopeUpdated();
#endif
}

bool Controller::latestScope(ScopeBlock& out, uint32& ageMs) const {
    std::lock_guard<std::mutex> lock(scopeMutex_);
    if (!hasScope_) return false;
    out = scope_;
    ageMs = static_cast<uint32>(nowMs() - scopeTimeMs_);
    return true;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
#if defined(_WIN32)
    if (name && std::strcmp(name, ViewType::kEditor) == 0) {
        auto* view = new Editor(this);
        editor_ = view;
        return view;
    }
#else
    (void)name;
#endif
    return nullptr;
}

tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 busIndex, int16 /*channel*/,
                                                           CtrlNumber midiControllerNumber, ParamID& id) {
    if (busIndex != 0) return kResultFalse;
    switch (midiControllerNumber) {
        case Steinberg::Vst::kPitchBend: id = retro80::kPitchBend; return kResultTrue;
        case kCtrlModWheel: id = retro80::kModWheel; return kResultTrue;
        case kCtrlSustainOnOff: id = retro80::kSustainPedal; return kResultTrue;
        case Steinberg::Vst::kAfterTouch: id = retro80::kAftertouch; return kResultTrue;
        default: return kResultFalse;
    }
}

} // namespace retro80

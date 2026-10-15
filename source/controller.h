#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

namespace retro80 {

class Editor;

class Controller : public Steinberg::Vst::EditControllerEx1, public Steinberg::Vst::IMidiMapping {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new Controller);
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                                     Steinberg::Vst::ParamValue value) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override;

    // IMidiMapping: pitch bend, mod wheel and sustain pedal arrive as parameters.
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment(Steinberg::int32 busIndex,
                                                              Steinberg::int16 channel,
                                                              Steinberg::Vst::CtrlNumber midiControllerNumber,
                                                              Steinberg::Vst::ParamID& id) override;

    // Called by the editor while the user turns a knob.
    void editorBeginEdit(Steinberg::Vst::ParamID id) { beginEdit(id); }
    void editorPerformEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue v);
    void editorEndEdit(Steinberg::Vst::ParamID id) { endEdit(id); }
    void editorClosed(Editor* e) { if (editor_ == e) editor_ = nullptr; }

    OBJ_METHODS(Controller, EditControllerEx1)
    DEFINE_INTERFACES
        DEF_INTERFACE(IMidiMapping)
    END_DEFINE_INTERFACES(EditControllerEx1)
    REFCOUNT_METHODS(EditControllerEx1)

private:
    void loadFactoryPatch(int index);

    Editor* editor_ = nullptr;
    bool loadingPatch_ = false;
};

} // namespace retro80

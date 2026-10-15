#pragma once

#include "public.sdk/source/vst/utility/dataexchange.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstdataexchange.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include "scope.h"

#include <mutex>

namespace retro80 {

class Editor;

class Controller : public Steinberg::Vst::EditControllerEx1,
                   public Steinberg::Vst::IMidiMapping,
                   public Steinberg::Vst::IDataExchangeReceiver {
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

    // Scope blocks from the processor.
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) override;
    void PLUGIN_API queueOpened(Steinberg::Vst::DataExchangeUserContextID, Steinberg::uint32,
                                Steinberg::TBool& dispatchOnBackgroundThread) override {
        dispatchOnBackgroundThread = false;
    }
    void PLUGIN_API queueClosed(Steinberg::Vst::DataExchangeUserContextID) override {}
    void PLUGIN_API onDataExchangeBlocksReceived(Steinberg::Vst::DataExchangeUserContextID, Steinberg::uint32 numBlocks,
                                                 Steinberg::Vst::DataExchangeBlock* blocks,
                                                 Steinberg::TBool onBackgroundThread) override;

    // Latest scope data (UI thread). Returns false if nothing arrived yet.
    bool latestScope(ScopeBlock& out, Steinberg::uint32& ageMs) const;

    // Panel page, kept while the plugin is loaded.
    int editorPage = 0;

    // Called by the editor while the user turns a knob.
    void editorBeginEdit(Steinberg::Vst::ParamID id) { beginEdit(id); }
    void editorPerformEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue v);
    void editorEndEdit(Steinberg::Vst::ParamID id) { endEdit(id); }
    void editorClosed(Editor* e) { if (editor_ == e) editor_ = nullptr; }

    OBJ_METHODS(Controller, EditControllerEx1)
    DEFINE_INTERFACES
        DEF_INTERFACE(IMidiMapping)
        DEF_INTERFACE(IDataExchangeReceiver)
    END_DEFINE_INTERFACES(EditControllerEx1)
    REFCOUNT_METHODS(EditControllerEx1)

private:
    void loadFactoryPatch(int index);

    Editor* editor_ = nullptr;
    Steinberg::Vst::DataExchangeReceiverHandler dataExchangeReceiver_{this};
    mutable std::mutex scopeMutex_;
    ScopeBlock scope_{};
    bool hasScope_ = false;
    Steinberg::uint64 scopeTimeMs_ = 0;
    bool loadingPatch_ = false;
};

} // namespace retro80

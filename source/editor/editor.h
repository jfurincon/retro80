// Retro80 control panel (Windows): a plain Win32 child window drawn with
// GDI+. Every knob/selector is generated from the parameter table.
#pragma once

#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <memory>
#include <vector>

namespace retro80 {

class Controller;

class Editor : public Steinberg::CPluginView {
public:
    explicit Editor(Controller* controller);
    ~Editor() override;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultFalse; }
    void attachedToParent() override;
    void removedFromParent() override;

    // Controller -> editor: a value changed (automation, preset, host).
    void paramChanged(Steinberg::Vst::ParamID id);

    struct Impl;  // Win32/GDI+ details live in editor.cpp

private:
    Controller* controller_;
    std::unique_ptr<Impl> impl_;
};

} // namespace retro80

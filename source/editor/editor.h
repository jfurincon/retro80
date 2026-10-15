// Retro80 control panel (Windows): a plain Win32 child window drawn with
// GDI+. Every knob/selector is generated from the parameter table.
// The whole panel is drawn in "logical" units and scaled (zoom x DPI).
#pragma once

#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <memory>

namespace retro80 {

class Controller;

class Editor : public Steinberg::CPluginView, public Steinberg::IPlugViewContentScaleSupport {
public:
    explicit Editor(Controller* controller);
    ~Editor() override;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
    Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override;
    void attachedToParent() override;
    void removedFromParent() override;

    // Controller -> editor: a value changed (automation, preset, host).
    void paramChanged(Steinberg::Vst::ParamID id);

    // Impl -> editor: the zoom/DPI changed, ask the host for a new window size.
    void requestSize(int width, int height);

    OBJ_METHODS(Editor, CPluginView)
    DEFINE_INTERFACES
        DEF_INTERFACE(IPlugViewContentScaleSupport)
    END_DEFINE_INTERFACES(CPluginView)
    REFCOUNT_METHODS(CPluginView)

    struct Impl;  // Win32/GDI+ details live in editor.cpp

private:
    Controller* controller_;
    std::unique_ptr<Impl> impl_;
};

} // namespace retro80

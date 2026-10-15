#include "controller.h"
#include "ids.h"
#include "processor.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(RETRO80_VENDOR, RETRO80_URL, RETRO80_EMAIL)

    DEF_CLASS2(INLINE_UID_FROM_FUID(retro80::kProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               RETRO80_NAME,
               Vst::kDistributable,
               PlugType::kInstrumentSynth,
               RETRO80_VERSION_STR,
               kVstVersionString,
               retro80::Processor::createInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(retro80::kControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               RETRO80_NAME " Controller",
               0,
               "",
               RETRO80_VERSION_STR,
               kVstVersionString,
               retro80::Controller::createInstance)

END_FACTORY

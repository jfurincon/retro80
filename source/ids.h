#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

#define RETRO80_VERSION_STR "0.3.0"
#define RETRO80_VERSION_WSTR L"0.3.0"
#define RETRO80_NAME "Retro80"
#define RETRO80_VENDOR "Retro80"
#define RETRO80_URL ""
#define RETRO80_EMAIL ""

namespace retro80 {

static const Steinberg::FUID kProcessorUID(0xF3BB2EE5, 0x3C324091, 0x87FECBCA, 0x75109279);
static const Steinberg::FUID kControllerUID(0x5CCA1E27, 0x86AE46E6, 0xAA5DB56C, 0xA3702535);

} // namespace retro80

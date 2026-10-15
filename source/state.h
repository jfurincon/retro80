// Saving/loading the patch (used by both processor and controller so the two
// always agree). Format v2: magic, version, count, then (id, normalized) pairs.
// Loading by ID keeps old projects working as parameters get added.
#pragma once

#include "params.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"

#include <array>

namespace retro80 {

constexpr Steinberg::int32 kStateMagic = 0x52383050;  // 'R80P'
constexpr Steinberg::int32 kStateVersion = 2;

using PatchArray = std::array<double, kNumPatchParams>;

inline PatchArray defaultPatch() {
    PatchArray p{};
    for (int i = 0; i < kNumPatchParams; ++i) p[i] = defaultNormalized(kParamDefs[i]);
    return p;
}

inline bool writeState(Steinberg::IBStream* stream, const PatchArray& p) {
    if (!stream) return false;
    Steinberg::IBStreamer s(stream, kLittleEndian);
    s.writeInt32(kStateMagic);
    s.writeInt32(kStateVersion);
    s.writeInt32(kNumPatchParams);
    for (int i = 0; i < kNumPatchParams; ++i) {
        s.writeInt32(i);
        s.writeDouble(p[i]);
    }
    return true;
}

// Unknown IDs are skipped; missing IDs keep their defaults.
inline bool readState(Steinberg::IBStream* stream, PatchArray& p) {
    if (!stream) return false;
    p = defaultPatch();
    Steinberg::IBStreamer s(stream, kLittleEndian);
    Steinberg::int64 start = 0;
    stream->tell(&start);

    Steinberg::int32 magic = 0;
    if (!s.readInt32(magic)) return false;
    if (magic != kStateMagic) {
        // Phase 1 (v0.1.0) stored a single double: the volume.
        stream->seek(start, Steinberg::IBStream::kIBSeekSet, nullptr);
        double volume = 0;
        if (!s.readDouble(volume)) return false;
        p[kVolume] = std::clamp(volume, 0.0, 1.0);
        return true;
    }
    Steinberg::int32 version = 0, count = 0;
    if (!s.readInt32(version) || !s.readInt32(count)) return false;
    for (Steinberg::int32 i = 0; i < count; ++i) {
        Steinberg::int32 id = 0;
        double value = 0;
        if (!s.readInt32(id) || !s.readDouble(value)) return false;
        if (id >= 0 && id < kNumPatchParams) p[id] = std::clamp(value, 0.0, 1.0);
    }
    return true;
}

} // namespace retro80

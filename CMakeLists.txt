cmake_minimum_required(VERSION 3.25)

# Only build what we need from the SDK.
option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)
option(SMTG_ENABLE_VSTGUI_SUPPORT "Enable VSTGUI Support" OFF)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/vst3sdk")
if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "VST3 SDK missing. Run: git submodule update --init --recursive")
endif()

project(Retro80 VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    # MinGW has no std::aligned_alloc; use _aligned_malloc/_aligned_free like MSVC.
    set(_aa "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/alignedalloc.h")
    file(READ "${_aa}" _aa_text)
    string(REPLACE "#elif defined(_MSC_VER)\n\tdata = _aligned_malloc"
                   "#elif defined(_MSC_VER) || defined(__MINGW32__)\n\tdata = _aligned_malloc"
                   _aa_text "${_aa_text}")
    string(REPLACE "#if defined(_MSC_VER)\n\t\t_aligned_free"
                   "#if defined(_MSC_VER) || defined(__MINGW32__)\n\t\t_aligned_free"
                   _aa_text "${_aa_text}")
    file(WRITE "${_aa}" "${_aa_text}")
endif()

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(Retro80
    source/ids.h
    source/params.h
    source/presets.h
    source/state.h
    source/dsp/building_blocks.h
    source/dsp/engine.h
    source/dsp/fx.h
    source/dsp/arp.h
    source/scope.h
    source/processor.h
    source/processor.cpp
    source/controller.h
    source/controller.cpp
    source/entry.cpp
)
target_link_libraries(Retro80 PRIVATE sdk)

if(WIN32)
    target_sources(Retro80 PRIVATE source/editor/editor.h source/editor/editor.cpp)
    target_link_libraries(Retro80 PRIVATE gdiplus user32 gdi32 shell32)
endif()

if(MINGW)
    # Self-contained DLL: no MinGW runtime DLLs needed on the user's PC.
    target_link_options(Retro80 PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Offline test: renders a WAV through the DSP engine (host build only).
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(render_test tests/render_test.cpp)
    target_include_directories(render_test PRIVATE source)
endif()

# Windows-only test host that opens the editor and saves screenshots.
if(WIN32 AND RETRO80_BUILD_TESTS)
    add_executable(editor_snapshot tests/editor_snapshot.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/vstinitiids.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/hostclasses.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/eventlist.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/processdata.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/stringconvert.cpp
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/commonstringconvert.cpp)
    target_include_directories(editor_snapshot PRIVATE ${vst3sdk_SOURCE_DIR})
    target_link_libraries(editor_snapshot PRIVATE base pluginterfaces user32 gdi32 ole32)
    target_link_options(editor_snapshot PRIVATE -municode -static)
endif()

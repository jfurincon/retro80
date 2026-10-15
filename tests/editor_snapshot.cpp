// Windows test host: loads Retro80.vst3 like a DAW would (processor + controller,
// connected), plays notes through the real audio processor, opens the editor,
// simulates clicks and saves screenshots (BMP) after each step.
// usage: editor_snapshot.exe <path\to\Retro80.vst3 dll> <output prefix>
#include <windows.h>

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/processdata.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;

typedef IPluginFactory*(PLUGIN_API* GetFactoryProc)();
typedef bool(PLUGIN_API* InitDllProc)();

static void pumpOnce() {
    MSG msg;
    for (int n = 0; n < 200 && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE); ++n) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static bool saveBmp(HWND hwnd, int w, int h, const std::wstring& path) {
    HDC src = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(src);
    HBITMAP bmp = CreateCompatibleBitmap(src, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, src, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    const int stride = ((w * 3 + 3) / 4) * 4;
    std::string pixels(stride * h, '\0');
    GetDIBits(mem, bmp, 0, h, &pixels[0], reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    BITMAPFILEHEADER bf{};
    bf.bfType = 0x4D42;
    bf.bfOffBits = sizeof(bf) + sizeof(bi);
    bf.bfSize = bf.bfOffBits + static_cast<DWORD>(pixels.size());
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) return false;
    fwrite(&bf, sizeof(bf), 1, f);
    fwrite(&bi, sizeof(bi), 1, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, src);
    return true;
}

int wmain(int argc, wchar_t** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 3) return 2;
    HMODULE lib = LoadLibraryW(argv[1]);
    if (!lib) { printf("LoadLibrary failed %lu\n", GetLastError()); return 1; }
    if (auto init = reinterpret_cast<InitDllProc>(GetProcAddress(lib, "InitDll"))) init();
    IPluginFactory* factory = reinterpret_cast<GetFactoryProc>(GetProcAddress(lib, "GetPluginFactory"))();

    IComponent* comp = nullptr;
    IEditController* ctrl = nullptr;
    for (int32 i = 0; i < factory->countClasses(); ++i) {
        PClassInfo ci;
        factory->getClassInfo(i, &ci);
        if (std::strcmp(ci.category, kVstAudioEffectClass) == 0)
            factory->createInstance(ci.cid, IComponent::iid, reinterpret_cast<void**>(&comp));
        if (std::strcmp(ci.category, kVstComponentControllerClass) == 0)
            factory->createInstance(ci.cid, IEditController::iid, reinterpret_cast<void**>(&ctrl));
    }
    if (!comp || !ctrl) { printf("missing classes\n"); return 1; }

    auto* host = new HostApplication;
    comp->initialize(host);
    ctrl->initialize(host);
    IConnectionPoint *cpComp = nullptr, *cpCtrl = nullptr;
    comp->queryInterface(IConnectionPoint::iid, reinterpret_cast<void**>(&cpComp));
    ctrl->queryInterface(IConnectionPoint::iid, reinterpret_cast<void**>(&cpCtrl));
    cpComp->connect(cpCtrl);
    cpCtrl->connect(cpComp);

    IAudioProcessor* proc = nullptr;
    comp->queryInterface(IAudioProcessor::iid, reinterpret_cast<void**>(&proc));
    ProcessSetup setup{kRealtime, kSample32, 512, 48000.0};
    proc->setupProcessing(setup);
    comp->activateBus(kAudio, kOutput, 0, true);
    comp->activateBus(kEvent, kInput, 0, true);
    comp->setActive(true);
    proc->setProcessing(true);

    HostProcessData pd;
    pd.prepare(*comp, 512, kSample32);
    pd.numSamples = 512;
    EventList events;
    pd.inputEvents = &events;
    ProcessContext ctx{};
    ctx.sampleRate = 48000;
    ctx.tempo = 120;
    ctx.state = ProcessContext::kTempoValid;
    pd.processContext = &ctx;

    auto note = [&](int pitch, bool on) {
        Event e{};
        e.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
        if (on) { e.noteOn.pitch = static_cast<int16>(pitch); e.noteOn.velocity = 0.9f; e.noteOn.noteId = -1; }
        else { e.noteOff.pitch = static_cast<int16>(pitch); e.noteOff.velocity = 0; e.noteOff.noteId = -1; }
        events.addEvent(e);
    };
    // Run audio in roughly real time while the UI thread pumps messages.
    auto run = [&](int ms) {
        const DWORD end = GetTickCount() + ms;
        while (GetTickCount() < end) {
            proc->process(pd);
            events.clear();
            pumpOnce();
            Sleep(10);
        }
    };

    IPlugView* view = ctrl->createView(ViewType::kEditor);
    ViewRect r;
    view->getSize(&r);
    const int w = r.getWidth(), h = r.getHeight();
    const double sc = w / 1136.0;
    printf("editor %dx%d (%.0f%%)\n", w, h, sc * 100);
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SnapshotHost";
    RegisterClassW(&wc);
    HWND top = CreateWindowExW(0, L"SnapshotHost", L"host", WS_POPUP | WS_VISIBLE, 0, 0, w, h, nullptr, nullptr,
                               wc.hInstance, nullptr);
    view->attached(top, kPlatformTypeHWND);
    HWND child = GetWindow(top, GW_CHILD);
    std::wstring prefix = argv[2];
    auto click = [&](double lx, double ly) {
        const LPARAM p = MAKELPARAM(int(lx * sc), int(ly * sc));
        SendMessageW(child, WM_MOUSEMOVE, 0, p);
        SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, p);
        SendMessageW(child, WM_LBUTTONUP, 0, p);
        run(200);
    };

    run(700);
    saveBmp(top, w, h, prefix + L"_1_synth_idle.bmp");

    note(45, true);  // A2
    run(700);
    saveBmp(top, w, h, prefix + L"_2_synth_live.bmp");
    note(45, false);
    run(400);

    // Click the OSC 2 title: section switches off and dims.
    click(300, 84);
    printf("osc2 on after title click = %.0f (expect 0)\n", ctrl->getParamNormalized(78));
    saveBmp(top, w, h, prefix + L"_3_osc2_off.bmp");
    click(300, 84);
    printf("osc2 on after second click = %.0f (expect 1)\n", ctrl->getParamNormalized(78));

    // Patch browser: open, hover "Toy Box" (Keys column, 7th), screenshot, pick it.
    printf("step: open browser\n");
    click(980, 29);
    printf("step: browser open, hovering\n");
    SendMessageW(child, WM_MOUSEMOVE, 0, MAKELPARAM(int(86 * sc), int(268 * sc)));
    run(150);
    printf("step: screenshot\n");
    saveBmp(top, w, h, prefix + L"_4_patch_browser.bmp");
    printf("step: pick\n");
    click(86, 268);
    printf("program = %.4f (Toy Box = %.4f)\n", ctrl->getParamNormalized(102), 7.0 / 42.0);

    click(379, 29);  // MOD / ARP / FX tab
    saveBmp(top, w, h, prefix + L"_5_mod_page.bmp");

    click(214 + 43, 29);  // back to SYNTH
    view->removed();
    view->release();
    proc->setProcessing(false);
    comp->setActive(false);
    cpComp->disconnect(cpCtrl);
    cpCtrl->disconnect(cpComp);
    cpComp->release();
    cpCtrl->release();
    proc->release();
    pd.unprepare();
    ctrl->terminate();
    comp->terminate();
    ctrl->release();
    comp->release();
    DestroyWindow(top);
    printf("done\n");
    return 0;
}

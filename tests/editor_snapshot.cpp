// Windows test host: loads Retro80.vst3, opens its editor in a window,
// simulates a few clicks/drags and saves screenshots (BMP) after each step.
// usage: editor_snapshot.exe <path\to\Retro80.vst3 dll> <output prefix>
#include <windows.h>

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;

typedef IPluginFactory*(PLUGIN_API* GetFactoryProc)();
typedef bool(PLUGIN_API* InitDllProc)();

static void pump(int ms) {
    const DWORD end = GetTickCount() + ms;
    while (GetTickCount() < end) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
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
    if (argc < 3) return 2;
    HMODULE lib = LoadLibraryW(argv[1]);
    if (!lib) { printf("LoadLibrary failed %lu\n", GetLastError()); return 1; }
    if (auto init = reinterpret_cast<InitDllProc>(GetProcAddress(lib, "InitDll"))) init();
    auto getFactory = reinterpret_cast<GetFactoryProc>(GetProcAddress(lib, "GetPluginFactory"));
    IPluginFactory* factory = getFactory();

    IEditController* ctrl = nullptr;
    for (int32 i = 0; i < factory->countClasses(); ++i) {
        PClassInfo ci;
        factory->getClassInfo(i, &ci);
        if (std::strcmp(ci.category, kVstComponentControllerClass) == 0)
            factory->createInstance(ci.cid, IEditController::iid, reinterpret_cast<void**>(&ctrl));
    }
    if (!ctrl) { printf("no controller\n"); return 1; }
    ctrl->initialize(nullptr);
    printf("parameters: %d\n", ctrl->getParameterCount());

    IPlugView* view = ctrl->createView(ViewType::kEditor);
    if (!view) { printf("no view\n"); return 1; }
    ViewRect r;
    view->getSize(&r);
    const int w = r.getWidth(), h = r.getHeight();
    printf("editor size %dx%d, HWND supported=%d\n", w, h, view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue);

    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SnapshotHost";
    RegisterClassW(&wc);
    HWND top = CreateWindowExW(0, L"SnapshotHost", L"host", WS_POPUP | WS_VISIBLE, 0, 0, w, h, nullptr, nullptr,
                               wc.hInstance, nullptr);
    view->attached(top, kPlatformTypeHWND);
    pump(800);
    HWND child = GetWindow(top, GW_CHILD);
    std::wstring prefix = argv[2];
    saveBmp(top, w, h, prefix + L"_1_init.bmp");

    auto click = [&](int x, int y) {
        SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessageW(child, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        pump(250);
    };

    // Next patch (arrow at the top right) three times -> "Strings 1983".
    for (int i = 0; i < 4; ++i) click(w - 16 - 14, 29);
    printf("program after 4 clicks: %.3f\n", ctrl->getParamNormalized(102));
    saveBmp(top, w, h, prefix + L"_2_patch.bmp");

    // Drag the Cutoff knob up (row 2, second knob) and hover it.
    // Row 2 has 13 cells stretched to the widest row (14 cells).
    const double cell2 = 64 + (w - 32 - (13 * 64 + 3 * 20 + 2 * 10)) / 13.0;
    const int cx = int(16 + 10 + cell2 * 1.5), cy = 58 + 16 + 116 + 10 + 50;
    const double before = ctrl->getParamNormalized(12);
    SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(cx, cy));
    for (int dy = 0; dy <= 60; dy += 10) { SendMessageW(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(cx, cy - dy)); pump(30); }
    printf("cutoff %.3f -> %.3f (while dragging)\n", before, ctrl->getParamNormalized(12));
    saveBmp(top, w, h, prefix + L"_3_drag.bmp");
    SendMessageW(child, WM_LBUTTONUP, 0, MAKELPARAM(cx, cy - 60));
    pump(100);

    // Click the Voice Mode selector (row 3, VOICE section first control) -> Mono.
    const double cell3 = 64 + (w - 32 - (11 * 64 + 2 * 20 + 1 * 10)) / 11.0;
    const int vx = int(16 + (5 * cell3 + 20) + 10 + 10 + cell3 / 2), vy = 58 + 16 + 2 * (116 + 10) + 50;
    click(vx, vy);
    printf("voice mode now %.3f\n", ctrl->getParamNormalized(31));
    saveBmp(top, w, h, prefix + L"_4_mode.bmp");

    view->removed();
    view->release();
    ctrl->terminate();
    ctrl->release();
    DestroyWindow(top);
    printf("done\n");
    return 0;
}

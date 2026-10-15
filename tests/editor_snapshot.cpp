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

    // Panel is drawn in logical units (1038 wide at 100%) and scaled.
    double sc = w / 1038.0;
    printf("opened at %.0f%% zoom\n", sc * 100);
    auto click = [&](double lx, double ly) {  // logical coordinates
        const LPARAM p = MAKELPARAM(int(lx * sc), int(ly * sc));
        SendMessageW(child, WM_MOUSEMOVE, 0, p);
        SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, p);
        SendMessageW(child, WM_LBUTTONUP, 0, p);
        pump(250);
    };
    auto hoverAt = [&](double lx, double ly) {
        SendMessageW(child, WM_MOUSEMOVE, 0, MAKELPARAM(int(lx * sc), int(ly * sc)));
        pump(150);
    };

    // 1. Open the OSC 1 wave dropdown, hover "Pulse", screenshot, pick it.
    click(58, 124);
    hoverAt(70, 186);
    saveBmp(top, w, h, prefix + L"_2_wave_dropdown.bmp");
    click(70, 186);
    printf("osc1 wave after picking Pulse: %.3f (expect 0.333)\n", ctrl->getParamNormalized(1));

    // 2. Zoom dropdown -> 150%: the view asks for a new size.
    click(677, 29);
    saveBmp(top, w, h, prefix + L"_3_zoom_dropdown.bmp");
    click(677, 115);
    view->getSize(&r);
    const int w2 = r.getWidth(), h2 = r.getHeight();
    printf("size after choosing 150%%: %dx%d\n", w2, h2);
    SetWindowPos(top, nullptr, 0, 0, w2, h2, SWP_NOZORDER | SWP_NOMOVE);
    pump(300);
    sc = w2 / 1038.0;
    saveBmp(top, w2, h2, prefix + L"_4_zoom150.bmp");

    // 3. Patch dropdown at 150%: open, hover 5th patch, screenshot, pick it.
    click(872, 29);
    hoverAt(872, 50 + 4 * 26 + 13);
    saveBmp(top, w2, h2, prefix + L"_5_patch_dropdown.bmp");
    click(872, 50 + 4 * 26 + 13);
    printf("program after picking 5th patch: %.3f (expect 0.500)\n", ctrl->getParamNormalized(102));

    // 4. Restore 125% so the next run starts from the default.
    click(677, 29);
    click(677, 50 + 1 * 26 + 13);
    view->getSize(&r);
    printf("size after choosing 125%%: %dx%d\n", r.getWidth(), r.getHeight());

    view->removed();
    view->release();
    ctrl->terminate();
    ctrl->release();
    DestroyWindow(top);
    printf("done\n");
    return 0;
}

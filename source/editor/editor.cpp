#include "editor.h"

#include "../controller.h"
#include "../ids.h"
#include "../params.h"
#include "../presets.h"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
namespace Gdiplus {
using std::max;
using std::min;
}
#include <gdiplus.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace retro80 {

namespace {

// ------------------------------------------------------------------ layout
struct SectionDef {
    const wchar_t* title;
    int accent;  // index into kAccents
    std::vector<ParamId> params;
};

const std::vector<std::vector<SectionDef>> kRows = {
    {
        {L"OSC 1", 0, {kOsc1Wave, kOsc1PW, kOsc1PWM}},
        {L"OSC 2", 0, {kOsc2Wave, kOsc2Semi, kOsc2Fine}},
        {L"MIXER", 1, {kMixOsc1, kMixOsc2, kMixSub, kMixNoise}},
        {L"LFO", 3, {kLfoWave, kLfoRate, kLfoDelay, kLfoPitch}},
    },
    {
        {L"FILTER", 1, {kHpf, kCutoff, kReso, kFiltEnvAmt, kFiltLfoAmt, kFiltKeytrack, kFiltVel}},
        {L"FILTER ENV", 2, {kFEnvA, kFEnvD, kFEnvS, kFEnvR}},
        {L"OUTPUT", 3, {kChorus, kVolume}},
    },
    {
        {L"AMP ENV", 2, {kAEnvA, kAEnvD, kAEnvS, kAEnvR, kAmpVel}},
        {L"VOICE", 0, {kVoiceMode, kUnisonVoices, kUnisonDetune, kGlide, kBendRange, kDrift}},
    },
};

constexpr int kMargin = 16;
constexpr int kHeaderH = 58;
constexpr int kCellW = 64;
constexpr int kSectionPad = 10;
constexpr int kSectionGap = 10;
constexpr int kSectionH = 116;
constexpr int kRowGap = 10;
constexpr int kKnobR = 18;

// Sunset palette.
const Gdiplus::Color kAccents[] = {
    Gdiplus::Color(255, 255, 138, 61),   // orange
    Gdiplus::Color(255, 255, 72, 132),   // pink
    Gdiplus::Color(255, 160, 92, 255),   // purple
    Gdiplus::Color(255, 64, 214, 230),   // cyan
};
const Gdiplus::Color kBg(255, 24, 22, 30);
const Gdiplus::Color kPanel(255, 36, 33, 45);
const Gdiplus::Color kBorder(255, 58, 53, 70);
const Gdiplus::Color kText(255, 236, 232, 244);
const Gdiplus::Color kTextDim(255, 150, 143, 168);
const Gdiplus::Color kKnobBody(255, 20, 18, 26);
const Gdiplus::Color kTrack(255, 62, 57, 76);

bool isBipolar(ParamId id) { return id == kFiltEnvAmt || id == kOsc2Semi || id == kOsc2Fine; }

struct Control {
    ParamId id;
    int accent;
    RECT hit;      // clickable area
    int cx, cy;    // knob center / selector center
    bool list;
};

struct SectionBox {
    const wchar_t* title;
    int accent;
    RECT r;
};

std::wstring widen(const char* s) {
    std::wstring w;
    while (*s) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s++)));
    return w;
}

// ------------------------------------------------------------------ GDI+ lifetime
int gGdiplusUsers = 0;
ULONG_PTR gGdiplusToken = 0;
const wchar_t* kWindowClass = L"Retro80EditorWindow";
int gClassUsers = 0;

HINSTANCE moduleInstance() {
    HMODULE m = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&moduleInstance), &m);
    return m;
}

} // namespace

// ------------------------------------------------------------------ implementation
struct Editor::Impl {
    Controller* controller = nullptr;
    HWND hwnd = nullptr;
    int width = 0, height = 0;
    std::vector<Control> controls;
    std::vector<SectionBox> sections;
    RECT prevPatch{}, nextPatch{};

    int hover = -1;
    int dragging = -1;
    int dragStartY = 0;
    double dragStartValue = 0;

    std::unique_ptr<Gdiplus::Bitmap> backbuffer;
    std::unique_ptr<Gdiplus::FontFamily> family;
    std::unique_ptr<Gdiplus::Font> fontTitle, fontSection, fontLabel, fontValue, fontPatch;

    void layout() {
        controls.clear();
        sections.clear();
        // Widest row decides the window width; narrower rows get wider cells.
        auto rowWidth = [](const std::vector<SectionDef>& row, int cell) {
            int w = 0;
            for (const auto& sec : row) w += static_cast<int>(sec.params.size()) * cell + 2 * kSectionPad + kSectionGap;
            return w - kSectionGap;
        };
        int maxRow = 0;
        for (const auto& row : kRows) maxRow = std::max(maxRow, rowWidth(row, kCellW));

        int y = kHeaderH + kMargin;
        for (const auto& row : kRows) {
            int cells = 0;
            for (const auto& sec : row) cells += static_cast<int>(sec.params.size());
            const int extra = maxRow - rowWidth(row, kCellW);
            const double cell = kCellW + double(extra) / cells;
            double x = kMargin;
            for (const auto& sec : row) {
                const double w = sec.params.size() * cell + 2 * kSectionPad;
                sections.push_back({sec.title, sec.accent, {int(x), y, int(x + w), y + kSectionH}});
                double cx = x + kSectionPad + cell / 2;
                for (ParamId id : sec.params) {
                    Control c;
                    c.id = id;
                    c.accent = sec.accent;
                    c.list = paramDef(id).scale == Scale::List;
                    c.cx = int(cx);
                    c.cy = y + 50;
                    c.hit = {int(cx - cell / 2) + 2, y + 24, int(cx + cell / 2) - 2, y + kSectionH - 4};
                    controls.push_back(c);
                    cx += cell;
                }
                x += w + kSectionGap;
            }
            y += kSectionH + kRowGap;
        }
        width = maxRow + 2 * kMargin;
        height = y - kRowGap + kMargin;
        prevPatch = {width - kMargin - 300, 16, width - kMargin - 272, 42};
        nextPatch = {width - kMargin - 28, 16, width - kMargin, 42};
    }

    double value(ParamId id) const { return controller->getParamNormalized(id); }

    int hitTest(int x, int y) const {
        for (size_t i = 0; i < controls.size(); ++i) {
            const RECT& r = controls[i].hit;
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return static_cast<int>(i);
        }
        return -1;
    }

    static bool inRect(const RECT& r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

    void setValue(ParamId id, double v, bool gesture = true) {
        v = std::clamp(v, 0.0, 1.0);
        if (gesture) controller->editorBeginEdit(id);
        controller->editorPerformEdit(id, v);
        if (gesture) controller->editorEndEdit(id);
    }

    double stepValue(ParamId id, double v, int delta) const {
        const int steps = stepCount(paramDef(id));
        if (steps > 0) {
            const int idx = static_cast<int>(std::lround(v * steps)) + delta;
            return std::clamp(idx, 0, steps) / static_cast<double>(steps);
        }
        return std::clamp(v + 0.01 * delta, 0.0, 1.0);
    }

    void changePatch(int delta) {
        const int n = kNumFactoryPatches;
        int idx = static_cast<int>(std::lround(value(kProgram) * (n - 1))) + delta;
        idx = (idx % n + n) % n;
        setValue(kProgram, n > 1 ? idx / double(n - 1) : 0.0);
    }

    // -------------------------------------------------------------- painting
    void createFonts() {
        for (const wchar_t* name : {L"Segoe UI", L"Arial", L"Tahoma", L"Verdana"}) {
            family = std::make_unique<Gdiplus::FontFamily>(name);
            if (family->GetLastStatus() == Gdiplus::Ok && family->IsAvailable()) break;
        }
        if (!family->IsAvailable()) family.reset(Gdiplus::FontFamily::GenericSansSerif()->Clone());
        fontTitle = std::make_unique<Gdiplus::Font>(family.get(), 26.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        fontSection = std::make_unique<Gdiplus::Font>(family.get(), 11.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        fontLabel = std::make_unique<Gdiplus::Font>(family.get(), 11.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        fontValue = std::make_unique<Gdiplus::Font>(family.get(), 10.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        fontPatch = std::make_unique<Gdiplus::Font>(family.get(), 14.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    }

    void releaseGraphics() {
        backbuffer.reset();
        fontTitle.reset(); fontSection.reset(); fontLabel.reset(); fontValue.reset(); fontPatch.reset();
        family.reset();
    }

    static void centeredText(Gdiplus::Graphics& g, const std::wstring& s, const Gdiplus::Font* f,
                             const Gdiplus::Color& c, float cx, float y, float w = 80.0f) {
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::SolidBrush b(c);
        g.DrawString(s.c_str(), -1, f, Gdiplus::RectF(cx - w / 2, y, w, 16), &fmt, &b);
    }

    static void roundRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
        p.AddArc(x, y, 2 * r, 2 * r, 180, 90);
        p.AddArc(x + w - 2 * r, y, 2 * r, 2 * r, 270, 90);
        p.AddArc(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 0, 90);
        p.AddArc(x, y + h - 2 * r, 2 * r, 2 * r, 90, 90);
        p.CloseFigure();
    }

    void paint(Gdiplus::Graphics& g) {
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        g.Clear(kBg);

        // Header: sunset stripe + logo + patch selector.
        {
            Gdiplus::LinearGradientBrush stripe(Gdiplus::Point(0, 0), Gdiplus::Point(width, 0), kAccents[0], kAccents[2]);
            Gdiplus::Color colors[] = {kAccents[0], kAccents[1], kAccents[2], kAccents[3]};
            Gdiplus::REAL pos[] = {0.0f, 0.35f, 0.7f, 1.0f};
            stripe.SetInterpolationColors(colors, pos, 4);
            g.FillRectangle(&stripe, 0, kHeaderH - 4, width, 4);

            Gdiplus::SolidBrush white(kText);
            g.DrawString(L"RETRO", -1, fontTitle.get(), Gdiplus::PointF(float(kMargin) - 2, 13), &white);
            Gdiplus::RectF box;
            g.MeasureString(L"RETRO", -1, fontTitle.get(), Gdiplus::PointF(0, 0), &box);
            Gdiplus::SolidBrush pink(kAccents[1]);
            g.DrawString(L"80", -1, fontTitle.get(), Gdiplus::PointF(float(kMargin) - 6 + box.Width, 13), &pink);
            Gdiplus::SolidBrush dim(kTextDim);
            g.DrawString(L"POLYPHONIC SYNTHESIZER  v" RETRO80_VERSION_WSTR, -1, fontValue.get(),
                         Gdiplus::PointF(float(kMargin) + box.Width + 36, 25), &dim);

            // Patch display.
            const float px = float(prevPatch.right) + 6, pw = float(nextPatch.left - prevPatch.right) - 12;
            Gdiplus::GraphicsPath lcd;
            roundRect(lcd, px, 16, pw, 26, 4);
            Gdiplus::SolidBrush lcdBg(Gdiplus::Color(255, 14, 12, 18));
            g.FillPath(&lcdBg, &lcd);
            Gdiplus::Pen lcdPen(kBorder, 1);
            g.DrawPath(&lcdPen, &lcd);
            const int idx = static_cast<int>(std::lround(value(kProgram) * (kNumFactoryPatches - 1)));
            centeredText(g, widen(kFactoryPatches[std::clamp(idx, 0, kNumFactoryPatches - 1)].name), fontPatch.get(),
                         kAccents[3], px + pw / 2, 20, pw);
            for (int side = 0; side < 2; ++side) {
                const RECT& r = side == 0 ? prevPatch : nextPatch;
                const float cx = (r.left + r.right) / 2.0f, cy = (r.top + r.bottom) / 2.0f;
                Gdiplus::PointF tri[3];
                if (side == 0) { tri[0] = {cx + 4, cy - 6}; tri[1] = {cx + 4, cy + 6}; tri[2] = {cx - 5, cy}; }
                else { tri[0] = {cx - 4, cy - 6}; tri[1] = {cx - 4, cy + 6}; tri[2] = {cx + 5, cy}; }
                Gdiplus::SolidBrush ab(kText);
                g.FillPolygon(&ab, tri, 3);
            }
        }

        // Sections.
        for (const auto& s : sections) {
            Gdiplus::GraphicsPath p;
            roundRect(p, float(s.r.left), float(s.r.top), float(s.r.right - s.r.left), float(s.r.bottom - s.r.top), 6);
            Gdiplus::SolidBrush panel(kPanel);
            g.FillPath(&panel, &p);
            Gdiplus::Pen border(kBorder, 1);
            g.DrawPath(&border, &p);
            Gdiplus::SolidBrush accent(kAccents[s.accent]);
            g.FillRectangle(&accent, float(s.r.left + 10), float(s.r.top + 9), 3.0f, 10.0f);
            Gdiplus::SolidBrush title(kAccents[s.accent]);
            g.DrawString(s.title, -1, fontSection.get(), Gdiplus::PointF(float(s.r.left + 16), float(s.r.top + 6)), &title);
        }

        // Controls.
        for (size_t i = 0; i < controls.size(); ++i) {
            const Control& c = controls[i];
            const ParamDef& d = paramDef(c.id);
            const double n = value(c.id);
            const bool active = static_cast<int>(i) == dragging || (dragging < 0 && static_cast<int>(i) == hover);
            const Gdiplus::Color& acc = kAccents[c.accent];

            if (c.list) {
                Gdiplus::GraphicsPath p;
                roundRect(p, c.cx - 28.0f, c.cy - 13.0f, 56.0f, 26.0f, 4);
                Gdiplus::SolidBrush body(Gdiplus::Color(255, 14, 12, 18));
                g.FillPath(&body, &p);
                Gdiplus::Pen pen(active ? acc : kBorder, 1.2f);
                g.DrawPath(&pen, &p);
                char text[64];
                formatValue(d, toPlain(d, n), text, sizeof(text));
                centeredText(g, widen(text), fontLabel.get(), acc, float(c.cx), c.cy - 7.0f, 54.0f);
            } else {
                const float r = float(kKnobR);
                const float x = c.cx - r, y = c.cy - r;
                // Track and value arcs (270 degrees, from bottom-left).
                Gdiplus::Pen track(kTrack, 3.5f);
                track.SetStartCap(Gdiplus::LineCapRound);
                track.SetEndCap(Gdiplus::LineCapRound);
                g.DrawArc(&track, x - 4, y - 4, 2 * r + 8, 2 * r + 8, 135.0f, 270.0f);
                const float startAngle = isBipolar(c.id) ? 270.0f : 135.0f;
                const float endAngle = 135.0f + 270.0f * float(n);
                const float sweep = endAngle - startAngle;
                if (std::fabs(sweep) > 0.5f) {
                    Gdiplus::Pen arc(acc, 3.5f);
                    arc.SetStartCap(Gdiplus::LineCapRound);
                    arc.SetEndCap(Gdiplus::LineCapRound);
                    g.DrawArc(&arc, x - 4, y - 4, 2 * r + 8, 2 * r + 8, startAngle, sweep);
                }
                // Body.
                Gdiplus::GraphicsPath body;
                body.AddEllipse(x + 3, y + 3, 2 * r - 6, 2 * r - 6);
                Gdiplus::PathGradientBrush shade(&body);
                shade.SetCenterColor(Gdiplus::Color(255, 52, 48, 64));
                Gdiplus::Color edge = kKnobBody;
                int count = 1;
                shade.SetSurroundColors(&edge, &count);
                shade.SetCenterPoint(Gdiplus::PointF(c.cx - 4.0f, c.cy - 5.0f));
                g.FillPath(&shade, &body);
                if (active) {
                    Gdiplus::Pen ring(acc, 1.0f);
                    g.DrawEllipse(&ring, x + 3, y + 3, 2 * r - 6, 2 * r - 6);
                }
                // Pointer.
                const float a = endAngle * 3.14159265f / 180.0f;
                Gdiplus::Pen ptr(kText, 2.2f);
                ptr.SetStartCap(Gdiplus::LineCapRound);
                ptr.SetEndCap(Gdiplus::LineCapRound);
                g.DrawLine(&ptr, c.cx + std::cos(a) * 4.0f, c.cy + std::sin(a) * 4.0f,
                           c.cx + std::cos(a) * (r - 5), c.cy + std::sin(a) * (r - 5));
            }

            centeredText(g, widen(d.shortName), fontLabel.get(), kText, float(c.cx), c.cy + 25.0f, kCellW - 2.0f);
            if (!c.list) {
                char text[64];
                formatValue(d, toPlain(d, n), text, sizeof(text));
                centeredText(g, widen(text), fontValue.get(), active ? acc : kTextDim, float(c.cx), c.cy + 39.0f,
                             kCellW - 2.0f);
            }
        }
    }

    void onPaint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!backbuffer || int(backbuffer->GetWidth()) != width || int(backbuffer->GetHeight()) != height)
            backbuffer = std::make_unique<Gdiplus::Bitmap>(width, height, PixelFormat32bppPARGB);
        {
            Gdiplus::Graphics g(backbuffer.get());
            paint(g);
        }
        Gdiplus::Graphics screen(hdc);
        screen.DrawImage(backbuffer.get(), 0, 0, width, height);
        EndPaint(hwnd, &ps);
    }

    void invalidate() { if (hwnd) InvalidateRect(hwnd, nullptr, FALSE); }

    // -------------------------------------------------------------- input
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_PAINT: onPaint(); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_LBUTTONDOWN: {
                const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                SetFocus(hwnd);
                if (inRect(prevPatch, x, y)) { changePatch(-1); return 0; }
                if (inRect(nextPatch, x, y)) { changePatch(+1); return 0; }
                const int i = hitTest(x, y);
                if (i < 0) return 0;
                const Control& c = controls[i];
                if (wp & MK_CONTROL) { setValue(c.id, defaultNormalized(paramDef(c.id))); return 0; }
                if (c.list) { setValue(c.id, stepValueWrap(c.id, value(c.id), +1)); return 0; }
                dragging = i;
                dragStartY = y;
                dragStartValue = value(c.id);
                controller->editorBeginEdit(c.id);
                SetCapture(hwnd);
                invalidate();
                return 0;
            }
            case WM_RBUTTONDOWN: {
                const int i = hitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                if (i >= 0 && controls[i].list) setValue(controls[i].id, stepValueWrap(controls[i].id, value(controls[i].id), -1));
                return 0;
            }
            case WM_LBUTTONDBLCLK: {
                const int i = hitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                if (i >= 0 && !controls[i].list) setValue(controls[i].id, defaultNormalized(paramDef(controls[i].id)));
                else if (i >= 0) setValue(controls[i].id, stepValueWrap(controls[i].id, value(controls[i].id), +1));
                return 0;
            }
            case WM_MOUSEMOVE: {
                const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                if (dragging >= 0) {
                    const Control& c = controls[dragging];
                    const double sens = (wp & MK_SHIFT) ? 0.0012 : 0.0055;
                    double v = std::clamp(dragStartValue + (dragStartY - y) * sens, 0.0, 1.0);
                    const int steps = stepCount(paramDef(c.id));
                    if (steps > 0) v = std::lround(v * steps) / double(steps);
                    if (v != value(c.id)) controller->editorPerformEdit(c.id, v);
                    invalidate();
                } else {
                    const int h = hitTest(x, y);
                    if (h != hover) { hover = h; invalidate(); }
                    TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, hwnd, 0};
                    TrackMouseEvent(&t);
                }
                return 0;
            }
            case WM_MOUSELEAVE:
                if (hover != -1) { hover = -1; invalidate(); }
                return 0;
            case WM_LBUTTONUP:
                if (dragging >= 0) {
                    controller->editorEndEdit(controls[dragging].id);
                    dragging = -1;
                    ReleaseCapture();
                    invalidate();
                }
                return 0;
            case WM_CAPTURECHANGED:
                if (dragging >= 0) {
                    controller->editorEndEdit(controls[dragging].id);
                    dragging = -1;
                    invalidate();
                }
                return 0;
            case WM_MOUSEWHEEL: {
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                ScreenToClient(hwnd, &pt);
                const int i = hitTest(pt.x, pt.y);
                if (i >= 0) {
                    const int delta = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
                    setValue(controls[i].id, stepValue(controls[i].id, value(controls[i].id), delta));
                }
                return 0;
            }
            default: break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    double stepValueWrap(ParamId id, double v, int delta) const {
        const int steps = stepCount(paramDef(id));
        if (steps <= 0) return v;
        int idx = static_cast<int>(std::lround(v * steps)) + delta;
        idx = (idx % (steps + 1) + steps + 1) % (steps + 1);
        return idx / double(steps);
    }

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && self->hwnd == hwnd) return self->handle(msg, wp, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
};

// ------------------------------------------------------------------ Editor
Editor::Editor(Controller* controller) : controller_(controller), impl_(std::make_unique<Impl>()) {
    impl_->controller = controller;
    impl_->layout();
    ViewRect r(0, 0, impl_->width, impl_->height);
    setRect(r);
}

Editor::~Editor() {
    removedFromParent();
    if (controller_) controller_->editorClosed(this);
}

tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type) {
    return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue : kResultFalse;
}

void Editor::attachedToParent() {
    if (impl_->hwnd || !systemWindow) return;
    if (gGdiplusUsers++ == 0) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&gGdiplusToken, &input, nullptr);
    }
    HINSTANCE inst = moduleInstance();
    if (gClassUsers++ == 0) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = &Impl::wndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        wc.lpszClassName = kWindowClass;
        RegisterClassExW(&wc);
    }
    impl_->createFonts();
    impl_->hwnd = nullptr;
    HWND h = CreateWindowExW(0, kWindowClass, L"Retro80", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0,
                             impl_->width, impl_->height, static_cast<HWND>(systemWindow), nullptr, inst, impl_.get());
    impl_->hwnd = h;
    impl_->invalidate();
}

void Editor::removedFromParent() {
    if (!impl_ || !impl_->hwnd) return;
    DestroyWindow(impl_->hwnd);
    impl_->hwnd = nullptr;
    impl_->dragging = -1;
    impl_->releaseGraphics();
    if (--gClassUsers == 0) UnregisterClassW(kWindowClass, moduleInstance());
    if (--gGdiplusUsers == 0) Gdiplus::GdiplusShutdown(gGdiplusToken);
}

void Editor::paramChanged(ParamID) {
    if (impl_) impl_->invalidate();
}

} // namespace retro80

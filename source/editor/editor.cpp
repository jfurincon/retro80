#include "editor.h"

#include "../controller.h"
#include "../ids.h"
#include "../params.h"
#include "../presets.h"

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include <algorithm>
namespace Gdiplus {
using std::max;
using std::min;
}
#include <gdiplus.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace retro80 {

namespace {

// ------------------------------------------------------------------ layout (logical units)
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
constexpr float kItemH = 26.0f;

const float kZoomSteps[] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
constexpr int kNumZoomSteps = 5;
constexpr float kDefaultZoom = 1.25f;

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
const Gdiplus::Color kLcd(255, 14, 12, 18);
const Gdiplus::Color kPopupBg(255, 30, 27, 38);
const Gdiplus::Color kPopupHover(255, 52, 46, 66);

bool isBipolar(ParamId id) { return id == kFiltEnvAmt || id == kOsc2Semi || id == kOsc2Fine; }

// ------------------------------------------------------------------ waveform icons
enum class Icon { None, Saw, Pulse, SawPulse, Triangle, Sine, Square, SawDown, Random };

Icon iconFor(ParamId id, int index) {
    static const Icon osc1[] = {Icon::Saw, Icon::Pulse, Icon::SawPulse, Icon::Triangle};
    static const Icon osc2[] = {Icon::Saw, Icon::Square, Icon::Triangle, Icon::Sine};
    static const Icon lfo[] = {Icon::Triangle, Icon::Sine, Icon::Square, Icon::SawDown, Icon::Random};
    switch (id) {
        case kOsc1Wave: return index >= 0 && index < 4 ? osc1[index] : Icon::None;
        case kOsc2Wave: return index >= 0 && index < 4 ? osc2[index] : Icon::None;
        case kLfoWave: return index >= 0 && index < 5 ? lfo[index] : Icon::None;
        default: return Icon::None;
    }
}

bool hasIcons(ParamId id) { return id == kOsc1Wave || id == kOsc2Wave || id == kLfoWave; }

float waveValue(Icon icon, float t, float pw) {
    t -= std::floor(t);
    const float saw = 2.0f * t - 1.0f;
    const float pulse = t < pw ? 1.0f : -1.0f;
    switch (icon) {
        case Icon::Saw: return saw;
        case Icon::Pulse: return pulse;
        case Icon::SawPulse: return 0.5f * (saw + pulse);
        case Icon::Triangle: return 1.0f - 4.0f * std::fabs(t - 0.5f);
        case Icon::Sine: return std::sin(2.0f * 3.14159265f * t);
        case Icon::Square: return t < 0.5f ? 1.0f : -1.0f;
        case Icon::SawDown: return -saw;
        case Icon::Random: {
            static const float steps[] = {0.6f, -0.3f, 0.9f, -0.8f};
            return steps[std::min(3, static_cast<int>(t * 4))];
        }
        default: return 0.0f;
    }
}

// Two cycles of the waveform in a w x h box; vertical edges stay vertical.
void drawWave(Gdiplus::Graphics& g, Icon icon, float x, float y, float w, float h, const Gdiplus::Color& c,
              float pw = 0.3f, float strokeW = 1.8f) {
    if (icon == Icon::None) return;
    std::vector<Gdiplus::PointF> pts;
    const int n = 96;
    const bool steppy = icon == Icon::Pulse || icon == Icon::Square || icon == Icon::Random ||
                        icon == Icon::Saw || icon == Icon::SawDown || icon == Icon::SawPulse;
    float prev = 0.0f;
    for (int i = 0; i <= n; ++i) {
        const float t = 2.0f * i / n;
        const float v = waveValue(icon, t - (i == n ? 1e-4f : 0.0f), pw);
        const float px = x + w * i / n, py = y + h * 0.5f - v * h * 0.45f;
        if (i > 0 && steppy && std::fabs(v - prev) > 0.3f) pts.push_back({px, y + h * 0.5f - prev * h * 0.45f});
        pts.push_back({px, py});
        prev = v;
    }
    Gdiplus::Pen pen(c, strokeW);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLines(&pen, pts.data(), static_cast<INT>(pts.size()));
}

// ------------------------------------------------------------------ helpers
struct Control {
    ParamId id;
    int accent;
    RECT hit;      // clickable area (logical)
    int cx, cy;    // knob / selector center (logical)
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

bool inRect(const RECT& r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }
bool inRectF(const Gdiplus::RectF& r, float x, float y) {
    return x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height;
}

void roundRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    p.AddArc(x, y, 2 * r, 2 * r, 180, 90);
    p.AddArc(x + w - 2 * r, y, 2 * r, 2 * r, 270, 90);
    p.AddArc(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 0, 90);
    p.AddArc(x, y + h - 2 * r, 2 * r, 2 * r, 90, 90);
    p.CloseFigure();
}

void drawCaret(Gdiplus::Graphics& g, float cx, float cy, const Gdiplus::Color& c) {
    Gdiplus::PointF tri[3] = {{cx - 3.5f, cy - 1.5f}, {cx + 3.5f, cy - 1.5f}, {cx, cy + 2.5f}};
    Gdiplus::SolidBrush b(c);
    g.FillPolygon(&b, tri, 3);
}

// ------------------------------------------------------------------ remembered zoom
std::wstring settingsPath() {
    wchar_t base[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, base))) return L"";
    std::wstring dir = std::wstring(base) + L"\\Retro80";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\settings.txt";
}

float loadZoom() {
    const std::wstring path = settingsPath();
    if (path.empty()) return kDefaultZoom;
    FILE* f = _wfopen(path.c_str(), L"r");
    if (!f) return kDefaultZoom;
    float z = kDefaultZoom;
    if (std::fscanf(f, "zoom=%f", &z) != 1) z = kDefaultZoom;
    std::fclose(f);
    for (float s : kZoomSteps) if (std::fabs(s - z) < 0.01f) return s;
    return kDefaultZoom;
}

void saveZoom(float z) {
    const std::wstring path = settingsPath();
    if (path.empty()) return;
    if (FILE* f = _wfopen(path.c_str(), L"w")) {
        std::fprintf(f, "zoom=%.2f\n", z);
        std::fclose(f);
    }
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
    Editor* owner = nullptr;
    Controller* controller = nullptr;
    HWND hwnd = nullptr;
    int width = 0, height = 0;  // logical size
    float zoom = kDefaultZoom, dpi = 1.0f;
    std::vector<Control> controls;
    std::vector<SectionBox> sections;
    RECT prevPatch{}, nextPatch{}, patchBox{}, zoomBox{};

    int hover = -1;
    int dragging = -1;
    int dragStartY = 0;
    double dragStartValue = 0;

    // Dropdown (waves, modes, patches, zoom).
    enum class PopupKind { None, Param, Patch, Zoom };
    struct Popup {
        PopupKind kind = PopupKind::None;
        ParamId id = kVolume;
        std::vector<std::wstring> items;
        std::vector<Icon> icons;
        int selected = -1;
        int hover = -1;
        int accent = 3;
        Gdiplus::RectF rect;
        Gdiplus::RectF itemRect(int i) const { return {rect.X + 4, rect.Y + 4 + i * kItemH, rect.Width - 8, kItemH}; }
    } popup;

    std::unique_ptr<Gdiplus::Bitmap> backbuffer;
    std::unique_ptr<Gdiplus::FontFamily> family;
    std::unique_ptr<Gdiplus::Font> fontTitle, fontSection, fontLabel, fontValue, fontPatch;

    float scale() const { return zoom * dpi; }
    int pixelWidth() const { return static_cast<int>(std::lround(width * scale())); }
    int pixelHeight() const { return static_cast<int>(std::lround(height * scale())); }

    // -------------------------------------------------------------- layout
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
        nextPatch = {width - kMargin - 28, 16, width - kMargin, 42};
        patchBox = {width - kMargin - 266, 16, width - kMargin - 34, 42};
        prevPatch = {patchBox.left - 34, 16, patchBox.left - 6, 42};
        zoomBox = {prevPatch.left - 78, 16, prevPatch.left - 12, 42};
    }

    double value(ParamId id) const { return controller->getParamNormalized(id); }
    int listIndex(ParamId id) const { return static_cast<int>(std::lround(value(id) * stepCount(paramDef(id)))); }
    int patchIndex() const {
        return std::clamp(static_cast<int>(std::lround(value(kProgram) * (kNumFactoryPatches - 1))), 0,
                          kNumFactoryPatches - 1);
    }

    int hitTest(int x, int y) const {
        for (size_t i = 0; i < controls.size(); ++i)
            if (inRect(controls[i].hit, x, y)) return static_cast<int>(i);
        return -1;
    }

    void setValue(ParamId id, double v) {
        v = std::clamp(v, 0.0, 1.0);
        controller->editorBeginEdit(id);
        controller->editorPerformEdit(id, v);
        controller->editorEndEdit(id);
    }

    double stepValue(ParamId id, double v, int delta) const {
        const int steps = stepCount(paramDef(id));
        if (steps > 0) {
            const int idx = static_cast<int>(std::lround(v * steps)) + delta;
            return std::clamp(idx, 0, steps) / static_cast<double>(steps);
        }
        return std::clamp(v + 0.01 * delta, 0.0, 1.0);
    }

    void setPatch(int idx) {
        const int n = kNumFactoryPatches;
        idx = (idx % n + n) % n;
        setValue(kProgram, n > 1 ? idx / double(n - 1) : 0.0);
    }

    void setZoom(float z) {
        if (std::fabs(z - zoom) < 0.001f) return;
        zoom = z;
        saveZoom(z);
        backbuffer.reset();
        owner->requestSize(pixelWidth(), pixelHeight());
    }

    // -------------------------------------------------------------- dropdowns
    void openPopup(PopupKind kind, const RECT& anchor, ParamId id, int accent) {
        popup = Popup{};
        popup.kind = kind;
        popup.id = id;
        popup.accent = accent;
        if (kind == PopupKind::Param) {
            const ParamDef& d = paramDef(id);
            for (int i = 0; i < d.labelCount; ++i) {
                popup.items.push_back(widen(d.labels[i]));
                popup.icons.push_back(iconFor(id, i));
            }
            popup.selected = listIndex(id);
        } else if (kind == PopupKind::Patch) {
            for (int i = 0; i < kNumFactoryPatches; ++i) {
                popup.items.push_back(widen(kFactoryPatches[i].name));
                popup.icons.push_back(Icon::None);
            }
            popup.selected = patchIndex();
        } else {
            for (int i = 0; i < kNumZoomSteps; ++i) {
                wchar_t t[16];
                swprintf(t, 16, L"%d%%", static_cast<int>(std::lround(kZoomSteps[i] * 100)));
                popup.items.push_back(t);
                popup.icons.push_back(Icon::None);
                if (std::fabs(kZoomSteps[i] - zoom) < 0.01f) popup.selected = i;
            }
        }
        const bool icons = kind == PopupKind::Param && hasIcons(id);
        const float w = std::max(float(anchor.right - anchor.left), icons ? 136.0f : 112.0f);
        const float h = popup.items.size() * kItemH + 8;
        float x = (anchor.left + anchor.right) / 2.0f - w / 2;
        x = std::clamp(x, 4.0f, width - w - 4.0f);
        float y = float(anchor.bottom) + 4;
        if (y + h > height - 4) y = float(anchor.top) - 4 - h;  // open upward near the bottom
        y = std::max(4.0f, y);
        popup.rect = {x, y, w, h};
        popup.hover = popup.selected;
        invalidate();
    }

    void choosePopupItem(int i) {
        const Popup p = popup;
        popup.kind = PopupKind::None;
        if (i < 0 || i >= static_cast<int>(p.items.size())) { invalidate(); return; }
        if (p.kind == PopupKind::Param) {
            const int steps = stepCount(paramDef(p.id));
            setValue(p.id, steps > 0 ? i / double(steps) : 0.0);
        } else if (p.kind == PopupKind::Patch) {
            setPatch(i);
        } else if (p.kind == PopupKind::Zoom) {
            setZoom(kZoomSteps[i]);
        }
        invalidate();
    }

    int popupItemAt(float x, float y) const {
        for (int i = 0; i < static_cast<int>(popup.items.size()); ++i)
            if (inRectF(popup.itemRect(i), x, y)) return i;
        return -1;
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

    static void text(Gdiplus::Graphics& g, const std::wstring& s, const Gdiplus::Font* f, const Gdiplus::Color& c,
                     const Gdiplus::RectF& r, Gdiplus::StringAlignment align = Gdiplus::StringAlignmentCenter) {
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(align);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::SolidBrush b(c);
        g.DrawString(s.c_str(), -1, f, r, &fmt, &b);
    }

    static void centeredText(Gdiplus::Graphics& g, const std::wstring& s, const Gdiplus::Font* f,
                             const Gdiplus::Color& c, float cx, float y, float w = 80.0f) {
        text(g, s, f, c, Gdiplus::RectF(cx - w / 2, y, w, 16));
    }

    void drawLcd(Gdiplus::Graphics& g, const RECT& r, bool highlight, const Gdiplus::Color& acc) {
        Gdiplus::GraphicsPath p;
        roundRect(p, float(r.left), float(r.top), float(r.right - r.left), float(r.bottom - r.top), 4);
        Gdiplus::SolidBrush bg(kLcd);
        g.FillPath(&bg, &p);
        Gdiplus::Pen pen(highlight ? acc : kBorder, 1.2f);
        g.DrawPath(&pen, &p);
    }

    void paintHeader(Gdiplus::Graphics& g) {
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

        // Zoom selector.
        const bool zoomOpen = popup.kind == PopupKind::Zoom;
        drawLcd(g, zoomBox, zoomOpen, kAccents[3]);
        wchar_t z[16];
        swprintf(z, 16, L"%d%%", static_cast<int>(std::lround(zoom * 100)));
        text(g, z, fontLabel.get(), kTextDim,
             Gdiplus::RectF(float(zoomBox.left) + 6, float(zoomBox.top), float(zoomBox.right - zoomBox.left) - 18, 26.0f));
        drawCaret(g, zoomBox.right - 10.0f, (zoomBox.top + zoomBox.bottom) / 2.0f, kTextDim);

        // Patch selector: arrows + dropdown.
        drawLcd(g, patchBox, popup.kind == PopupKind::Patch, kAccents[3]);
        text(g, widen(kFactoryPatches[patchIndex()].name), fontPatch.get(), kAccents[3],
             Gdiplus::RectF(float(patchBox.left) + 8, float(patchBox.top), float(patchBox.right - patchBox.left) - 26, 26.0f));
        drawCaret(g, patchBox.right - 12.0f, (patchBox.top + patchBox.bottom) / 2.0f, kAccents[3]);
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

    void paintKnob(Gdiplus::Graphics& g, const Control& c, double n, bool active) {
        const Gdiplus::Color& acc = kAccents[c.accent];
        const float r = float(kKnobR);
        const float x = c.cx - r, y = c.cy - r;
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
        const float a = endAngle * 3.14159265f / 180.0f;
        Gdiplus::Pen ptr(kText, 2.2f);
        ptr.SetStartCap(Gdiplus::LineCapRound);
        ptr.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&ptr, c.cx + std::cos(a) * 4.0f, c.cy + std::sin(a) * 4.0f, c.cx + std::cos(a) * (r - 5),
                   c.cy + std::sin(a) * (r - 5));
    }

    RECT selectorRect(const Control& c) const { return {c.cx - 29, c.cy - 15, c.cx + 29, c.cy + 15}; }

    void paintSelector(Gdiplus::Graphics& g, const Control& c, bool active) {
        const Gdiplus::Color& acc = kAccents[c.accent];
        const RECT r = selectorRect(c);
        const bool open = popup.kind == PopupKind::Param && popup.id == c.id;
        drawLcd(g, r, active || open, acc);
        const int idx = listIndex(c.id);
        const Icon icon = iconFor(c.id, idx);
        if (icon != Icon::None) {
            drawWave(g, icon, r.left + 7.0f, r.top + 7.0f, 34.0f, 16.0f, acc, float(toPlainPW()));
        } else {
            const ParamDef& d = paramDef(c.id);
            text(g, widen(d.labels[std::clamp(idx, 0, d.labelCount - 1)]), fontLabel.get(), acc,
                 Gdiplus::RectF(r.left + 3.0f, float(r.top), 44.0f, 30.0f));
        }
        drawCaret(g, r.right - 8.0f, (r.top + r.bottom) / 2.0f, active || open ? acc : kTextDim);
    }

    double toPlainPW() const { return toPlain(paramDef(kOsc1PW), value(kOsc1PW)); }

    void paintPopup(Gdiplus::Graphics& g) {
        if (popup.kind == PopupKind::None) return;
        const Gdiplus::Color& acc = kAccents[popup.accent];
        const Gdiplus::RectF& r = popup.rect;
        // Soft shadow.
        for (int i = 3; i >= 1; --i) {
            Gdiplus::GraphicsPath sp;
            roundRect(sp, r.X - i + 2, r.Y - i + 4, r.Width + 2 * i, r.Height + 2 * i, 7);
            Gdiplus::SolidBrush sb(Gdiplus::Color(40, 0, 0, 0));
            g.FillPath(&sb, &sp);
        }
        Gdiplus::GraphicsPath p;
        roundRect(p, r.X, r.Y, r.Width, r.Height, 6);
        Gdiplus::SolidBrush bg(kPopupBg);
        g.FillPath(&bg, &p);
        Gdiplus::Pen border(acc, 1.2f);
        g.DrawPath(&border, &p);

        const bool icons = popup.kind == PopupKind::Param && hasIcons(popup.id);
        for (int i = 0; i < static_cast<int>(popup.items.size()); ++i) {
            const Gdiplus::RectF ir = popup.itemRect(i);
            const bool sel = i == popup.selected;
            if (i == popup.hover) {
                Gdiplus::GraphicsPath hp;
                roundRect(hp, ir.X, ir.Y + 1, ir.Width, ir.Height - 2, 4);
                Gdiplus::SolidBrush hb(kPopupHover);
                g.FillPath(&hb, &hp);
            }
            if (sel) {
                Gdiplus::SolidBrush mark(acc);
                g.FillRectangle(&mark, ir.X + 2, ir.Y + 7, 3.0f, ir.Height - 14);
            }
            float tx = ir.X + 12;
            if (icons) {
                drawWave(g, popup.icons[i], ir.X + 12, ir.Y + 6, 36.0f, 14.0f, sel || i == popup.hover ? acc : kTextDim,
                         float(toPlainPW()));
                tx = ir.X + 58;
            }
            text(g, popup.items[i], fontLabel.get(), sel ? acc : kText,
                 Gdiplus::RectF(tx, ir.Y, ir.X + ir.Width - tx - 4, ir.Height), Gdiplus::StringAlignmentNear);
        }
    }

    void paint(Gdiplus::Graphics& g) {
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.Clear(kBg);
        g.ScaleTransform(scale(), scale());

        paintHeader(g);

        for (const auto& s : sections) {
            Gdiplus::GraphicsPath p;
            roundRect(p, float(s.r.left), float(s.r.top), float(s.r.right - s.r.left), float(s.r.bottom - s.r.top), 6);
            Gdiplus::SolidBrush panel(kPanel);
            g.FillPath(&panel, &p);
            Gdiplus::Pen border(kBorder, 1);
            g.DrawPath(&border, &p);
            Gdiplus::SolidBrush accent(kAccents[s.accent]);
            g.FillRectangle(&accent, float(s.r.left + 10), float(s.r.top + 9), 3.0f, 10.0f);
            g.DrawString(s.title, -1, fontSection.get(), Gdiplus::PointF(float(s.r.left + 16), float(s.r.top + 6)),
                         &accent);
        }

        for (size_t i = 0; i < controls.size(); ++i) {
            const Control& c = controls[i];
            const ParamDef& d = paramDef(c.id);
            const double n = value(c.id);
            const bool active = popup.kind == PopupKind::None &&
                                (static_cast<int>(i) == dragging || (dragging < 0 && static_cast<int>(i) == hover));
            if (c.list) paintSelector(g, c, active);
            else paintKnob(g, c, n, active);

            centeredText(g, widen(d.shortName), fontLabel.get(), kText, float(c.cx), c.cy + 25.0f, kCellW - 2.0f);
            char buf[64];
            formatValue(d, toPlain(d, n), buf, sizeof(buf));
            centeredText(g, widen(buf), fontValue.get(), active ? kAccents[c.accent] : kTextDim, float(c.cx),
                         c.cy + 39.0f, kCellW + 6.0f);
        }

        paintPopup(g);
    }

    void onPaint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        const int pw = pixelWidth(), ph = pixelHeight();
        if (!backbuffer || int(backbuffer->GetWidth()) != pw || int(backbuffer->GetHeight()) != ph)
            backbuffer = std::make_unique<Gdiplus::Bitmap>(pw, ph, PixelFormat32bppPARGB);
        {
            Gdiplus::Graphics g(backbuffer.get());
            paint(g);
        }
        Gdiplus::Graphics screen(hdc);
        screen.DrawImage(backbuffer.get(), 0, 0, pw, ph);
        EndPaint(hwnd, &ps);
    }

    void invalidate() { if (hwnd) InvalidateRect(hwnd, nullptr, FALSE); }

    // -------------------------------------------------------------- input (converted to logical units)
    void logical(LPARAM lp, int& x, int& y) const {
        x = static_cast<int>(GET_X_LPARAM(lp) / scale());
        y = static_cast<int>(GET_Y_LPARAM(lp) / scale());
    }

    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_PAINT: onPaint(); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_LBUTTONDOWN: {
                int x, y;
                logical(lp, x, y);
                if (popup.kind != PopupKind::None) {  // any click closes the dropdown
                    choosePopupItem(popupItemAt(float(x), float(y)));
                    return 0;
                }
                if (inRect(prevPatch, x, y)) { setPatch(patchIndex() - 1); return 0; }
                if (inRect(nextPatch, x, y)) { setPatch(patchIndex() + 1); return 0; }
                if (inRect(patchBox, x, y)) { openPopup(PopupKind::Patch, patchBox, kProgram, 3); return 0; }
                if (inRect(zoomBox, x, y)) { openPopup(PopupKind::Zoom, zoomBox, kProgram, 3); return 0; }
                const int i = hitTest(x, y);
                if (i < 0) return 0;
                const Control& c = controls[i];
                if (c.list) { openPopup(PopupKind::Param, selectorRect(c), c.id, c.accent); return 0; }
                if (wp & MK_CONTROL) { setValue(c.id, defaultNormalized(paramDef(c.id))); return 0; }
                dragging = i;
                dragStartY = GET_Y_LPARAM(lp);
                dragStartValue = value(c.id);
                controller->editorBeginEdit(c.id);
                SetCapture(hwnd);
                invalidate();
                return 0;
            }
            case WM_RBUTTONDOWN:
                if (popup.kind != PopupKind::None) { popup.kind = PopupKind::None; invalidate(); }
                return 0;
            case WM_LBUTTONDBLCLK: {
                int x, y;
                logical(lp, x, y);
                if (popup.kind != PopupKind::None) { choosePopupItem(popupItemAt(float(x), float(y))); return 0; }
                const int i = hitTest(x, y);
                if (i >= 0 && !controls[i].list) setValue(controls[i].id, defaultNormalized(paramDef(controls[i].id)));
                return 0;
            }
            case WM_MOUSEMOVE: {
                int x, y;
                logical(lp, x, y);
                if (dragging >= 0) {
                    const Control& c = controls[dragging];
                    // Drag distance is in screen pixels so the feel is the same at every zoom.
                    const double sens = (wp & MK_SHIFT) ? 0.0012 : 0.0055;
                    double v = std::clamp(dragStartValue + (dragStartY - GET_Y_LPARAM(lp)) * sens / scale(), 0.0, 1.0);
                    const int steps = stepCount(paramDef(c.id));
                    if (steps > 0) v = std::lround(v * steps) / double(steps);
                    if (v != value(c.id)) controller->editorPerformEdit(c.id, v);
                    invalidate();
                } else if (popup.kind != PopupKind::None) {
                    const int h = popupItemAt(float(x), float(y));
                    if (h != popup.hover && h >= 0) { popup.hover = h; invalidate(); }
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
            // The panel never takes keyboard focus, so FL's typing keyboard keeps
            // playing notes. If keys reach us anyway, hand focus and the key back.
            case WM_SETFOCUS:
                if (HWND parent = GetParent(hwnd)) SetFocus(parent);
                return 0;
            case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR:
                if (msg == WM_KEYDOWN && wp == VK_ESCAPE && popup.kind != PopupKind::None) {
                    popup.kind = PopupKind::None;
                    invalidate();
                    return 0;
                }
                if (HWND parent = GetParent(hwnd)) return SendMessageW(parent, msg, wp, lp);
                break;
            case WM_MOUSEWHEEL: {
                if (popup.kind != PopupKind::None) return 0;
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                ScreenToClient(hwnd, &pt);
                const int i = hitTest(static_cast<int>(pt.x / scale()), static_cast<int>(pt.y / scale()));
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
    impl_->owner = this;
    impl_->controller = controller;
    impl_->zoom = loadZoom();
    impl_->layout();
    ViewRect r(0, 0, impl_->pixelWidth(), impl_->pixelHeight());
    setRect(r);
}

Editor::~Editor() {
    removedFromParent();
    if (controller_) controller_->editorClosed(this);
}

tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type) {
    return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue : kResultFalse;
}

void Editor::requestSize(int width, int height) {
    ViewRect r(0, 0, width, height);
    // Ask the host to resize its window; it answers with onSize().
    if (!(plugFrame && plugFrame->resizeView(this, &r) == kResultTrue)) onSize(&r);
}

tresult PLUGIN_API Editor::onSize(ViewRect* newSize) {
    if (newSize) setRect(*newSize);
    if (impl_ && impl_->hwnd) {
        SetWindowPos(impl_->hwnd, nullptr, 0, 0, rect.getWidth(), rect.getHeight(),
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        impl_->invalidate();
    }
    return kResultTrue;
}

tresult PLUGIN_API Editor::setContentScaleFactor(ScaleFactor factor) {
    factor = std::clamp(factor, 1.0f, 4.0f);
    if (std::fabs(factor - impl_->dpi) < 0.001f) return kResultTrue;
    impl_->dpi = factor;
    impl_->backbuffer.reset();
    requestSize(impl_->pixelWidth(), impl_->pixelHeight());
    return kResultTrue;
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
    impl_->popup.kind = Impl::PopupKind::None;
    HWND h = CreateWindowExW(0, kWindowClass, L"Retro80", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0,
                             rect.getWidth(), rect.getHeight(), static_cast<HWND>(systemWindow), nullptr, inst,
                             impl_.get());
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

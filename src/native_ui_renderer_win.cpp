#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "native_ui_renderer.h"

namespace NativeUiRenderer {
namespace {
using Microsoft::WRL::ComPtr;

struct GradientBrush {
  ComPtr<ID2D1LinearGradientBrush> brush;
  uint64_t lastUsed = 0;
};

struct WindowSurface {
  ComPtr<ID2D1HwndRenderTarget> target;
  ComPtr<ID2D1GdiInteropRenderTarget> interop;
  ComPtr<ID2D1SolidColorBrush> brush;
  std::unordered_map<uint64_t, GradientBrush> gradients;
  uint64_t gradientClock = 0;
  UINT width = 0;
  UINT height = 0;
  ULONGLONG retryAfter = 0;
  bool hasFrame = false;
};

struct Font {
  LOGFONTW description{};
  TEXTMETRICW metrics{};
  ComPtr<IDWriteTextFormat> format;
  uint64_t serial = 0;
};

struct LayoutKey {
  uint64_t font = 0;
  std::string text;
  LONG width = 0;
  LONG height = 0;
  UINT flags = 0;
  bool operator==(const LayoutKey& other) const
  {
    return font == other.font && width == other.width && height == other.height &&
      flags == other.flags && text == other.text;
  }
};

struct LayoutHash {
  size_t operator()(const LayoutKey& key) const
  {
    size_t value = std::hash<std::string>{}(key.text);
    const auto combine = [&value](size_t next) {
      value ^= next + static_cast<size_t>(0x9e3779b9) + (value << 6) + (value >> 2);
    };
    combine(std::hash<uint64_t>{}(key.font));
    combine(std::hash<LONG>{}(key.width));
    combine(std::hash<LONG>{}(key.height));
    combine(std::hash<UINT>{}(key.flags));
    return value;
  }
};

struct CachedLayout {
  ComPtr<IDWriteTextLayout> layout;
  DWRITE_TEXT_METRICS metrics{};
  std::list<LayoutKey>::iterator recent;
};

struct Renderer {
  ComPtr<ID2D1Factory> factory;
  ComPtr<IDWriteFactory> writeFactory;
  std::unordered_map<HWND, std::unique_ptr<WindowSurface>> surfaces;
  std::unordered_map<HFONT, Font> fonts;
  std::unordered_map<LayoutKey, CachedLayout, LayoutHash> layouts;
  std::list<LayoutKey> layoutLru;
  size_t layoutTextBytes = 0;
  uint64_t fontSerial = 0;
  HWND window = nullptr;
  HDC proxy = nullptr;
  HDC interopDc = nullptr;
  int interopSavedState = 0;
  WindowSurface* surface = nullptr;
  RECT client{};
  HRGN scratchRegion = nullptr;
  HRGN auxiliaryRegion = nullptr;
  HRGN cachedRegion = nullptr;
  ComPtr<ID2D1PathGeometry> clipGeometry;
  bool clipPushed = false;
  bool clipIsLayer = false;
  bool emptyClip = false;
  bool clipDirty = true;
  int clipKind = NULLREGION;
  RECT clipBounds{};
  bool failed = false;
  bool fullPaint = true;
  bool attemptedFactory = false;

  ~Renderer()
  {
    if (scratchRegion) DeleteObject(scratchRegion);
    if (auxiliaryRegion) DeleteObject(auxiliaryRegion);
    if (cachedRegion) DeleteObject(cachedRegion);
  }
};

Renderer& renderer()
{
  static Renderer instance;
  return instance;
}

bool factories()
{
  auto& r = renderer();
  if (r.factory && r.writeFactory) return true;
  if (r.attemptedFactory) return false;
  r.attemptedFactory = true;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
        r.factory.GetAddressOf()))) return false;
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(r.writeFactory.GetAddressOf())))) {
    r.factory.Reset();
    return false;
  }
  r.scratchRegion = CreateRectRgn(0, 0, 0, 0);
  r.auxiliaryRegion = CreateRectRgn(0, 0, 0, 0);
  r.cachedRegion = CreateRectRgn(0, 0, 0, 0);
  if (r.scratchRegion && r.auxiliaryRegion && r.cachedRegion) return true;
  if (r.scratchRegion) DeleteObject(r.scratchRegion);
  if (r.auxiliaryRegion) DeleteObject(r.auxiliaryRegion);
  if (r.cachedRegion) DeleteObject(r.cachedRegion);
  r.scratchRegion = r.auxiliaryRegion = r.cachedRegion = nullptr;
  r.writeFactory.Reset();
  r.factory.Reset();
  return false;
}

void discard(WindowSurface& surface)
{
  surface.gradients.clear();
  surface.brush.Reset();
  surface.interop.Reset();
  surface.target.Reset();
  surface.hasFrame = false;
}

WindowSurface* windowSurface(HWND hwnd, const RECT& client)
{
  if (!hwnd || !factories()) return nullptr;
  const UINT width = static_cast<UINT>(std::max<LONG>(1, client.right - client.left));
  const UINT height = static_cast<UINT>(std::max<LONG>(1, client.bottom - client.top));
  auto& slot = renderer().surfaces[hwnd];
  if (!slot) slot = std::make_unique<WindowSurface>();
  auto& surface = *slot;
  if (surface.width != width || surface.height != height) {
    surface.width = width;
    surface.height = height;
    surface.retryAfter = 0;
    surface.hasFrame = false;
    if (surface.target &&
        FAILED(surface.target->Resize(D2D1::SizeU(width, height)))) discard(surface);
    // A resized swap chain has no previous image to retain.
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  if (surface.target) return &surface;
  if (GetTickCount64() < surface.retryAfter) return nullptr;

  auto properties = D2D1::RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE_HARDWARE,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
    96.0f, 96.0f, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
  const auto windowProperties = D2D1::HwndRenderTargetProperties(
    hwnd, D2D1::SizeU(width, height),
    // Painting is synchronous on REAPER's input thread, including UpdateWindow
    // during arrow navigation. Timers already pace the UI; waiting for vblank
    // here stalls keyboard repeat (and compounds across controller/clock views).
    static_cast<D2D1_PRESENT_OPTIONS>(D2D1_PRESENT_OPTIONS_RETAIN_CONTENTS |
      D2D1_PRESENT_OPTIONS_IMMEDIATELY));
  HRESULT hr = renderer().factory->CreateHwndRenderTarget(
    properties, windowProperties, surface.target.GetAddressOf());
  if (FAILED(hr)) {
    // Remote sessions or older drivers may only expose the software target.
    // Keep the same drawing/interop contract and let Direct2D select it.
    properties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    hr = renderer().factory->CreateHwndRenderTarget(
      properties, windowProperties, surface.target.GetAddressOf());
  }
  if (SUCCEEDED(hr)) hr = surface.target.As(&surface.interop);
  if (SUCCEEDED(hr)) hr = surface.target->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::Black), surface.brush.GetAddressOf());
  if (FAILED(hr)) {
    discard(surface);
    surface.retryAfter = GetTickCount64() + 5000;
    return nullptr;
  }
  surface.target->SetDpi(96.0f, 96.0f);
  surface.target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  surface.hasFrame = false;
  InvalidateRect(hwnd, nullptr, FALSE);
  return &surface;
}

bool defaultCoordinates(HDC dc)
{
  if (!dc || GetMapMode(dc) != MM_TEXT || GetLayout(dc) != 0) return false;
  POINT viewport{}, origin{};
  if (!GetViewportOrgEx(dc, &viewport) || !GetWindowOrgEx(dc, &origin) ||
      viewport.x || viewport.y || origin.x || origin.y) return false;
  if (GetGraphicsMode(dc) == GM_ADVANCED) {
    XFORM transform{};
    if (!GetWorldTransform(dc, &transform) ||
        transform.eM11 != 1 || transform.eM12 != 0 ||
        transform.eM21 != 0 || transform.eM22 != 1 ||
        transform.eDx != 0 || transform.eDy != 0) return false;
  }
  return true;
}

void popClip()
{
  auto& r = renderer();
  if (!r.clipPushed || !r.surface) return;
  if (r.clipIsLayer) r.surface->target->PopLayer();
  else r.surface->target->PopAxisAlignedClip();
  r.clipPushed = false;
}

bool releaseInterop()
{
  auto& r = renderer();
  if (!r.interopDc) return !r.failed;
  // GetDC/ReleaseDC are the ordering barrier between legacy GDI and the GPU.
  // Never EndDraw per primitive or read the window back into a CPU bitmap.
  GdiFlush();
  if (r.interopSavedState) RestoreDC(r.interopDc, r.interopSavedState);
  r.interopSavedState = 0;
  const HRESULT hr = r.surface->interop->ReleaseDC(nullptr);
  r.interopDc = nullptr;
  if (FAILED(hr)) r.failed = true;
  return !r.failed;
}

bool effectiveClip(HDC dc)
{
  auto& r = renderer();
  RECT box{};
  const int boxKind = GetClipBox(dc, &box);
  if (boxKind == ERROR) return false;
  box.left = std::max(box.left, r.client.left);
  box.top = std::max(box.top, r.client.top);
  box.right = std::min(box.right, r.client.right);
  box.bottom = std::min(box.bottom, r.client.bottom);
  SetRectRgn(r.scratchRegion, box.left, box.top,
    std::max(box.left, box.right), std::max(box.top, box.bottom));
  // APIRGN includes a DC's meta-region as well as its selected application
  // clip. SYSRGN adds the exact BeginPaint update region for window DCs.
  if (GetRandomRgn(dc, r.auxiliaryRegion, 3) == 1)
    CombineRgn(r.scratchRegion, r.scratchRegion, r.auxiliaryRegion, RGN_AND);
  else if (GetClipRgn(dc, r.auxiliaryRegion) == 1)
    CombineRgn(r.scratchRegion, r.scratchRegion, r.auxiliaryRegion, RGN_AND);
  if (GetObjectType(dc) != OBJ_MEMDC &&
      GetRandomRgn(dc, r.auxiliaryRegion, 4) == 1) {
    POINT origin{};
    if (!GetDCOrgEx(dc, &origin)) return false;
    OffsetRgn(r.auxiliaryRegion, -origin.x, -origin.y);
    CombineRgn(r.scratchRegion, r.scratchRegion, r.auxiliaryRegion, RGN_AND);
  }
  return true;
}

bool updateClip()
{
  auto& r = renderer();
  if (!r.clipDirty) return true;
  if (!effectiveClip(r.proxy)) return false;
  const bool unchanged = EqualRgn(r.scratchRegion, r.cachedRegion) != FALSE;
  if (!unchanged) {
    popClip();
    CombineRgn(r.cachedRegion, r.scratchRegion, nullptr, RGN_COPY);
    r.clipGeometry.Reset();
  }
  r.clipKind = GetRgnBox(r.cachedRegion, &r.clipBounds);
  r.emptyClip = r.clipKind == NULLREGION;
  if (r.clipKind == ERROR) return false;
  r.clipDirty = false;
  return true;
}

bool pushClip()
{
  auto& r = renderer();
  if (!updateClip()) return false;
  if (r.clipPushed) return true;
  if (r.emptyClip) return true;
  if (r.clipKind == SIMPLEREGION) {
    const RECT& bounds = r.clipBounds;
    r.surface->target->PushAxisAlignedClip(D2D1::RectF(
      static_cast<float>(bounds.left), static_cast<float>(bounds.top),
      static_cast<float>(bounds.right), static_cast<float>(bounds.bottom)),
      D2D1_ANTIALIAS_MODE_ALIASED);
    r.clipIsLayer = false;
  } else {
    if (!r.clipGeometry) {
      const DWORD size = GetRegionData(r.cachedRegion, 0, nullptr);
      if (!size) return false;
      std::vector<BYTE> bytes(size);
      auto* data = reinterpret_cast<RGNDATA*>(bytes.data());
      if (GetRegionData(r.cachedRegion, size, data) != size) return false;
      if (FAILED(r.factory->CreatePathGeometry(r.clipGeometry.GetAddressOf())))
        return false;
      ComPtr<ID2D1GeometrySink> sink;
      if (FAILED(r.clipGeometry->Open(sink.GetAddressOf()))) {
        r.clipGeometry.Reset();
        return false;
      }
      sink->SetFillMode(D2D1_FILL_MODE_WINDING);
      const auto* rects = reinterpret_cast<const RECT*>(data->Buffer);
      for (DWORD i = 0; i < data->rdh.nCount; ++i) {
        const auto& a = rects[i];
        sink->BeginFigure(D2D1::Point2F(static_cast<float>(a.left),
          static_cast<float>(a.top)), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(static_cast<float>(a.right), static_cast<float>(a.top)));
        sink->AddLine(D2D1::Point2F(static_cast<float>(a.right), static_cast<float>(a.bottom)));
        sink->AddLine(D2D1::Point2F(static_cast<float>(a.left), static_cast<float>(a.bottom)));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
      }
      if (FAILED(sink->Close())) {
        r.clipGeometry.Reset();
        return false;
      }
    }
    r.surface->target->PushLayer(D2D1::LayerParameters(
      D2D1::InfiniteRect(), r.clipGeometry.Get(), D2D1_ANTIALIAS_MODE_ALIASED,
      D2D1::Matrix3x2F::Identity(), 1.0f, nullptr,
      D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE), nullptr);
    r.clipIsLayer = true;
  }
  r.clipPushed = true;
  return true;
}

bool drawing(HDC dc)
{
  auto& r = renderer();
  return dc && r.proxy == dc && r.surface && !r.failed &&
    releaseInterop() && pushClip();
}

D2D1_COLOR_F color(COLORREF value)
{
  return D2D1::ColorF(GetRValue(value) / 255.0f,
    GetGValue(value) / 255.0f, GetBValue(value) / 255.0f, 1.0f);
}

ID2D1SolidColorBrush* brush(COLORREF value)
{
  auto* result = renderer().surface->brush.Get();
  result->SetColor(color(value));
  return result;
}

D2D1_RECT_F rectangle(const RECT& rect, float inset = 0.0f)
{
  return D2D1::RectF(rect.left + inset, rect.top + inset,
    rect.right - inset, rect.bottom - inset);
}

D2D1_ROUNDED_RECT roundedRectangle(const RECT& rect, int radius)
{
  const float corner = std::max(0.0f, std::min(static_cast<float>(radius),
    std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.5f));
  return D2D1::RoundedRect(rectangle(rect, 0.5f), corner, corner);
}

bool solidPen(HDC dc, LOGPEN& pen)
{
  HGDIOBJ handle = GetCurrentObject(dc, OBJ_PEN);
  if (!handle || GetObjectType(handle) != OBJ_PEN ||
      GetObjectW(handle, sizeof(pen), &pen) != sizeof(pen) ||
      (pen.lopnStyle != PS_SOLID && pen.lopnStyle != PS_NULL)) return false;
  if (handle == GetStockObject(DC_PEN)) pen.lopnColor = GetDCPenColor(dc);
  return true;
}

bool solidBrush(HDC dc, LOGBRUSH& selected)
{
  HGDIOBJ handle = GetCurrentObject(dc, OBJ_BRUSH);
  if (!handle || GetObjectW(handle, sizeof(selected), &selected) != sizeof(selected) ||
      (selected.lbStyle != BS_SOLID && selected.lbStyle != BS_NULL)) return false;
  if (handle == GetStockObject(DC_BRUSH)) selected.lbColor = GetDCBrushColor(dc);
  return true;
}

bool wideText(const std::string& source, std::wstring& result)
{
  if (source.empty()) { result.clear(); return true; }
  if (source.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
  const int length = MultiByteToWideChar(CP_UTF8, 0, source.data(),
    static_cast<int>(source.size()), nullptr, 0);
  if (!length) return false;
  result.resize(static_cast<size_t>(length));
  return MultiByteToWideChar(CP_UTF8, 0, source.data(),
    static_cast<int>(source.size()), result.data(), length) == length;
}

void clearLayouts()
{
  auto& r = renderer();
  r.layouts.clear();
  r.layoutLru.clear();
  r.layoutTextBytes = 0;
}

void eraseFontLayouts(uint64_t serial)
{
  auto& r = renderer();
  for (auto it = r.layouts.begin(); it != r.layouts.end();) {
    if (it->first.font == serial) {
      r.layoutTextBytes -= it->first.text.size();
      r.layoutLru.erase(it->second.recent);
      it = r.layouts.erase(it);
    } else ++it;
  }
}

Font* fontFor(HDC dc, HFONT handle)
{
  if (!dc || !factories()) return nullptr;
  if (!handle) handle = static_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
  if (!handle) return nullptr;
  LOGFONTW description{};
  if (GetObjectW(handle, sizeof(description), &description) != sizeof(description) ||
      description.lfEscapement || description.lfOrientation || description.lfWidth)
    return nullptr;
  auto& cache = renderer().fonts;
  const auto found = cache.find(handle);
  if (found != cache.end() &&
      !std::memcmp(&found->second.description, &description, sizeof(description)))
    return &found->second;
  if (found != cache.end()) eraseFontLayouts(found->second.serial);
  Font result;
  result.serial = ++renderer().fontSerial;
  result.description = description;
  HGDIOBJ previous = SelectObject(dc, handle);
  const BOOL measured = GetTextMetricsW(dc, &result.metrics);
  if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
  if (!measured) return nullptr;
  const float size = static_cast<float>(std::max<LONG>(
    1, result.metrics.tmHeight - result.metrics.tmInternalLeading));
  if (FAILED(renderer().writeFactory->CreateTextFormat(
        description.lfFaceName[0] ? description.lfFaceName : L"Segoe UI", nullptr,
        static_cast<DWRITE_FONT_WEIGHT>(std::max<LONG>(1,
          std::min<LONG>(999, description.lfWeight ? description.lfWeight : FW_NORMAL))),
        description.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"",
        result.format.GetAddressOf()))) return nullptr;
  result.format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  result.format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,
    static_cast<float>(result.metrics.tmHeight),
    static_cast<float>(result.metrics.tmAscent));
  if (cache.size() >= 256) {
    clearLayouts();
    cache.clear();
  }
  return &cache.insert_or_assign(handle, std::move(result)).first->second;
}

bool textLayout(HDC dc, HFONT handle, const std::string& source, LONG width,
    LONG height, UINT flags, ComPtr<IDWriteTextLayout>& layout,
    DWRITE_TEXT_METRICS& metrics)
{
  auto& r = renderer();
  Font* font = fontFor(dc, handle);
  if (!font) return false;
  // Cached layouts are immutable. Alignment, trimming, font generation, and
  // bounds are part of the key, so drawing and measurement cannot mutate one
  // another's cached result. The LRU remains bounded even for live timers.
  LayoutKey key{font->serial, source, width, height, flags & ~DT_NOCLIP};
  auto found = r.layouts.find(key);
  if (found != r.layouts.end()) {
    r.layoutLru.splice(r.layoutLru.begin(), r.layoutLru, found->second.recent);
    layout = found->second.layout;
    metrics = found->second.metrics;
    return true;
  }
  std::wstring text;
  if (!wideText(source, text)) return false;
  if (flags & DT_SINGLELINE)
    for (auto& c : text) if (c == L'\r' || c == L'\n') c = L' ';
  std::vector<UINT32> underline;
  if (!(flags & DT_NOPREFIX)) {
    std::wstring visible;
    visible.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == L'&') {
        if (i + 1 == text.size()) continue;
        if (text[i + 1] != L'&') underline.push_back(static_cast<UINT32>(visible.size()));
        ++i;
      }
      visible.push_back(text[i]);
    }
    text.swap(visible);
  }
  if (FAILED(r.writeFactory->CreateGdiCompatibleTextLayout(
        text.data(), static_cast<UINT32>(text.size()), font->format.Get(),
        static_cast<float>(std::max<LONG>(1, width)),
        static_cast<float>(std::max<LONG>(1, height)), 1.0f, nullptr, FALSE,
        layout.GetAddressOf()))) return false;
  const DWRITE_TEXT_RANGE all{0, static_cast<UINT32>(text.size())};
  if (font->description.lfUnderline) layout->SetUnderline(TRUE, all);
  if (font->description.lfStrikeOut) layout->SetStrikethrough(TRUE, all);
  layout->SetWordWrapping((flags & DT_WORDBREAK) && !(flags & DT_SINGLELINE)
    ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
  for (const auto index : underline) layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{index, 1});
  if (!(flags & DT_CALCRECT)) {
    layout->SetTextAlignment(flags & DT_CENTER ? DWRITE_TEXT_ALIGNMENT_CENTER :
      flags & DT_RIGHT ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    if (flags & DT_SINGLELINE) {
      layout->SetParagraphAlignment(flags & DT_VCENTER ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER :
        flags & DT_BOTTOM ? DWRITE_PARAGRAPH_ALIGNMENT_FAR : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    if (flags & DT_END_ELLIPSIS) {
      ComPtr<IDWriteInlineObject> ellipsis;
      if (FAILED(r.writeFactory->CreateEllipsisTrimmingSign(
            font->format.Get(), ellipsis.GetAddressOf()))) return false;
      const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
      layout->SetTrimming(&trimming, ellipsis.Get());
    }
  }
  if (FAILED(layout->GetMetrics(&metrics))) return false;
  constexpr size_t maxLayouts = 1024;
  constexpr size_t maxTextBytes = 1024 * 1024;
  if (source.size() <= 8192) {
    while (!r.layoutLru.empty() && (r.layouts.size() >= maxLayouts ||
        r.layoutTextBytes + source.size() > maxTextBytes)) {
      const auto& oldest = r.layoutLru.back();
      r.layoutTextBytes -= oldest.text.size();
      r.layouts.erase(oldest);
      r.layoutLru.pop_back();
    }
    r.layoutLru.push_front(key);
    r.layoutTextBytes += source.size();
    r.layouts.emplace(std::move(key), CachedLayout{layout, metrics, r.layoutLru.begin()});
  }
  return true;
}

bool copyGdiState(HDC source, HDC destination)
{
  for (const UINT object : {OBJ_FONT, OBJ_PEN, OBJ_BRUSH}) {
    HGDIOBJ handle = GetCurrentObject(source, object);
    if (handle) SelectObject(destination, handle);
  }
  SetBkMode(destination, GetBkMode(source));
  SetBkColor(destination, GetBkColor(source));
  SetTextColor(destination, GetTextColor(source));
  SetTextAlign(destination, GetTextAlign(source));
  SetTextCharacterExtra(destination, GetTextCharacterExtra(source));
  SetPolyFillMode(destination, GetPolyFillMode(source));
  SetStretchBltMode(destination, GetStretchBltMode(source));
  SetROP2(destination, GetROP2(source));
  SetArcDirection(destination, GetArcDirection(source));
  POINT point{};
  if (GetBrushOrgEx(source, &point)) SetBrushOrgEx(destination, point.x, point.y, nullptr);
  if (GetCurrentPositionEx(source, &point)) MoveToEx(destination, point.x, point.y, nullptr);
  return updateClip() &&
    SelectClipRgn(destination, renderer().cachedRegion) != ERROR;
}
} // namespace

void prepareWindow(HWND hwnd)
{
  // Must run before BeginPaint: target creation/resizing invalidates the whole
  // client so retained partial paints never reveal an uninitialized surface.
  if (!hwnd || renderer().surface) return;
  RECT client{};
  if (GetClientRect(hwnd, &client)) windowSurface(hwnd, client);
}

bool begin(HWND hwnd, HDC dc, const RECT& client)
{
  auto& r = renderer();
  if (r.surface || !defaultCoordinates(dc)) return false;
  auto* surface = windowSurface(hwnd, client);
  if (!surface) return false;
  r.window = hwnd;
  r.proxy = dc;
  r.surface = surface;
  r.client = client;
  r.interopDc = nullptr;
  r.interopSavedState = 0;
  r.failed = false;
  r.clipPushed = false;
  r.emptyClip = false;
  r.clipDirty = true;
  r.clipKind = NULLREGION;
  r.clipBounds = RECT{};
  r.clipGeometry.Reset();
  r.fullPaint = !surface->hasFrame;
  SetRectRgn(r.cachedRegion, 0, 0, 0, 0);
  surface->target->BeginDraw();
  surface->target->SetTransform(D2D1::Matrix3x2F::Identity());
  return true;
}

bool end()
{
  auto& r = renderer();
  if (!r.surface) return false;
  releaseInterop();
  popClip();
  const HRESULT hr = r.surface->target->EndDraw();
  const bool success = !r.failed && SUCCEEDED(hr);
  const HWND hwnd = r.window;
  auto* surface = r.surface;
  r.proxy = nullptr;
  r.window = nullptr;
  r.surface = nullptr;
  r.clipGeometry.Reset();
  if (success) surface->hasFrame = true;
  else {
    discard(*surface);
    // Let one complete legacy frame restore the display before retrying a
    // device that was removed, reset, or rejected GDI interop.
    surface->retryAfter = GetTickCount64() + 2000;
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  return success;
}

bool isActive(HDC dc)
{
  const auto& r = renderer();
  return dc && r.proxy == dc && r.surface;
}

bool needsFullPaint()
{
  return renderer().fullPaint;
}

void releaseWindow(HWND hwnd)
{
  auto& r = renderer();
  if (r.window == hwnd) end();
  r.surfaces.erase(hwnd);
}

HDC gdiDC(HDC dc)
{
  auto& r = renderer();
  if (!isActive(dc) || r.failed) return dc;
  if (!r.interopDc) {
    // GetDC must not be called with a clip/layer pushed on Windows 7.
    popClip();
    if (FAILED(r.surface->interop->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,
          &r.interopDc))) {
      r.interopDc = nullptr;
      r.failed = true;
      return dc;
    }
    r.interopSavedState = SaveDC(r.interopDc);
    if (!r.interopSavedState) {
      r.failed = true;
      releaseInterop();
      return dc;
    }
  }
  if (!copyGdiState(dc, r.interopDc)) {
    r.failed = true;
    releaseInterop();
    return dc;
  }
  return r.interopDc;
}

void flushGdi()
{
  releaseInterop();
}

void invalidateClip(HDC dc)
{
  auto& r = renderer();
  // All proxy clip mutations in the UI translation unit use the bridge.
  // Retain the current clip for already queued commands; update it before
  // the next draw instead of querying the GDI region for every primitive.
  if (dc && dc == r.proxy) r.clipDirty = true;
}

bool lineTo(HDC dc, int x, int y)
{
  if (!isActive(dc) || GetROP2(dc) != R2_COPYPEN) return false;
  LOGPEN selected{};
  POINT start{};
  if (!solidPen(dc, selected) || !GetCurrentPositionEx(dc, &start) ||
      !drawing(dc)) return false;
  if (!renderer().emptyClip && selected.lopnStyle != PS_NULL) {
    auto* target = renderer().surface->target.Get();
    const LONG width = std::max<LONG>(1, std::abs(selected.lopnWidth.x));
    const float pixelOffset = (width & 1) ? 0.5f : 0.0f;
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->DrawLine(
      D2D1::Point2F(start.x + pixelOffset, start.y + pixelOffset),
      D2D1::Point2F(x + pixelOffset, y + pixelOffset),
      brush(selected.lopnColor), static_cast<float>(width));
  }
  // LineTo changes the current position even when its stroke is fully
  // clipped. Keep that Win32 contract on the state-only proxy.
  MoveToEx(dc, x, y, nullptr);
  return true;
}

bool ellipse(HDC dc, int left, int top, int right, int bottom)
{
  if (!isActive(dc) || GetROP2(dc) != R2_COPYPEN ||
      right <= left || bottom <= top) return false;
  LOGPEN pen{};
  LOGBRUSH fill{};
  if (!solidPen(dc, pen) || !solidBrush(dc, fill) || !drawing(dc)) return false;
  if (renderer().emptyClip) return true;
  auto* target = renderer().surface->target.Get();
  target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  const D2D1_ELLIPSE shape = D2D1::Ellipse(
    D2D1::Point2F((static_cast<float>(left) + right) * 0.5f,
      (static_cast<float>(top) + bottom) * 0.5f),
    (right - left - 1) * 0.5f, (bottom - top - 1) * 0.5f);
  if (fill.lbStyle != BS_NULL) target->FillEllipse(shape, brush(fill.lbColor));
  if (pen.lopnStyle != PS_NULL) target->DrawEllipse(shape, brush(pen.lopnColor),
    static_cast<float>(std::max<LONG>(1, std::abs(pen.lopnWidth.x))));
  return true;
}

bool fillRect(HDC dc, const RECT& rect, COLORREF value)
{
  if (!drawing(dc)) return false;
  if (renderer().emptyClip || rect.right <= rect.left || rect.bottom <= rect.top) return true;
  auto* target = renderer().surface->target.Get();
  target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  target->FillRectangle(rectangle(rect), brush(value));
  return true;
}

bool fillRoundRect(HDC dc, const RECT& rect,
    COLORREF fill, COLORREF border, int radius)
{
  if (!drawing(dc)) return false;
  if (renderer().emptyClip || rect.right <= rect.left || rect.bottom <= rect.top) return true;
  auto* target = renderer().surface->target.Get();
  if (radius <= 0) {
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->FillRectangle(rectangle(rect), brush(fill));
    if (fill != border) target->DrawRectangle(rectangle(rect, 0.5f), brush(border), 1.0f);
  } else {
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    const auto rounded = roundedRectangle(rect, radius);
    target->FillRoundedRectangle(rounded, brush(fill));
    target->DrawRoundedRectangle(rounded, brush(border), 1.0f);
  }
  return true;
}

bool strokeRoundRect(HDC dc, const RECT& rect, COLORREF value, int radius)
{
  if (!drawing(dc)) return false;
  if (renderer().emptyClip || rect.right <= rect.left || rect.bottom <= rect.top) return true;
  auto* target = renderer().surface->target.Get();
  if (radius > 0) {
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->DrawRoundedRectangle(roundedRectangle(rect, radius), brush(value), 1.0f);
  } else {
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->DrawRectangle(rectangle(rect, 0.5f), brush(value), 1.0f);
  }
  return true;
}

bool fillGradient(HDC dc, const RECT& rect,
    COLORREF first, COLORREF last, bool horizontal)
{
  if (!drawing(dc)) return false;
  if (renderer().emptyClip || rect.right <= rect.left || rect.bottom <= rect.top) return true;
  auto& surface = *renderer().surface;
  auto* target = surface.target.Get();
  const uint64_t key = (static_cast<uint64_t>(first & 0xffffff) << 24) |
    static_cast<uint64_t>(last & 0xffffff);
  auto found = surface.gradients.find(key);
  if (found == surface.gradients.end()) {
    const D2D1_GRADIENT_STOP stops[] = {{0.0f, color(first)}, {1.0f, color(last)}};
    ComPtr<ID2D1GradientStopCollection> collection;
    if (FAILED(target->CreateGradientStopCollection(stops, 2,
          D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, collection.GetAddressOf()))) return false;
    GradientBrush entry;
    if (FAILED(target->CreateLinearGradientBrush(
          D2D1::LinearGradientBrushProperties(D2D1::Point2F(), D2D1::Point2F()),
          collection.Get(), entry.brush.GetAddressOf()))) return false;
    // A visible playlist can have several independent gradients per row.
    // Keep enough custom color pairs for that frame without cycling the
    // cache, while bounding resources under animated/theme-driven colors.
    if (surface.gradients.size() >= 256) {
      const auto oldest = std::min_element(surface.gradients.begin(),
        surface.gradients.end(), [](const auto& a, const auto& b) {
          return a.second.lastUsed < b.second.lastUsed;
        });
      surface.gradients.erase(oldest);
    }
    found = surface.gradients.emplace(key, std::move(entry)).first;
  }
  auto* gradient = found->second.brush.Get();
  found->second.lastUsed = ++surface.gradientClock;
  gradient->SetStartPoint(D2D1::Point2F(static_cast<float>(rect.left),
    static_cast<float>(rect.top)));
  gradient->SetEndPoint(D2D1::Point2F(static_cast<float>(horizontal ? rect.right : rect.left),
    static_cast<float>(horizontal ? rect.top : rect.bottom)));
  target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  target->FillRectangle(rectangle(rect), gradient);
  return true;
}

bool drawText(HDC dc, const std::string& source, RECT& rect,
    UINT flags, COLORREF value, HFONT handle)
{
  constexpr UINT supported = DT_CENTER | DT_RIGHT | DT_VCENTER | DT_BOTTOM |
    DT_WORDBREAK | DT_SINGLELINE | DT_NOCLIP | DT_CALCRECT | DT_NOPREFIX |
    DT_END_ELLIPSIS;
  if ((flags & ~supported) || !isActive(dc)) return false;
  ComPtr<IDWriteTextLayout> layout;
  const bool measuring = (flags & DT_CALCRECT) != 0;
  const LONG width = measuring && (flags & DT_SINGLELINE) ? 1000000 :
    std::max<LONG>(1, rect.right - rect.left);
  const LONG height = measuring ? 1000000 : std::max<LONG>(1, rect.bottom - rect.top);
  DWRITE_TEXT_METRICS metrics{};
  if (!textLayout(dc, handle, source, width, height, flags, layout, metrics)) return false;
  if (measuring) {
    rect.right = rect.left + static_cast<LONG>(std::ceil(metrics.widthIncludingTrailingWhitespace));
    rect.bottom = rect.top + static_cast<LONG>(std::ceil(metrics.height));
    return true;
  }
  if (!drawing(dc)) return false;
  if (renderer().emptyClip || source.empty()) return true;
  renderer().surface->target->DrawTextLayout(
    D2D1::Point2F(static_cast<float>(rect.left), static_cast<float>(rect.top)),
    layout.Get(), brush(value), flags & DT_NOCLIP ?
      D2D1_DRAW_TEXT_OPTIONS_NONE : D2D1_DRAW_TEXT_OPTIONS_CLIP);
  return true;
}

bool measureText(HDC dc, const std::string& source, HFONT handle,
    int& width, int& height)
{
  if (!isActive(dc) || !factories()) return false;
  ComPtr<IDWriteTextLayout> layout;
  DWRITE_TEXT_METRICS metrics{};
  if (!textLayout(dc, handle, source, 1000000, 1000000,
        DT_CALCRECT | DT_NOPREFIX, layout, metrics)) return false;
  width = static_cast<int>(std::ceil(metrics.widthIncludingTrailingWhitespace));
  height = static_cast<int>(std::ceil(metrics.height));
  return true;
}

void registerFont(HFONT handle, const char*, int, int, bool)
{
  // A real HFONT exposes its exact face, style, and GDI cell/character height.
  // Do not reinterpret positive CreateFont heights as DirectWrite em sizes.
  releaseFont(handle);
}

void releaseFont(HFONT handle)
{
  auto& fonts = renderer().fonts;
  const auto found = fonts.find(handle);
  if (found != fonts.end()) {
    eraseFontLayouts(found->second.serial);
    fonts.erase(found);
  }
}

void shutdown()
{
  auto& r = renderer();
  if (r.surface) end();
  clearLayouts();
  r.fonts.clear();
  r.surfaces.clear();
  r.clipGeometry.Reset();
  r.writeFactory.Reset();
  r.factory.Reset();
  if (r.scratchRegion) DeleteObject(r.scratchRegion);
  if (r.auxiliaryRegion) DeleteObject(r.auxiliaryRegion);
  if (r.cachedRegion) DeleteObject(r.cachedRegion);
  r.scratchRegion = r.auxiliaryRegion = r.cachedRegion = nullptr;
  r.attemptedFactory = false;
}
} // namespace NativeUiRenderer

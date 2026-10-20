#pragma once
#include "native_ui_renderer.h"

// This bridge is deliberately included AFTER all platform/WDL headers, in the
// UI translation unit only. GDI continues to hold layout/clip/selection state.
// Raster operations borrow the renderer's interop DC during a native frame;
// other windows and offscreen image caches retain their original behavior.
namespace NativeUiRendererBridge {
class Frame {
public:
  Frame(HWND hwnd, HDC dc, const RECT& client)
    : active_(NativeUiRenderer::begin(hwnd, dc, client)), started_(active_) {}
  ~Frame() { finish(); }
  Frame(const Frame&) = delete;
  Frame& operator=(const Frame&) = delete;
  bool presentsWindow() const {
#ifdef _WIN32
    return started_;
#else
    return false;
#endif
  }
  bool succeeded() const { return succeeded_; }
  void finish() {
    if (active_) { succeeded_ = NativeUiRenderer::end(); active_ = false; }
  }
private:
  bool active_ = false;
  bool started_ = false;
  bool succeeded_ = false;
};

inline HFONT createFont(int height, int width, int escapement, int orientation,
  int weight, DWORD italic, DWORD underline, DWORD strikeout, DWORD charset,
  DWORD output, DWORD clip, DWORD quality, DWORD pitch, const char* family)
{
  HFONT font = CreateFont(height, width, escapement, orientation, weight,
    italic, underline, strikeout, charset, output, clip, quality, pitch, family);
  // Rotated/underlined fonts retain the platform text path.
  if (!escapement && !orientation && !underline && !strikeout)
    NativeUiRenderer::registerFont(font, family, height ? height : width,
      weight, italic != 0);
  return font;
}

inline auto deleteObject(HGDIOBJ object) -> decltype(DeleteObject(object))
{
  // A restored proxy selection must also be released by the borrowed GDI DC
  // before Windows can delete its former font/brush/pen.
  NativeUiRenderer::flushGdi();
  NativeUiRenderer::releaseFont(static_cast<HFONT>(object));
  return DeleteObject(object);
}

#ifdef _WIN32
template<class... Args> inline auto fillRect(HDC dc, Args... args)
  -> decltype(::FillRect(dc, args...))
{ return ::FillRect(NativeUiRenderer::gdiDC(dc), args...); }
template<class... Args> inline auto roundRect(HDC dc, Args... args)
  -> decltype(::RoundRect(dc, args...))
{ return ::RoundRect(NativeUiRenderer::gdiDC(dc), args...); }
inline BOOL ellipse(HDC dc, int left, int top, int right, int bottom)
{
  if (NativeUiRenderer::ellipse(dc, left, top, right, bottom)) return TRUE;
  return ::Ellipse(NativeUiRenderer::gdiDC(dc), left, top, right, bottom);
}
template<class... Args> inline auto bitBlt(HDC dc, Args... args)
  -> decltype(::BitBlt(dc, args...))
{ return ::BitBlt(NativeUiRenderer::gdiDC(dc), args...); }
template<class... Args> inline auto stretchBlt(HDC dc, Args... args)
  -> decltype(::StretchBlt(dc, args...))
{ return ::StretchBlt(NativeUiRenderer::gdiDC(dc), args...); }
template<class... Args> inline auto stretchDIBits(HDC dc, Args... args)
  -> decltype(::StretchDIBits(dc, args...))
{ return ::StretchDIBits(NativeUiRenderer::gdiDC(dc), args...); }

inline BOOL lineTo(HDC dc, int x, int y)
{
  if (NativeUiRenderer::lineTo(dc, x, y)) return TRUE;
  HDC target = NativeUiRenderer::gdiDC(dc);
  const BOOL result = ::LineTo(target, x, y);
  if (target != dc && result) ::MoveToEx(dc, x, y, nullptr);
  return result;
}

inline int intersectClipRect(HDC dc, int left, int top, int right, int bottom)
{
  const int result = ::IntersectClipRect(dc, left, top, right, bottom);
  NativeUiRenderer::invalidateClip(dc);
  return result;
}
inline int selectClipRgn(HDC dc, HRGN region)
{
  const int result = ::SelectClipRgn(dc, region);
  NativeUiRenderer::invalidateClip(dc);
  return result;
}
inline int extSelectClipRgn(HDC dc, HRGN region, int mode)
{
  const int result = ::ExtSelectClipRgn(dc, region, mode);
  NativeUiRenderer::invalidateClip(dc);
  return result;
}
inline BOOL restoreDC(HDC dc, int saved)
{
  const BOOL result = ::RestoreDC(dc, saved);
  NativeUiRenderer::invalidateClip(dc);
  return result;
}

inline int drawTextW(HDC dc, LPCWSTR text, int length, LPRECT rect, UINT flags)
{
  return ::DrawTextW((flags & DT_CALCRECT) ? dc : NativeUiRenderer::gdiDC(dc),
    text, length, rect, flags);
}
inline int drawTextA(HDC dc, LPCSTR text, int length, LPRECT rect, UINT flags)
{
  return ::DrawTextA((flags & DT_CALCRECT) ? dc : NativeUiRenderer::gdiDC(dc),
    text, length, rect, flags);
}
#endif
}

#undef CreateFont
#define CreateFont NativeUiRendererBridge::createFont
#define DeleteObject NativeUiRendererBridge::deleteObject
#ifdef _WIN32
#define FillRect NativeUiRendererBridge::fillRect
#define RoundRect NativeUiRendererBridge::roundRect
#define Ellipse NativeUiRendererBridge::ellipse
#define BitBlt NativeUiRendererBridge::bitBlt
#define StretchBlt NativeUiRendererBridge::stretchBlt
#define StretchDIBits NativeUiRendererBridge::stretchDIBits
#define LineTo NativeUiRendererBridge::lineTo
#define IntersectClipRect NativeUiRendererBridge::intersectClipRect
#define SelectClipRgn NativeUiRendererBridge::selectClipRgn
#define ExtSelectClipRgn NativeUiRendererBridge::extSelectClipRgn
#define RestoreDC NativeUiRendererBridge::restoreDC
#define DrawTextW NativeUiRendererBridge::drawTextW
#define DrawTextA NativeUiRendererBridge::drawTextA
#endif

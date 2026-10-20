#pragma once
#include <string>

// Include after Win32 / SWELL declarations. Coordinates remain the existing
// client coordinates: rendering never changes layout, hit tests or user settings.
namespace NativeUiRenderer {
void prepareWindow(HWND hwnd);
bool begin(HWND hwnd, HDC dc, const RECT& client);
bool end();
bool isActive(HDC dc);
bool needsFullPaint();
void releaseWindow(HWND hwnd);

// Windows uses the supplied HDC as a state/clip proxy for a window render
// target. Legacy drawing obtains the interop DC here, preserving draw order.
// On macOS and outside a frame this returns the original HDC.
HDC gdiDC(HDC dc);
void flushGdi();
// Call after changing the active proxy's clipping state. The bridge handles
// legacy clipping APIs; damage regions established outside it notify once.
void invalidateClip(HDC dc);
#ifdef _WIN32
bool lineTo(HDC dc, int x, int y);
bool ellipse(HDC dc, int left, int top, int right, int bottom);
#endif

bool fillRect(HDC dc, const RECT& rect, COLORREF color);
bool fillRoundRect(HDC dc, const RECT& rect,
  COLORREF fill, COLORREF border, int radius);
bool strokeRoundRect(HDC dc, const RECT& rect, COLORREF color, int radius);
bool fillGradient(HDC dc, const RECT& rect,
  COLORREF start, COLORREF end, bool horizontal = false);
bool drawText(HDC dc, const std::string& text, RECT& rect,
  UINT flags, COLORREF color, HFONT font);
bool measureText(HDC dc, const std::string& text, HFONT font,
  int& width, int& height);
void registerFont(HFONT font, const char* family, int size,
  int weight, bool italic = false);
void releaseFont(HFONT font);
void shutdown();
}

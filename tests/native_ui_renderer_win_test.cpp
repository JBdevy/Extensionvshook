#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/native_ui_renderer.h"
#include "../src/native_ui_renderer_bridge.h"

// Exercises the production renderer without loading REAPER. The HWND is never
// shown. Its memory DC only supplies coordinates and clipping; pixels are read
// through the renderer's GDI interop surface, not through that proxy DC.
static void require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

struct Surface {
  HDC dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ previous = nullptr;
  uint32_t* pixels = nullptr;
  int width = 0;
  int height = 0;

  Surface(int w, int h) : width(w), height(h)
  {
    dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* data = nullptr;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
    require(dc && bitmap && data, "Could not create test DIB");
    pixels = static_cast<uint32_t*>(data);
    previous = SelectObject(dc, bitmap);
    std::fill(pixels, pixels + width * height, 0x001d2735);
  }
  ~Surface()
  {
    if (previous) SelectObject(dc, previous);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
  }
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  COLORREF pixel(int x, int y) const
  {
    const uint32_t value = pixels[y * width + x];
    return RGB((value >> 16) & 255, (value >> 8) & 255, value & 255);
  }
  void capture(HDC proxy)
  {
    HDC actual = NativeUiRenderer::gdiDC(proxy);
    require(actual && actual != proxy, "GPU frame did not expose its interop DC");
    require(BitBlt(dc, 0, 0, width, height, actual, 0, 0, SRCCOPY) != FALSE,
      "Could not capture the actual rendered surface");
    GdiFlush();
  }
  void save(const char* path) const
  {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = -height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biSizeImage = static_cast<DWORD>(width * height * 4);
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + info.biSizeImage;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    out.write(reinterpret_cast<const char*>(pixels), info.biSizeImage);
    require(static_cast<bool>(out), "Could not save renderer fixture BMP");
  }
};

static void legacyFill(HDC dc, RECT rect, COLORREF color)
{
  HBRUSH brush = CreateSolidBrush(color);
  require(brush != nullptr, "Could not create legacy GDI brush");
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

static void solid(HDC proxy, RECT rect, COLORREF color)
{
  require(NativeUiRenderer::fillRect(proxy, rect, color),
    "Native solid fill failed");
}

static void text(HDC proxy, HFONT font, RECT rect,
  const std::string& value, COLORREF color, UINT flags = DT_LEFT)
{
  require(NativeUiRenderer::drawText(proxy, value, rect,
    flags | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, color, font),
    "DirectWrite text draw failed");
}

static void renderFixture(HWND hwnd, Surface& proxy, Surface& capture, HFONT font)
{
  const RECT client{0, 0, proxy.width, proxy.height};
  require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Fixture begin failed");
  solid(proxy.dc, client, RGB(13, 18, 27));
  RECT title{18, 12, 460, 42};
  text(proxy.dc, font, title, "VS Hook / renderer fixture", RGB(235, 242, 252));
  RECT note{18, 42, 460, 66};
  text(proxy.dc, font, note, "Direct2D + DirectWrite", RGB(125, 157, 193));
  const COLORREF edge = RGB(59, 76, 99);
  for (int i = 0; i < 3; ++i) {
    RECT button{18 + i * 149, 80, 157 + i * 149, 116};
    require(NativeUiRenderer::fillRoundRect(proxy.dc, button,
      i == 0 ? RGB(32, 112, 179) : RGB(26, 37, 53), edge, 7),
      "Rounded button failed");
    text(proxy.dc, font, button, i == 0 ? "Músicas" : i == 1 ? "Mixer" : "Parts",
      RGB(237, 243, 252), DT_CENTER);
  }
  RECT list{18, 130, 462, 239};
  require(NativeUiRenderer::fillRoundRect(proxy.dc, list,
    RGB(19, 28, 41), edge, 8), "Rounded panel failed");
  require(NativeUiRenderer::strokeRoundRect(proxy.dc, list, edge, 8),
    "Rounded panel border failed");
  for (int i = 0; i < 3; ++i) {
    RECT row{28, 137 + 31 * i, 451, 165 + 31 * i};
    if (i == 0) solid(proxy.dc, row, RGB(24, 65, 85));
    text(proxy.dc, font, row,
      i == 0 ? "01   Música selecionada" :
      i == 1 ? "02   São Paulo — Próxima música" : "03   Ω • Texto Unicode",
      i == 0 ? RGB(107, 223, 205) : RGB(208, 220, 237));
  }
  RECT track{18, 252, 462, 268};
  require(NativeUiRenderer::fillRoundRect(proxy.dc, track,
    RGB(32, 44, 61), RGB(32, 44, 61), 7), "Progress track failed");
  RECT progress = track;
  progress.right = 310;
  require(NativeUiRenderer::fillGradient(proxy.dc, progress,
    RGB(35, 154, 205), RGB(74, 217, 183), true), "Progress gradient failed");
  RECT caption{18, 280, 462, 308};
  text(proxy.dc, font, caption, "Progresso   02:18 / 03:42", RGB(182, 203, 226));
  capture.capture(proxy.dc);
  require(NativeUiRenderer::end(), "Fixture presentation failed");
}

int main(int argc, char** argv)
{
  HWND hwnd = nullptr;
  HFONT font = nullptr;
  try {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"VSHookRendererSmokeTest";
    require(RegisterClassW(&windowClass) != 0, "Could not register hidden window");
    hwnd = CreateWindowExW(0, windowClass.lpszClassName, L"VS Hook renderer test",
      WS_POPUP, 0, 0, 480, 320, nullptr, nullptr, windowClass.hInstance, nullptr);
    require(hwnd != nullptr, "Could not create hidden render window");
    NativeUiRenderer::prepareWindow(hwnd);
    Surface proxy(480, 320), capture(480, 320);
    const RECT client{0, 0, 480, 320};
    const COLORREF base = RGB(8, 13, 21);
    const COLORREF red = RGB(228, 48, 62);
    const COLORREF green = RGB(40, 208, 126);
    const COLORREF blue = RGB(32, 115, 236);
    const COLORREF orange = RGB(241, 151, 39);
    require(!NativeUiRenderer::isActive(proxy.dc), "Renderer active outside a frame");
    require(NativeUiRenderer::gdiDC(proxy.dc) == proxy.dc,
      "GDI fallback outside a frame must preserve the caller DC");
    require(!NativeUiRenderer::fillRect(proxy.dc, client, base),
      "Native fill outside a frame must return false for the legacy fallback");

    // Interleaving native and legacy drawing must preserve exact painter order.
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Initial begin failed");
    require(NativeUiRenderer::needsFullPaint(), "First frame must request a full paint");
    require(NativeUiRenderer::isActive(proxy.dc), "Frame did not activate proxy");
    solid(proxy.dc, client, base);
    solid(proxy.dc, RECT{12, 12, 72, 72}, red);
    legacyFill(proxy.dc, RECT{32, 32, 92, 92}, green);
    solid(proxy.dc, RECT{52, 52, 112, 112}, blue);
    capture.capture(proxy.dc);
    require(capture.pixel(20, 20) == red, "Native fill was lost before GDI drawing");
    require(capture.pixel(40, 40) == green, "GDI draw order was not preserved");
    require(capture.pixel(60, 60) == blue, "Native draw did not cover earlier GDI");
    require(capture.pixel(200, 30) == base, "Unexpected background corruption");
    require(NativeUiRenderer::end(), "Initial presentation failed");

    // The union of two separate dirty regions must not become its bounding box.
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Retained begin failed");
    require(!NativeUiRenderer::needsFullPaint(), "Second frame unexpectedly lost content");
    HRGN left = CreateRectRgn(10, 130, 70, 180);
    HRGN right = CreateRectRgn(160, 130, 230, 180);
    require(left && right, "Could not create disjoint test regions");
    CombineRgn(left, left, right, RGN_OR);
    SelectClipRgn(proxy.dc, left);
    solid(proxy.dc, RECT{0, 120, 240, 190}, orange);
    legacyFill(proxy.dc, RECT{0, 140, 240, 160}, green);
    solid(proxy.dc, RECT{0, 160, 240, 170}, blue);
    SelectClipRgn(proxy.dc, nullptr);
    DeleteObject(left);
    DeleteObject(right);
    capture.capture(proxy.dc);
    require(capture.pixel(20, 135) == orange && capture.pixel(170, 135) == orange,
      "Native fill did not honor both dirty rectangles");
    require(capture.pixel(20, 150) == green && capture.pixel(170, 150) == green,
      "Legacy drawing did not honor both dirty rectangles");
    require(capture.pixel(20, 165) == blue && capture.pixel(170, 165) == blue,
      "Native drawing lost clipping after GDI interop");
    require(capture.pixel(120, 135) == base && capture.pixel(120, 150) == base &&
        capture.pixel(120, 165) == base,
      "Disjoint dirty-region gap was overwritten");
    require(capture.pixel(40, 40) == green, "Retained content outside damage was lost");
    require(NativeUiRenderer::end(), "Disjoint clip presentation failed");

    // A progress bar moving backwards must erase its old tail, preserving the
    // independent indicator beside it and every pixel outside the dirty area.
    const RECT track{10, 210, 260, 224};
    const COLORREF trackColor = RGB(44, 52, 66);
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Progress begin failed");
    solid(proxy.dc, track, trackColor);
    solid(proxy.dc, RECT{10, 210, 230, 224}, green);
    solid(proxy.dc, RECT{300, 12, 460, 65}, orange);
    require(NativeUiRenderer::end(), "Progress initial presentation failed");
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Progress update begin failed");
    int saved = SaveDC(proxy.dc);
    require(saved != 0, "Could not save clipping state");
    IntersectClipRect(proxy.dc, track.left, track.top, track.right, track.bottom);
    solid(proxy.dc, track, trackColor);
    solid(proxy.dc, RECT{10, 210, 70, 224}, green);
    RestoreDC(proxy.dc, saved);
    capture.capture(proxy.dc);
    require(capture.pixel(30, 215) == green && capture.pixel(150, 215) == trackColor,
      "Shrinking progress left a stale filled tail");
    require(capture.pixel(320, 30) == orange && capture.pixel(40, 40) == green,
      "Partial progress update damaged retained controls");
    require(NativeUiRenderer::end(), "Progress update presentation failed");

    // Nested SWELL/Win32-style SaveDC clipping is observed on every draw and is
    // restored when the inner scope ends.
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Nested clip begin failed");
    saved = SaveDC(proxy.dc);
    IntersectClipRect(proxy.dc, 280, 100, 460, 220);
    solid(proxy.dc, client, orange);
    int nested = SaveDC(proxy.dc);
    IntersectClipRect(proxy.dc, 300, 130, 330, 160);
    solid(proxy.dc, client, blue);
    RestoreDC(proxy.dc, nested);
    solid(proxy.dc, RECT{280, 180, 460, 195}, red);
    RestoreDC(proxy.dc, saved);
    capture.capture(proxy.dc);
    require(capture.pixel(310, 140) == blue && capture.pixel(350, 140) == orange,
      "Inner clipping scope was not applied");
    require(capture.pixel(400, 185) == red && capture.pixel(270, 140) == base,
      "Outer clipping scope was not restored");
    require(NativeUiRenderer::end(), "Nested clip presentation failed");

    // The production bridge keeps GDI state in the original proxy, including
    // line current position and cached-image sources, while routing its output.
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Bridge begin failed");
    saved = SaveDC(proxy.dc);
    HPEN pen = CreatePen(PS_SOLID, 1, green);
    require(pen != nullptr, "Could not create legacy test pen");
    HGDIOBJ oldPen = SelectObject(proxy.dc, pen);
    MoveToEx(proxy.dc, 300, 80, nullptr);
    require(LineTo(proxy.dc, 350, 80) != FALSE, "First bridged line failed");
    require(LineTo(proxy.dc, 350, 95) != FALSE, "Second bridged line failed");
    POINT current{};
    require(GetCurrentPositionEx(proxy.dc, &current) && current.x == 350 && current.y == 95,
      "Legacy LineTo did not preserve proxy current position");
    SelectObject(proxy.dc, oldPen);
    RestoreDC(proxy.dc, saved);
    require(DeleteObject(pen) != FALSE,
      "Temporary GDI pen remained selected in the borrowed renderer DC");
    Surface cachedImage(8, 8);
    legacyFill(cachedImage.dc, RECT{0, 0, 8, 8}, blue);
    require(BitBlt(proxy.dc, 390, 80, 8, 8, cachedImage.dc, 0, 0, SRCCOPY) != FALSE,
      "Cached-image BitBlt routing failed");
    const uint32_t samplePixels[4] = {0x00e4303e, 0x00e4303e, 0x00e4303e, 0x00e4303e};
    BITMAPINFO sampleInfo{};
    sampleInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sampleInfo.bmiHeader.biWidth = 2;
    sampleInfo.bmiHeader.biHeight = -2;
    sampleInfo.bmiHeader.biPlanes = 1;
    sampleInfo.bmiHeader.biBitCount = 32;
    require(StretchDIBits(proxy.dc, 410, 80, 12, 12, 0, 0, 2, 2,
      samplePixels, &sampleInfo, DIB_RGB_COLORS, SRCCOPY) != GDI_ERROR,
      "Splash-style StretchDIBits routing failed");
    solid(proxy.dc, RECT{415, 85, 422, 92}, orange);
    capture.capture(proxy.dc);
    require(capture.pixel(320, 80) == green && capture.pixel(350, 90) == green,
      "Sequential legacy lines used stale current-position state");
    require(capture.pixel(393, 83) == blue && capture.pixel(411, 81) == red &&
      capture.pixel(418, 88) == orange, "Legacy bitmap/native draw ordering failed");
    require(NativeUiRenderer::end(), "Bridge presentation failed");

    // Reusing a gradient color pair must update its direction and coordinates,
    // and cache eviction must not invalidate gradients already queued to draw.
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Gradient cache begin failed");
    solid(proxy.dc, client, base);
    require(NativeUiRenderer::fillGradient(proxy.dc, RECT{10, 20, 70, 80}, red, blue),
      "First cached gradient failed");
    require(NativeUiRenderer::fillGradient(proxy.dc, RECT{90, 110, 150, 170}, red, blue),
      "Translated cached gradient failed");
    require(NativeUiRenderer::fillGradient(proxy.dc, RECT{180, 20, 240, 80}, red, blue, true),
      "Horizontal cached gradient failed");
    require(NativeUiRenderer::fillGradient(proxy.dc, RECT{260, 20, 320, 80}, green, orange),
      "Different gradient colors failed");
    for (int i = 0; i < 280; ++i) {
      const int x = 10 + (i % 70) * 6;
      const int y = 190 + (i / 70) * 6;
      require(NativeUiRenderer::fillGradient(proxy.dc, RECT{x, y, x + 5, y + 5},
        RGB(i & 255, (i >> 8) * 123, 37), RGB(17, (i * 41) & 255, 240)),
        "Gradient cache eviction failed");
    }
    require(NativeUiRenderer::fillGradient(proxy.dc, RECT{340, 110, 400, 170}, red, blue),
      "Evicted gradient could not be recreated");
    capture.capture(proxy.dc);
    require(capture.pixel(40, 30) == capture.pixel(120, 120) &&
      capture.pixel(40, 30) == capture.pixel(190, 50) &&
      capture.pixel(40, 30) == capture.pixel(370, 120),
      "Cached gradient retained stale coordinates or orientation");
    require(GetRValue(capture.pixel(40, 30)) > GetRValue(capture.pixel(40, 70)) &&
      GetBValue(capture.pixel(40, 30)) < GetBValue(capture.pixel(40, 70)),
      "Gradient colors do not follow their endpoints");
    require(capture.pixel(290, 30) != capture.pixel(40, 30),
      "Gradient cache reused another color pair");
    require(NativeUiRenderer::end(), "Gradient cache presentation failed");

    // Native ornaments preserve selected brush/pen behavior. Unsupported
    // styles retain GDI fallback, including its line-position bookkeeping.
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Ornaments begin failed");
    solid(proxy.dc, client, base);
    saved = SaveDC(proxy.dc);
    HPEN solidPen = CreatePen(PS_SOLID, 1, red);
    HPEN dashedPen = CreatePen(PS_DASH, 1, green);
    HBRUSH solidBrush = CreateSolidBrush(green);
    require(solidPen && dashedPen && solidBrush, "Could not create ornament objects");
    SelectObject(proxy.dc, solidPen);
    SelectObject(proxy.dc, solidBrush);
    require(NativeUiRenderer::ellipse(proxy.dc, 10, 20, 70, 80),
      "Solid ellipse did not use native rendering");
    SelectObject(proxy.dc, GetStockObject(NULL_BRUSH));
    require(NativeUiRenderer::ellipse(proxy.dc, 90, 20, 150, 80),
      "Hollow ellipse did not use native rendering");
    SelectObject(proxy.dc, GetStockObject(NULL_PEN));
    SelectObject(proxy.dc, solidBrush);
    require(NativeUiRenderer::ellipse(proxy.dc, 170, 20, 230, 80),
      "Ellipse with null pen did not use native rendering");
    MoveToEx(proxy.dc, 10, 110, nullptr);
    require(NativeUiRenderer::lineTo(proxy.dc, 110, 110),
      "Null-pen line should be handled without rasterizing");
    GetCurrentPositionEx(proxy.dc, &current);
    require(current.x == 110 && current.y == 110,
      "Null-pen line did not update current position");
    SelectObject(proxy.dc, dashedPen);
    SelectObject(proxy.dc, GetStockObject(NULL_BRUSH));
    require(!NativeUiRenderer::ellipse(proxy.dc, 250, 20, 310, 80),
      "Dashed ellipse must use the legacy fallback");
    require(Ellipse(proxy.dc, 250, 20, 310, 80) != FALSE,
      "Dashed ellipse legacy fallback failed");
    MoveToEx(proxy.dc, 10, 140, nullptr);
    require(!NativeUiRenderer::lineTo(proxy.dc, 110, 140),
      "Dashed line must use the legacy fallback");
    require(LineTo(proxy.dc, 110, 140) != FALSE, "Dashed line legacy fallback failed");
    SelectObject(proxy.dc, solidPen);
    SetROP2(proxy.dc, R2_XORPEN);
    MoveToEx(proxy.dc, 10, 160, nullptr);
    require(!NativeUiRenderer::lineTo(proxy.dc, 110, 160),
      "XOR line must use the legacy fallback");
    require(LineTo(proxy.dc, 110, 160) != FALSE, "XOR line legacy fallback failed");
    SetROP2(proxy.dc, R2_COPYPEN);
    // A native clip change after the GDI lease, followed by restoring a saved
    // clip, must invalidate the cached region in both drawing paths.
    const int ornamentClip = SaveDC(proxy.dc);
    IntersectClipRect(proxy.dc, 350, 20, 370, 80);
    SelectObject(proxy.dc, solidBrush);
    require(NativeUiRenderer::ellipse(proxy.dc, 330, 20, 390, 80),
      "Clipped native ellipse failed");
    RestoreDC(proxy.dc, ornamentClip);
    MoveToEx(proxy.dc, 330, 100, nullptr);
    require(NativeUiRenderer::lineTo(proxy.dc, 390, 100),
      "Native line after restoring the clip failed");
    capture.capture(proxy.dc);
    require(capture.pixel(40, 50) == green && capture.pixel(120, 50) == base &&
      capture.pixel(200, 50) == green, "Ellipse brush/pen semantics changed");
    require(capture.pixel(50, 110) == base, "Null-pen line drew unexpected pixels");
    require(capture.pixel(280, 50) == base, "Hollow fallback ellipse filled its center");
    require(capture.pixel(360, 50) == green && capture.pixel(340, 50) == base,
      "Native ornament ignored changed clipping after GDI fallback");
    require(capture.pixel(340, 100) == red && capture.pixel(380, 100) == red,
      "Restored clip was not applied to native line");
    RestoreDC(proxy.dc, saved);
    require(DeleteObject(solidPen) && DeleteObject(dashedPen) && DeleteObject(solidBrush),
      "Ornament objects remained selected in the borrowed DC");
    require(NativeUiRenderer::end(), "Ornaments presentation failed");

    font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    require(font != nullptr, "Could not create text font");
    NativeUiRenderer::registerFont(font, "Segoe UI", 18, FW_NORMAL);
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Text begin failed");
    solid(proxy.dc, RECT{0, 240, 480, 320}, base);
    int measuredWidth = 0, measuredHeight = 0;
    require(NativeUiRenderer::measureText(proxy.dc, "Músicas — São Paulo • Ω",
      font, measuredWidth, measuredHeight) && measuredWidth > 50 && measuredHeight > 0,
      "UTF-8 text measurement failed");
    text(proxy.dc, font, RECT{16, 244, 460, 273},
      "Músicas — São Paulo • Ω", RGB(245, 248, 255));
    text(proxy.dc, font, RECT{16, 279, 178, 307},
      "Uma música com nome muito longo para a área disponível",
      RGB(245, 248, 255), DT_LEFT | DT_END_ELLIPSIS);
    capture.capture(proxy.dc);
    int ink = 0;
    for (int y = 244; y < 273; ++y)
      for (int x = 16; x < 460; ++x)
        if (capture.pixel(x, y) != base) ++ink;
    require(ink > 150, "Unicode text produced no visible glyphs");
    for (int y = 279; y < 307; ++y)
      for (int x = 181; x < 480; ++x)
        require(capture.pixel(x, y) == base, "Ellipsized text escaped its layout bounds");
    require(NativeUiRenderer::end(), "Text presentation failed");

    renderFixture(hwnd, proxy, capture, font);
    if (argc > 1) {
      capture.save(argv[1]);
      std::cout << "Renderer fixture (not an application screenshot): " << argv[1] << '\n';
    }
    NativeUiRenderer::releaseFont(font);
    DeleteObject(font);
    font = CreateFontW(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    require(font != nullptr, "Could not recreate text font");
    NativeUiRenderer::registerFont(font, "Segoe UI", 30, FW_BOLD);
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Font reuse begin failed");
    int largerWidth = 0, largerHeight = 0;
    require(NativeUiRenderer::measureText(proxy.dc, "Músicas — São Paulo • Ω",
      font, largerWidth, largerHeight) && largerHeight > measuredHeight &&
      largerWidth > measuredWidth, "Font cache kept stale metrics after font replacement");
    require(NativeUiRenderer::end(), "Font reuse presentation failed");
    NativeUiRenderer::releaseFont(font);
    DeleteObject(font);
    font = nullptr;

    NativeUiRenderer::releaseWindow(hwnd);
    require(NativeUiRenderer::begin(hwnd, proxy.dc, client), "Recreated target begin failed");
    require(NativeUiRenderer::needsFullPaint(), "Recreated target must request a full paint");
    solid(proxy.dc, client, blue);
    capture.capture(proxy.dc);
    require(capture.pixel(479, 319) == blue, "Recreated target has wrong dimensions");
    require(NativeUiRenderer::end(), "Recreated target presentation failed");
    require(SetWindowPos(hwnd, nullptr, 0, 0, 640, 360,
      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "Window resize failed");
    Surface largerProxy(640, 360), largerCapture(640, 360);
    const RECT largerClient{0, 0, 640, 360};
    require(NativeUiRenderer::begin(hwnd, largerProxy.dc, largerClient), "Resized target begin failed");
    require(NativeUiRenderer::needsFullPaint(), "Resized target must request a full paint");
    solid(largerProxy.dc, largerClient, green);
    largerCapture.capture(largerProxy.dc);
    require(largerCapture.pixel(639, 359) == green, "Resized target retained stale dimensions");
    require(NativeUiRenderer::end(), "Resized target presentation failed");

    NativeUiRenderer::releaseWindow(hwnd);
    DestroyWindow(hwnd);
    hwnd = nullptr;
    NativeUiRenderer::shutdown();
    std::cout << "UI_RENDERER_OK: native/GDI order, disjoint and nested clips, "
      "retained reverse progress, gradient reuse/eviction, native/legacy ornaments, "
      "Unicode/ellipsis, font replacement, target recreation/resize\n";
    return 0;
  } catch (const std::exception& error) {
    NativeUiRenderer::end();
    if (font) {
      NativeUiRenderer::releaseFont(font);
      DeleteObject(font);
    }
    if (hwnd) {
      NativeUiRenderer::releaseWindow(hwnd);
      DestroyWindow(hwnd);
    }
    NativeUiRenderer::shutdown();
    std::cerr << "UI_RENDERER_FAILED: " << error.what() << '\n';
    return 1;
  }
}

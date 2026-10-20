#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "swell/swell.h"
#include "native_ui_renderer.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// Exercise the production renderer on real CoreGraphics/CoreText. The only
// host substitutes are SWELL's exported context lookup and layer opt-in entry.
static void* testContext(HDC dc) { return reinterpret_cast<void*>(dc); }
extern "C" {
void* (*SWELL_GetCtxGC)(HDC) = testContext;
int (*SWELL_EnableMetal)(HWND, int) = nullptr;
}

namespace {
struct Bitmap {
  explicit Bitmap(int scale) : scale(scale), pixels(160 * scale * 96 * scale * 4)
  {
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    context = CGBitmapContextCreate(pixels.data(), 160 * scale, 96 * scale,
      8, 160 * scale * 4, color, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(color);
    assert(context);
    CGContextTranslateCTM(context, 0, 96 * scale);
    CGContextScaleCTM(context, scale, -scale);
  }
  ~Bitmap() { CGContextRelease(context); }
  HDC dc() const { return reinterpret_cast<HDC>(context); }
  int scale;
  std::vector<std::uint8_t> pixels;
  CGContextRef context = nullptr;
};

// Quartz antialiasing may affect the edge, but must never modify a pixel
// outside the clip, including when the backing store has a Retina scale.
void expectChangesOnlyInside(const Bitmap& bitmap,
  const std::vector<std::uint8_t>& before, const RECT& allowed,
  bool requireChange = true)
{
  bool changed = false;
  for (int y = 0; y < 96 * bitmap.scale; ++y) {
    for (int x = 0; x < 160 * bitmap.scale; ++x) {
      const std::size_t offset =
        (static_cast<std::size_t>(y) * 160 * bitmap.scale + x) * 4;
      bool pixelChanged = false;
      for (int channel = 0; channel < 4; ++channel)
        pixelChanged |= bitmap.pixels[offset + channel] != before[offset + channel];
      if (!pixelChanged) continue;
      changed = true;
      assert(x >= allowed.left * bitmap.scale);
      assert(x < allowed.right * bitmap.scale);
      assert(y >= allowed.top * bitmap.scale);
      assert(y < allowed.bottom * bitmap.scale);
    }
  }
  if (requireChange) assert(changed);
}

void testDrawing(int scale)
{
  using namespace NativeUiRenderer;
  Bitmap bitmap(scale), nested(scale);
  const RECT client{0, 0, 160, 96};
  const HFONT font = reinterpret_cast<HFONT>(static_cast<std::uintptr_t>(1));
  registerFont(font, "Arial", 15, FW_NORMAL);
  int width = 0, height = 0;
  assert(!isActive(bitmap.dc()));
  assert(!measureText(bitmap.dc(), "before frame", font, width, height));
  assert(!begin(nullptr, nullptr, client));
  assert(begin(nullptr, bitmap.dc(), client));
  assert(isActive(bitmap.dc()));
  assert(!needsFullPaint());
  assert(gdiDC(bitmap.dc()) == bitmap.dc());
  assert(fillRect(bitmap.dc(), client, RGB(10, 20, 30)));

  const auto before = bitmap.pixels;
  const RECT clip{20, 16, 124, 80};
  CGContextSaveGState(bitmap.context);
  CGContextClipToRect(bitmap.context, CGRectMake(20, 16, 104, 64));
  assert(fillGradient(bitmap.dc(), client, RGB(200, 30, 10), RGB(20, 70, 230)));
  assert(fillRoundRect(bitmap.dc(), RECT{12, 8, 140, 84},
    RGB(30, 70, 40), RGB(250, 250, 250), 9));
  assert(strokeRoundRect(bitmap.dc(), RECT{28, 24, 116, 64},
    RGB(255, 100, 30), 6));
  CGContextRestoreGState(bitmap.context);
  expectChangesOnlyInside(bitmap, before, clip);

  // A nested screen paint must not steal the active outer screen context.
  assert(begin(nullptr, nested.dc(), client));
  assert(isActive(nested.dc()));
  assert(!isActive(bitmap.dc()));
  assert(fillRect(nested.dc(), client, RGB(90, 30, 70)));
  assert(end());
  assert(isActive(bitmap.dc()));
  assert(!isActive(nested.dc()));

  const std::string unicode = "M\xC3\xBAsica \xE2\x80\x94 a\xC3\xA7\xC3\xA3o";
  assert(measureText(bitmap.dc(), unicode, font, width, height));
  assert(width > 0 && height > 0);
  const int originalWidth = width;
  RECT measured{7, 8, 8, 9};
  assert(drawText(bitmap.dc(), unicode, measured,
    DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT, RGB(255, 255, 255), font));
  assert(measured.right - measured.left == width);
  assert(measured.bottom - measured.top == height);

  const auto beforeText = bitmap.pixels;
  RECT textRect{34, 30, 94, 66};
  assert(drawText(bitmap.dc(), unicode + " com um nome muito longo", textRect,
    DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS | DT_VCENTER,
    RGB(255, 255, 255), font));
  expectChangesOnlyInside(bitmap, beforeText, textRect);

  // Unsupported layouts, malformed UTF-8 and missing font metadata must leave
  // the surface untouched so the caller can draw through SWELL instead.
  const auto beforeFallback = bitmap.pixels;
  assert(!drawText(bitmap.dc(), "one\ntwo", textRect,
    DT_WORDBREAK | DT_NOPREFIX, RGB(255, 255, 255), font));
  assert(!drawText(bitmap.dc(), std::string("\xFF"), textRect,
    DT_SINGLELINE | DT_NOPREFIX, RGB(255, 255, 255), font));
  assert(!drawText(bitmap.dc(), std::string(4097, 'x'), textRect,
    DT_SINGLELINE, RGB(255, 255, 255), font));
  assert(!drawText(bitmap.dc(), "unknown font", textRect,
    DT_SINGLELINE, RGB(255, 255, 255), nullptr));
  assert(bitmap.pixels == beforeFallback);

  // Exercise eviction and reused HFONT addresses: a replacement font must not
  // inherit glyph metrics from the previous object stored at that address.
  for (int i = 0; i < 700; ++i)
    assert(measureText(bitmap.dc(), "Clock " + std::to_string(i), font, width, height));
  assert(measureText(bitmap.dc(), unicode, font, width, height));
  assert(width == originalWidth);
  registerFont(font, "Arial", 30, FW_BOLD);
  assert(measureText(bitmap.dc(), unicode, font, width, height));
  assert(width > originalWidth);
  releaseFont(font);
  assert(!measureText(bitmap.dc(), unicode, font, width, height));

  assert(end());
  assert(!isActive(bitmap.dc()));
  assert(!end());
  assert(!fillRect(bitmap.dc(), client, RGB(0, 0, 0)));
  shutdown();
}
} // namespace

int main()
{
  @autoreleasepool {
    testDrawing(1);
    testDrawing(2);
  }
  std::cout << "Native macOS renderer: clipping, nesting, UTF-8 and cache passed.\n";
  return 0;
}

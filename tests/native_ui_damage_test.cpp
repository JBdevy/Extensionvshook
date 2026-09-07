#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include "../src/native_ui_damage.h"

static void fill(HDC dc, RECT rect, COLORREF color)
{
  if (!NativeUiDamage::visible(dc, rect)) return;
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

int main()
{
  HDC dc = CreateCompatibleDC(nullptr);
  HDC reference = CreateCompatibleDC(nullptr);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 100;
  info.bmiHeader.biHeight = -100;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  void* pixels = nullptr;
  void* referencePixels = nullptr;
  HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
  HBITMAP referenceBitmap = CreateDIBSection(reference, &info, DIB_RGB_COLORS,
    &referencePixels, nullptr, 0);
  HGDIOBJ old = SelectObject(dc, bitmap);
  HGDIOBJ referenceOld = SelectObject(reference, referenceBitmap);
  const RECT client{0, 0, 100, 100};
  const RECT dirty{10, 20, 90, 40};
  const auto scene = [&](HDC target, int progress) {
    fill(target, client, RGB(22, 22, 22));
    fill(target, RECT{10, 5, 90, 15}, RGB(40, 50, 60));
    fill(target, dirty, RGB(160, 10, 10));
    fill(target, RECT{10, 35, progress, 38}, RGB(20, 200, 20));
    fill(target, RECT{10, 60, 90, 80}, RGB(255, 100, 0));
  };
  // New or resized buffers ignore a small OS update rectangle.
  {
    NativeUiDamage::Frame frame(nullptr);
    frame.begin(dc, client, dirty, false);
    assert(frame.bounds().bottom == 100);
    scene(dc, 80);
  }
  {
    NativeUiDamage::Frame frame(nullptr);
    frame.begin(dc, client, dirty, true);
    assert(!NativeUiDamage::visible(dc, RECT{10, 60, 90, 80}));
    assert(NativeUiDamage::visible(reference, client));
    scene(dc, 30); // seek backwards must erase the previous longer bar
    // Nested drawing clips must intersect and restore the frame clip.
    const int saved = SaveDC(dc);
    IntersectClipRect(dc, 0, 0, 100, 100);
    const RECT outside{0, 60, 10, 70};
    assert(!RectVisible(dc, &outside));
    RestoreDC(dc, saved);
  }
  assert(NativeUiDamage::activeDc == nullptr);
  scene(reference, 30);
  GdiFlush();
  for (int y = 0; y < 100; ++y) {
    for (int x = 0; x < 100; ++x) {
      assert(GetPixel(dc, x, y) == GetPixel(reference, x, y));
    }
  }
  // A full invalidation after a modal/resize clears every previous pixel.
  {
    NativeUiDamage::Frame frame(nullptr);
    frame.begin(dc, client, client, true);
    fill(dc, client, RGB(1, 2, 3));
  }
  assert(GetPixel(dc, 50, 70) == RGB(1, 2, 3));
  SelectObject(dc, old);
  SelectObject(reference, referenceOld);
  DeleteObject(bitmap);
  DeleteObject(referenceBitmap);
  DeleteDC(dc);
  DeleteDC(reference);
  std::cout << "UI_DAMAGE_OK: partial/full equivalence, reverse progress, nested clips, first frame\n";
}

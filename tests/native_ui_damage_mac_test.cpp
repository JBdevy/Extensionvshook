#include <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

// Exercise the production damage/clip implementation on real CoreGraphics
// bitmap contexts; SWELL's clip API is a thin wrapper around these operations.
using HDC = CGContextRef;
using HWND = void*;
struct RECT { int left, top, right, bottom; };
static void SWELL_PushClipRegion(HDC dc) { CGContextSaveGState(dc); }
static void SWELL_PopClipRegion(HDC dc) { CGContextRestoreGState(dc); }
static void SWELL_SetClipRegion(HDC dc, const RECT* r) {
  CGContextClipToRect(dc, CGRectMake(r->left, r->top, r->right-r->left, r->bottom-r->top));
}
static void* getContext(HDC dc) { return dc; }
static auto SWELL_GetCtxGC = &getContext;
#include "../src/native_ui_damage.h"

static void fill(HDC dc, RECT r, double red, double green, double blue) {
  if (!NativeUiDamage::visible(dc, r)) return;
  CGContextSetRGBFillColor(dc, red, green, blue, 1);
  CGContextFillRect(dc, CGRectMake(r.left, r.top, r.right-r.left, r.bottom-r.top));
}

int main() {
  // A faster transport/drag timer must not accelerate decorative repaints.
  // Irregular 33/16ms delivery retains 10 updates/s, instead of drifting to
  // 7.5/8.9 updates/s by resetting the deadline to each delivery time.
  for (int timerMs : {100, 33, 16}) {
    NativeUiDamage::AnimationCadence cadence;
    int frames = 0;
    for (int now = 0; now < 1000; now += timerMs)
      frames += cadence.due(now, 100) ? 1 : 0;
    assert(frames == 10);
    assert(cadence.due(10000, 100));
    assert(!cadence.due(10000, 100)); // No catch-up burst.
    assert(!cadence.due(10001, 100));
    assert(cadence.due(10100, 100));
    cadence.reset();
    assert(cadence.due(10101, 100)); // Recreated window starts immediately.
    assert(cadence.due(5, 100)); // Defensive clock reset.
    assert(!cadence.due(6, 100));
  }
  NativeUiDamage::Pending pending;
  RECT small{20, 20, 30, 30};
  pending.add(&small);
  pending.add(&small);
  assert(pending.rects.size() == 1);
  pending.add(nullptr);
  pending.add(&small);
  assert(pending.full && pending.rects.empty());

  for (int scale : {1, 2}) {
    const int pixelsWide = 120 * scale, pixelsHigh = 100 * scale;
    std::vector<unsigned char> actual(pixelsWide * pixelsHigh * 4), expected(actual.size());
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    auto make = [&](std::vector<unsigned char>& pixels) {
      auto dc = CGBitmapContextCreate(pixels.data(), pixelsWide, pixelsHigh, 8,
        pixelsWide * 4, color, kCGImageAlphaPremultipliedLast);
      assert(dc);
      CGContextScaleCTM(dc, scale, scale);
      return dc;
    };
    HDC dc = make(actual), reference = make(expected);
    const RECT client{0, 0, 120, 100};
    const RECT textArea{20, 20, 100, 70};
    std::vector<RECT> outline;
    NativeUiDamage::outline(outline, client, 5, 9);
    const auto scene = [&](HDC target, bool changed) {
      fill(target, client, 0.1, 0.1, 0.1);
      fill(target, textArea, 0.2, 0.4, 0.8);
      for (const auto& edge : outline) fill(target, edge, changed ? 1 : 0, changed ? 0 : 1, 0);
      CGContextSetRGBStrokeColor(target, changed ? 0.5 : 1, 0.2, 0.7, 1);
      for (int inset = 1; inset <= 3; ++inset) {
        auto path = CGPathCreateWithRoundedRect(
          CGRectMake(inset, inset, 120 - inset * 2, 100 - inset * 2), 5, 5, nullptr);
        CGContextAddPath(target, path);
        CGContextStrokePath(target);
        CGPathRelease(path);
      }
    };
    {
      NativeUiDamage::Frame frame(nullptr);
      frame.begin(dc, client, small, false, &outline);
      scene(dc, false); // First frame ignores partial requests.
    }
    {
      NativeUiDamage::Frame frame(nullptr);
      frame.begin(dc, client, client, true, &outline);
      assert(!NativeUiDamage::visible(dc, textArea));
      assert(NativeUiDamage::visible(reference, textArea));
      scene(dc, true);
      // Nested child clips must retain the disjoint outer clip.
      SWELL_PushClipRegion(dc);
      SWELL_SetClipRegion(dc, &client);
      CGContextSetRGBFillColor(dc, 1, 0, 1, 1);
      CGContextFillRect(dc, CGRectMake(40, 40, 10, 10));
      SWELL_PopClipRegion(dc);
    }
    assert(NativeUiDamage::activeDc == nullptr && NativeUiDamage::activeRects == nullptr);
    scene(reference, true);
    assert(actual == expected); // No erased labels or accumulated border trails.

    // Rebuild after modal close/resize must overwrite all stale content.
    {
      NativeUiDamage::Frame frame(nullptr);
      frame.begin(dc, client, client, true);
      fill(dc, client, 0.7, 0.2, 0.1);
    }
    fill(reference, client, 0.7, 0.2, 0.1);
    assert(actual == expected);
    // Missing host graphics access falls back to full rendering, never culls
    // text while clearing the whole background.
    SWELL_GetCtxGC = nullptr;
    {
      NativeUiDamage::Frame frame(nullptr);
      frame.begin(dc, client, client, true, &outline);
      assert(NativeUiDamage::visible(dc, textArea));
      scene(dc, false);
    }
    scene(reference, false);
    assert(actual == expected);
    SWELL_GetCtxGC = &getContext;

    // Interior-only updates may replace a filled rounded path with a solid
    // rectangle and skip its stroke. Test actual CoreGraphics pixels, including
    // narrow damage just beside the corner envelope, mixed interior/edge
    // damage, tiny shapes, and both display scales.
    int roundedPaths = 0, solidInteriors = 0, skippedStrokes = 0;
    const auto roundedScene = [&](HDC target, const RECT& box, int radius,
                                  bool optimized, bool changed) {
      fill(target, client, 0.05, 0.07, 0.09);
      CGContextSetRGBFillColor(target, changed ? 0.4 : 0.2, 0.3, 0.6, 1);
      CGContextSetRGBStrokeColor(target, 0.9, 0.2, 0.4, 1);
      const CGRect bounds = CGRectMake(box.left, box.top,
        box.right - box.left, box.bottom - box.top);
      const bool edge = !optimized || NativeUiDamage::edgeVisible(
        target, box, radius * 2 + 2);
      if (edge) {
        auto path = CGPathCreateWithRoundedRect(bounds, radius * 1.5,
          radius * 1.5, nullptr);
        CGContextAddPath(target, path);
        CGContextDrawPath(target, kCGPathFillStroke);
        CGPathRelease(path);
        if (optimized) ++roundedPaths;
      } else {
        CGContextFillRect(target, bounds);
        ++solidInteriors;
      }
      if (edge) {
        auto path = CGPathCreateWithRoundedRect(bounds, radius * 1.5,
          radius * 1.5, nullptr);
        CGContextAddPath(target, path);
        CGContextStrokePath(target);
        CGPathRelease(path);
      } else ++skippedStrokes;
    };
    for (const RECT box : {client, RECT{7, 8, 111, 92}, RECT{40, 35, 47, 40}}) {
      for (int radius : {1, 3, 8, 16}) {
        for (const std::vector<RECT>& parts : std::vector<std::vector<RECT>>{
            {{45, 44, 65, 56}}, {{0, 0, 12, 12}}, {{20, 20, 23, 23}},
            {{45, 44, 65, 56}, {112, 91, 120, 100}}}) {
          roundedScene(dc, box, radius, false, false);
          roundedScene(reference, box, radius, false, false);
          {
            NativeUiDamage::Frame frame(nullptr);
            frame.begin(dc, client, client, true, &parts);
            roundedScene(dc, box, radius, true, true);
          }
          {
            NativeUiDamage::Frame frame(nullptr);
            frame.begin(reference, client, client, true, &parts);
            roundedScene(reference, box, radius, false, true);
          }
          assert(actual == expected);
        }
      }
    }
    assert(roundedPaths > 0 && solidInteriors > 0 && skippedStrokes > 0);
    assert(NativeUiDamage::edgeVisible(reference, client, 5));
    CGContextRelease(dc);
    CGContextRelease(reference);
    CGColorSpaceRelease(color);
  }
  std::cout << "UI_DAMAGE_MAC_OK: independent RGB cadence, sparse/full pixels, Retina, nested clips, fallback, rounded interior/edge equivalence\n";
}

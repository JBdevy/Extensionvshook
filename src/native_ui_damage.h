#pragma once
#include <cstdint>
#include <vector>

// Included after the platform's Win32/SWELL declarations. Only the main
// panel's current paint DC is filtered; offscreen images and other windows
// must still be rendered normally.
namespace NativeUiDamage {
// Decorative animation has its own cadence even when the host's input or
// playback timer runs faster. Keep its phase across irregular timer delivery,
// without scheduling catch-up frames after a stall/hidden window.
class AnimationCadence {
public:
  bool due(int64_t nowMs, int intervalMs) {
    const int64_t interval = std::max(1, intervalMs);
    if (!initialized_ || nowMs < lastMs_) {
      initialized_ = true;
      lastMs_ = nowMs;
      return true;
    }
    const int64_t elapsed = nowMs - lastMs_;
    if (elapsed < interval) return false;
    lastMs_ = nowMs - elapsed % interval;
    return true;
  }
  void reset() { initialized_ = false; }
private:
  int64_t lastMs_ = 0;
  bool initialized_ = false;
};

inline HDC activeDc = nullptr;
inline RECT activeBounds{};
inline const std::vector<RECT>* activeRects = nullptr;

struct Pending {
  bool full = false;
  std::vector<RECT> rects;
  void add(const RECT* rect) {
    if (!rect || rects.size() >= 256) { full = true; rects.clear(); }
    if (full || !rect || rect->right <= rect->left || rect->bottom <= rect->top) return;
    for (const auto& old : rects) {
      if (old.left <= rect->left && old.top <= rect->top &&
          old.right >= rect->right && old.bottom >= rect->bottom) return;
    }
    rects.push_back(*rect);
  }
};

inline void outline(std::vector<RECT>& rects, const RECT& r, int width, int corner = 0)
{
  if (r.right <= r.left || r.bottom <= r.top) return;
  rects.push_back({r.left, r.top, r.right, std::min(r.bottom, r.top + width)});
  rects.push_back({r.left, std::max(r.top, r.bottom - width), r.right, r.bottom});
  rects.push_back({r.left, r.top, std::min(r.right, r.left + width), r.bottom});
  rects.push_back({std::max(r.left, r.right - width), r.top, r.right, r.bottom});
  if (corner > width) {
    rects.push_back({r.left, r.top, r.left + corner, r.top + corner});
    rects.push_back({r.right - corner, r.top, r.right, r.top + corner});
    rects.push_back({r.left, r.bottom - corner, r.left + corner, r.bottom});
    rects.push_back({r.right - corner, r.bottom - corner, r.right, r.bottom});
  }
}

inline bool intersects(const RECT& a, const RECT& b)
{
  return a.left < b.right && a.right > b.left &&
    a.top < b.bottom && a.bottom > b.top;
}

inline bool visible(HDC dc, const RECT& rect)
{
  if (!dc) return false;
  if (dc != activeDc) return true;
  // Leave room for antialiasing and the edge of a one-pixel pen.
  const RECT padded{rect.left - 2, rect.top - 2,
    rect.right + 2, rect.bottom + 2};
  if (!intersects(padded, activeBounds)) return false;
  if (activeRects && !activeRects->empty()) {
    bool hit = false;
    for (const auto& part : *activeRects) if (intersects(padded, part)) { hit = true; break; }
    if (!hit) return false;
  }
#ifdef _WIN32
  return RectVisible(dc, &padded) != FALSE;
#else
  return true;
#endif
}

// Conservative test for a stroked/rounded shape. A damaged interior does not
// require rasterizing its outline; the corner envelope includes antialiasing
// through visible(). Untracked DCs always retain the original drawing path.
inline bool edgeVisible(HDC dc, const RECT& r, int inset)
{
  inset = std::max(2, inset);
  return visible(dc, RECT{r.left, r.top, r.right, std::min(r.bottom, r.top + inset)}) ||
    visible(dc, RECT{r.left, std::max(r.top, r.bottom - inset), r.right, r.bottom}) ||
    visible(dc, RECT{r.left, r.top, std::min(r.right, r.left + inset), r.bottom}) ||
    visible(dc, RECT{std::max(r.left, r.right - inset), r.top, r.right, r.bottom});
}

class Frame {
public:
  // Capture BEFORE BeginPaint validates the window's update region.
  explicit Frame(HWND hwnd)
  {
#ifdef _WIN32
    region_ = CreateRectRgn(0, 0, 0, 0);
    if (region_) {
      const int kind = GetUpdateRgn(hwnd, region_, FALSE);
      hasRegion_ = kind == SIMPLEREGION || kind == COMPLEXREGION;
    }
#else
    (void)hwnd;
#endif
  }
  Frame(const Frame&) = delete;
  Frame& operator=(const Frame&) = delete;
  ~Frame()
  {
    finish();
#ifdef _WIN32
    if (region_) DeleteObject(region_);
#endif
  }

  void begin(HDC dc, const RECT& client, const RECT& damage, bool reusable,
      const std::vector<RECT>* requested = nullptr)
  {
    bounds_ = client;
    const RECT clipped{std::max(client.left, damage.left),
      std::max(client.top, damage.top),
      std::min(client.right, damage.right),
      std::min(client.bottom, damage.bottom)};
    if (reusable && clipped.right > clipped.left &&
        clipped.bottom > clipped.top) bounds_ = clipped;
    dc_ = dc;
    previousDc_ = activeDc;
    previousBounds_ = activeBounds;
    previousRects_ = activeRects;
    rects_.clear();
    if (reusable && requested) {
      for (const auto& part : *requested) {
        const RECT clippedPart{std::max(client.left, part.left), std::max(client.top, part.top),
          std::min(client.right, part.right), std::min(client.bottom, part.bottom)};
        if (clippedPart.right > clippedPart.left && clippedPart.bottom > clippedPart.top)
          rects_.push_back(clippedPart);
      }
    }
#ifdef _WIN32
    saved_ = SaveDC(dc);
    if (saved_) {
      if (reusable && hasRegion_) ExtSelectClipRgn(dc, region_, RGN_AND);
      IntersectClipRect(dc, bounds_.left, bounds_.top,
        bounds_.right, bounds_.bottom);
    }
    // Win32 already preserved the exact update region above. RectVisible
    // uses that region directly, without a second application-owned mask.
    rects_.clear();
#else
    SWELL_PushClipRegion(dc);
    SWELL_SetClipRegion(dc, &bounds_);
#ifdef __APPLE__
    if (!rects_.empty()) {
      auto context = SWELL_GetCtxGC ? static_cast<CGContextRef>(SWELL_GetCtxGC(dc)) : nullptr;
      if (context) {
        std::vector<CGRect> parts;
        for (const auto& r : rects_) parts.push_back(CGRectMake(
          r.left, r.top, r.right - r.left, r.bottom - r.top));
        CGContextClipToRects(context, parts.data(), parts.size());
      } else rects_.clear();
    }
#else
    rects_.clear(); // Platforms without disjoint clipping keep the full path.
#endif
#endif
    activeDc = dc;
    activeBounds = bounds_;
    activeRects = &rects_;
  }

  const RECT& bounds() const { return bounds_; }

  void finish()
  {
    if (!dc_) return;
#ifdef _WIN32
    if (saved_) RestoreDC(dc_, saved_);
#else
    SWELL_PopClipRegion(dc_);
#endif
    activeDc = previousDc_;
    activeBounds = previousBounds_;
    activeRects = previousRects_;
    dc_ = nullptr;
  }

private:
  HDC dc_ = nullptr;
  HDC previousDc_ = nullptr;
  RECT previousBounds_{};
  RECT bounds_{};
  std::vector<RECT> rects_;
  const std::vector<RECT>* previousRects_ = nullptr;
#ifdef _WIN32
  HRGN region_ = nullptr;
  bool hasRegion_ = false;
  int saved_ = 0;
#endif
};
} // namespace NativeUiDamage

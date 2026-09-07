#pragma once

// Included after the platform's Win32/SWELL declarations. Only the main
// panel's current paint DC is filtered; offscreen images and other windows
// must still be rendered normally.
namespace NativeUiDamage {
inline HDC activeDc = nullptr;
inline RECT activeBounds{};

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
#ifdef _WIN32
  return RectVisible(dc, &padded) != FALSE;
#else
  return true;
#endif
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

  void begin(HDC dc, const RECT& client, const RECT& damage, bool reusable)
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
#ifdef _WIN32
    saved_ = SaveDC(dc);
    if (saved_) {
      if (reusable && hasRegion_) ExtSelectClipRgn(dc, region_, RGN_AND);
      IntersectClipRect(dc, bounds_.left, bounds_.top,
        bounds_.right, bounds_.bottom);
    }
#else
    SWELL_PushClipRegion(dc);
    SWELL_SetClipRegion(dc, &bounds_);
#endif
    activeDc = dc;
    activeBounds = bounds_;
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
    dc_ = nullptr;
  }

private:
  HDC dc_ = nullptr;
  HDC previousDc_ = nullptr;
  RECT previousBounds_{};
  RECT bounds_{};
#ifdef _WIN32
  HRGN region_ = nullptr;
  bool hasRegion_ = false;
  int saved_ = 0;
#endif
};
} // namespace NativeUiDamage

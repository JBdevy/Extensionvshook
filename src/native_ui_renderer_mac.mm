#ifdef __APPLE__

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <QuartzCore/QuartzCore.h>

#include "swell/swell.h"
#include "native_ui_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NativeUiRenderer {
namespace {

// SWELL and AppKit still own the windows, input and invalidation. This backend
// draws into their CGContext, preserving its Retina transform and damage clip.
// Synchronous AppKit layer backing retains/composites the finished view without
// introducing an extra application-owned bitmap or an asynchronous SWELL blit.
template <typename T>
class CFHandle {
public:
  explicit CFHandle(T value = nullptr) : value_(value) {}
  ~CFHandle() { if (value_) CFRelease(value_); }
  CFHandle(const CFHandle&) = delete;
  CFHandle& operator=(const CFHandle&) = delete;
  CFHandle(CFHandle&& other) noexcept : value_(other.value_)
  {
    other.value_ = nullptr;
  }
  CFHandle& operator=(CFHandle&& other) noexcept
  {
    if (this != &other) {
      reset(other.value_);
      other.value_ = nullptr;
    }
    return *this;
  }
  T get() const { return value_; }
  void reset(T value = nullptr)
  {
    if (value_) CFRelease(value_);
    value_ = value;
  }
private:
  T value_;
};

struct FrameState {
  HDC dc = nullptr;
  CGContextRef context = nullptr; // Borrowed only until end().
};

thread_local std::vector<FrameState> frames;
std::mutex textMutex;
std::unordered_map<HFONT, CFHandle<CTFontRef>> fonts;

struct LineKey {
  HFONT font = nullptr;
  std::string text;
  bool operator==(const LineKey& other) const
  {
    return font == other.font && text == other.text;
  }
};

struct LineKeyHash {
  std::size_t operator()(const LineKey& key) const
  {
    return std::hash<HFONT>{}(key.font) ^
      (std::hash<std::string>{}(key.text) << 1);
  }
};

struct CachedLine {
  LineKey key;
  CFHandle<CTLineRef> line;
  CFHandle<CTLineRef> truncated;
  CFHandle<CTLineRef> ellipsis;
  double truncatedWidth = -1.0;
  CGFloat ascent = 0.0;
  double width = 0.0;
  int height = 0;
};

// Repeated labels do not need to be shaped on every playback tick. The cache is
// bounded even when the user scrolls through a large setlist or a clock changes.
constexpr std::size_t maxCachedLines = 512;
constexpr std::size_t maxCachedTextBytes = 4096;
std::list<CachedLine> lines;
std::unordered_map<LineKey, std::list<CachedLine>::iterator, LineKeyHash> lineIndex;

CGContextRef activeContext(HDC dc)
{
  return !frames.empty() && dc && frames.back().dc == dc
    ? frames.back().context : nullptr;
}

bool validRect(const RECT& rect)
{
  return rect.right > rect.left && rect.bottom > rect.top;
}

CGRect cgRect(const RECT& rect)
{
  return CGRectMake(rect.left, rect.top,
    rect.right - rect.left, rect.bottom - rect.top);
}

void fillColor(CGContextRef context, COLORREF color)
{
  CGContextSetRGBFillColor(context, GetRValue(color) / 255.0,
    GetGValue(color) / 255.0, GetBValue(color) / 255.0, 1.0);
}

void strokeColor(CGContextRef context, COLORREF color)
{
  CGContextSetRGBStrokeColor(context, GetRValue(color) / 255.0,
    GetGValue(color) / 255.0, GetBValue(color) / 255.0, 1.0);
}

void eraseFontLines(HFONT font)
{
  for (auto it = lines.begin(); it != lines.end();) {
    if (it->key.font == font) {
      lineIndex.erase(it->key);
      it = lines.erase(it);
    } else {
      ++it;
    }
  }
}

CFHandle<CTLineRef> createLine(CTFontRef font, const std::string& text)
{
  CFHandle<CFStringRef> string(CFStringCreateWithBytes(kCFAllocatorDefault,
    reinterpret_cast<const UInt8*>(text.data()),
    static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
  if (!string.get()) return CFHandle<CTLineRef>();
  const void* keys[] = {
    kCTFontAttributeName, kCTForegroundColorFromContextAttributeName
  };
  const void* values[] = {font, kCFBooleanTrue};
  CFHandle<CFDictionaryRef> attributes(CFDictionaryCreate(kCFAllocatorDefault,
    keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks));
  if (!attributes.get()) return CFHandle<CTLineRef>();
  CFHandle<CFAttributedStringRef> attributed(CFAttributedStringCreate(
    kCFAllocatorDefault, string.get(), attributes.get()));
  return CFHandle<CTLineRef>(attributed.get()
    ? CTLineCreateWithAttributedString(attributed.get()) : nullptr);
}

CachedLine* cachedLine(HFONT font, const std::string& text)
{
  if (text.size() > maxCachedTextBytes) return nullptr;
  const auto fontIt = fonts.find(font);
  if (fontIt == fonts.end()) return nullptr;
  const LineKey key{font, text};
  const auto found = lineIndex.find(key);
  if (found != lineIndex.end()) {
    lines.splice(lines.begin(), lines, found->second);
    return &*found->second;
  }

  auto line = createLine(fontIt->second.get(), text);
  if (!line.get()) return nullptr;
  if (lines.size() >= maxCachedLines) {
    lineIndex.erase(lines.back().key);
    lines.pop_back();
  }
  CachedLine entry;
  entry.key = key;
  entry.line = std::move(line);
  CGFloat descent = 0.0;
  CGFloat leading = 0.0;
  entry.width = CTLineGetTypographicBounds(entry.line.get(),
    &entry.ascent, &descent, &leading);
  // Match SWELL's baseline/height convention so button and caret geometry stays
  // unchanged when native text is mixed with legacy controls.
  entry.height = static_cast<int>(std::floor(
    entry.ascent + descent + leading + 1.5)) + 1;
  lines.push_front(std::move(entry));
  lineIndex.emplace(lines.front().key, lines.begin());
  return &lines.front();
}

bool normalizeText(const std::string& text, UINT flags, std::string& result)
{
  constexpr UINT supportedFlags = DT_CENTER | DT_RIGHT | DT_VCENTER |
    DT_BOTTOM | DT_SINGLELINE | DT_NOCLIP | DT_CALCRECT |
    DT_END_ELLIPSIS | DT_NOPREFIX;
  if ((flags & ~supportedFlags) != 0 || text.size() > maxCachedTextBytes)
    return false;
  result.clear();
  result.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char value = text[i];
    if (!value) break;
    if (value == '\r') continue;
    if (value == '\n') {
      if (!(flags & DT_SINGLELINE)) return false;
      result.push_back(' ');
    } else if (value == '\t') {
      // Legacy DrawText tab expansion and wrapping remain on its own path.
      return false;
    } else if (value == '&' && !(flags & DT_NOPREFIX)) {
      if (i + 1 < text.size() && text[i + 1] == '&') {
        result.push_back('&');
        ++i;
      }
    } else {
      result.push_back(value);
    }
  }
  return true;
}

NSView* viewForWindow(HWND window)
{
  if (!window) return nil;
  id object = (id)window;
  if ([object isKindOfClass:[NSView class]]) return (NSView*)object;
  if ([object isKindOfClass:[NSWindow class]])
    return [(NSWindow*)object contentView];
  return nil;
}

} // namespace

void prepareWindow(HWND window)
{
  if (![NSThread isMainThread]) return;
  NSView* view = viewForWindow(window);
  if (!view || [view wantsLayer]) return;
  // Must run when the window is created, before BeginPaint/drawRect. -2 is
  // SWELL's synchronous Core Animation mode, supported by macOS 10.13.
  // Never use -1: SWELL documents unsafe lifetimes for asynchronous BitBlt.
  if (SWELL_EnableMetal) SWELL_EnableMetal((HWND)view, -2);
  if (![view wantsLayer]) [view setWantsLayer:YES];
  // Keep AppKit's during-resize redraw policy and automatic backing scale.
  // The existing WM_SIZE/InvalidateRect flow remains responsible for content.
}

bool begin(HWND hwnd, HDC dc, const RECT& client)
{
  (void)hwnd;
  (void)client;
  FrameState frame;
  frame.dc = dc;
  if (dc && SWELL_GetCtxGC)
    frame.context = static_cast<CGContextRef>(SWELL_GetCtxGC(dc));
  if (!frame.context) return false;
  CGContextSaveGState(frame.context);
  frames.push_back(frame);
  return true;
}

bool end()
{
  if (frames.empty()) return false;
  if (frames.back().context) CGContextRestoreGState(frames.back().context);
  frames.pop_back();
  return true;
}

bool isActive(HDC dc)
{
  return activeContext(dc) != nullptr;
}

bool needsFullPaint()
{
  return false;
}

void releaseWindow(HWND hwnd)
{
  // AppKit owns the backing layer and releases it with its NSView.
  (void)hwnd;
}

HDC gdiDC(HDC dc)
{
  return dc;
}

void flushGdi() {}
void invalidateClip(HDC) {}

bool fillRect(HDC dc, const RECT& rect, COLORREF color)
{
  CGContextRef context = activeContext(dc);
  if (!context) return false;
  if (!validRect(rect)) return true;
  CGContextSaveGState(context);
  fillColor(context, color);
  CGContextFillRect(context, cgRect(rect));
  CGContextRestoreGState(context);
  return true;
}

bool fillRoundRect(HDC dc, const RECT& rect,
  COLORREF fill, COLORREF border, int radius)
{
  CGContextRef context = activeContext(dc);
  if (!context) return false;
  if (!validRect(rect)) return true;
  const CGRect bounds = CGRectInset(cgRect(rect), 0.5, 0.5);
  if (bounds.size.width <= 0.0 || bounds.size.height <= 0.0)
    return fillRect(dc, rect, border);
  const CGFloat corner = std::min<CGFloat>(std::max(0, radius),
    std::min(bounds.size.width, bounds.size.height) * 0.5);
  CFHandle<CGPathRef> path(CGPathCreateWithRoundedRect(
    bounds, corner, corner, nullptr));
  if (!path.get()) return false;
  CGContextSaveGState(context);
  CGContextSetShouldAntialias(context, radius > 0);
  CGContextSetLineWidth(context, 1.0);
  fillColor(context, fill);
  strokeColor(context, border);
  CGContextBeginPath(context);
  CGContextAddPath(context, path.get());
  CGContextDrawPath(context, kCGPathFillStroke);
  CGContextRestoreGState(context);
  return true;
}

bool strokeRoundRect(HDC dc, const RECT& rect, COLORREF color, int radius)
{
  CGContextRef context = activeContext(dc);
  if (!context) return false;
  if (!validRect(rect)) return true;
  const CGRect bounds = CGRectInset(cgRect(rect), 0.5, 0.5);
  if (bounds.size.width <= 0.0 || bounds.size.height <= 0.0)
    return fillRect(dc, rect, color);
  const CGFloat corner = std::min<CGFloat>(std::max(0, radius),
    std::min(bounds.size.width, bounds.size.height) * 0.5);
  CFHandle<CGPathRef> path(CGPathCreateWithRoundedRect(
    bounds, corner, corner, nullptr));
  if (!path.get()) return false;
  CGContextSaveGState(context);
  CGContextSetShouldAntialias(context, radius > 0);
  CGContextSetLineWidth(context, 1.0);
  strokeColor(context, color);
  CGContextBeginPath(context);
  CGContextAddPath(context, path.get());
  CGContextStrokePath(context);
  CGContextRestoreGState(context);
  return true;
}

bool fillGradient(HDC dc, const RECT& rect, COLORREF top,
  COLORREF bottom, bool horizontal)
{
  CGContextRef context = activeContext(dc);
  if (!context) return false;
  if (!validRect(rect)) return true;
  if (top == bottom) return fillRect(dc, rect, top);
  CFHandle<CGColorSpaceRef> space(CGColorSpaceCreateDeviceRGB());
  const CGFloat components[] = {
    GetRValue(top) / 255.0, GetGValue(top) / 255.0,
    GetBValue(top) / 255.0, 1.0,
    GetRValue(bottom) / 255.0, GetGValue(bottom) / 255.0,
    GetBValue(bottom) / 255.0, 1.0
  };
  const CGFloat positions[] = {0.0, 1.0};
  CFHandle<CGGradientRef> gradient(space.get()
    ? CGGradientCreateWithColorComponents(space.get(), components,
        positions, 2) : nullptr);
  if (!gradient.get()) return false;
  CGContextSaveGState(context);
  CGContextClipToRect(context, cgRect(rect));
  CGContextDrawLinearGradient(context, gradient.get(),
    CGPointMake(rect.left, rect.top),
    horizontal ? CGPointMake(rect.right, rect.top)
               : CGPointMake(rect.left, rect.bottom),
    kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
  CGContextRestoreGState(context);
  return true;
}

bool drawText(HDC dc, const std::string& text, RECT& rect,
  UINT flags, COLORREF color, HFONT font)
{
  CGContextRef context = activeContext(dc);
  if (!context) return false;
  std::string normalized;
  if (!normalizeText(text, flags, normalized)) return false;
  if (normalized.empty()) {
    if (flags & DT_CALCRECT) {
      rect.right = rect.left;
      rect.bottom = rect.top;
    }
    return true;
  }
  std::lock_guard<std::mutex> lock(textMutex);
  CachedLine* entry = cachedLine(font, normalized);
  if (!entry) return false;
  if (flags & DT_CALCRECT) {
    rect.right = rect.left + static_cast<int>(std::floor(entry->width + 0.5));
    rect.bottom = rect.top + entry->height;
    return true;
  }
  if (!(flags & DT_NOCLIP) && !validRect(rect)) return true;
  const double availableWidth = std::max(0, rect.right - rect.left);
  CTLineRef line = entry->line.get();
  double width = entry->width;
  if ((flags & DT_END_ELLIPSIS) && width > availableWidth) {
    if (entry->truncatedWidth != availableWidth) {
      entry->truncated.reset();
      if (!entry->ellipsis.get())
        entry->ellipsis = createLine(fonts.find(font)->second.get(), "\xE2\x80\xA6");
      if (!entry->ellipsis.get()) return false;
      entry->truncated.reset(CTLineCreateTruncatedLine(
        line, availableWidth, kCTLineTruncationEnd, entry->ellipsis.get()));
      entry->truncatedWidth = availableWidth;
    }
    // A width smaller than the ellipsis legitimately produces no drawable line.
    if (!entry->truncated.get()) return true;
    line = entry->truncated.get();
    width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
  }
  CGFloat x = rect.left;
  CGFloat y = rect.top;
  if (flags & DT_RIGHT) x += availableWidth - width;
  else if (flags & DT_CENTER) x += (availableWidth - width) * 0.5;
  if (flags & DT_BOTTOM) y += rect.bottom - rect.top - entry->height;
  else if (flags & DT_VCENTER)
    y += (rect.bottom - rect.top - entry->height) * 0.5;

  CGContextSaveGState(context);
  if (!(flags & DT_NOCLIP)) CGContextClipToRect(context, cgRect(rect));
  fillColor(context, color);
  // SWELL's drawing coordinates are top-left oriented, including bitmap DCs.
  // Flip only the glyph matrix; leave the context's device/Retina transform.
  CGContextSetTextDrawingMode(context, kCGTextFill);
  CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1.0, -1.0));
  CGContextSetTextPosition(context, x, y + entry->ascent);
  CTLineDraw(line, context);
  CGContextRestoreGState(context);
  return true;
}

bool measureText(HDC dc, const std::string& text, HFONT font,
  int& width, int& height)
{
  if (!activeContext(dc)) return false;
  std::string normalized;
  if (!normalizeText(text, DT_SINGLELINE | DT_NOPREFIX, normalized))
    return false;
  std::lock_guard<std::mutex> lock(textMutex);
  CachedLine* entry = cachedLine(font, normalized);
  if (!entry) return false;
  width = static_cast<int>(std::floor(entry->width + 0.5));
  height = entry->height;
  return true;
}

void registerFont(HFONT handle, const char* family,
  int size, int weight, bool italic)
{
  if (!handle || !family) return;
  const double requestedSize = std::abs(static_cast<double>(size));
  const CGFloat pointSize = requestedSize < 2.0 || requestedSize > 8192.0
    ? 10.0 : requestedSize;
  CFHandle<CFStringRef> name(CFStringCreateWithCString(
    kCFAllocatorDefault, family, kCFStringEncodingUTF8));
  if (!name.get()) return;
  CFHandle<CTFontRef> font(CTFontCreateWithName(name.get(), pointSize, nullptr));
  if (!font.get()) return;
  // This matches SWELL's FW_BOLD threshold; its CreateFont also treats positive
  // and negative heights identically on macOS.
  const CTFontSymbolicTraits traits =
    (weight >= FW_BOLD ? kCTFontBoldTrait : 0) |
    (italic ? kCTFontItalicTrait : 0);
  if (traits) {
    CFHandle<CTFontRef> styled(CTFontCreateCopyWithSymbolicTraits(
      font.get(), pointSize, nullptr, traits, traits));
    if (styled.get()) font = std::move(styled);
  }
  std::lock_guard<std::mutex> lock(textMutex);
  eraseFontLines(handle);
  fonts.insert_or_assign(handle, std::move(font));
}

void releaseFont(HFONT handle)
{
  if (!handle) return;
  std::lock_guard<std::mutex> lock(textMutex);
  const auto found = fonts.find(handle);
  if (found == fonts.end()) return;
  eraseFontLines(handle);
  fonts.erase(found);
}

void shutdown()
{
  // Paint scopes are finished by the caller before releasing shared resources.
  std::lock_guard<std::mutex> lock(textMutex);
  lineIndex.clear();
  lines.clear();
  fonts.clear();
}

} // namespace NativeUiRenderer

#endif // __APPLE__

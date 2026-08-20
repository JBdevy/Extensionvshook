#include "native_teleprompt_mac.h"

#ifdef __APPLE__

#import <AppKit/AppKit.h>

#include "swell/swell.h"

#include <algorithm>
#include <cstddef>
#include <map>

namespace {

struct SavedWindowState {
  NSWindowStyleMask styleMask = NSWindowStyleMaskTitled;
  NSWindowCollectionBehavior collectionBehavior =
    NSWindowCollectionBehaviorDefault;
  NSWindowTitleVisibility titleVisibility = NSWindowTitleVisible;
  NSRect frame = NSZeroRect;
  NSInteger level = NSNormalWindowLevel;
  NSWindow* parentWindow = nil;
  bool titlebarAppearsTransparent = false;
  bool movable = true;
  bool movableByWindowBackground = false;
  bool hasShadow = true;
};

std::map<void*, SavedWindowState> g_fullscreenTeleprompts;

NSWindow* telepromptWindowFromSwellHandle(void* swellWindow)
{
  if (!swellWindow) return nil;
  id object = (id)swellWindow;
  if ([object isKindOfClass:[NSWindow class]]) {
    return (NSWindow*)object;
  }
  if ([object isKindOfClass:[NSView class]]) {
    return [(NSView*)object window];
  }
  return nil;
}

} // namespace

extern "C" bool VSHookMacSetTelepromptFullscreen(
  void* swellWindow,
  bool fullscreen)
{
  if (![NSThread isMainThread]) return false;

  NSWindow* window =
    telepromptWindowFromSwellHandle(swellWindow);
  if (!window) {
    g_fullscreenTeleprompts.erase(swellWindow);
    return false;
  }

  if (fullscreen) {
    if (g_fullscreenTeleprompts.find(swellWindow) !=
        g_fullscreenTeleprompts.end()) {
      return true;
    }
    SavedWindowState saved;
    saved.styleMask = [window styleMask];
    saved.collectionBehavior = [window collectionBehavior];
    saved.titleVisibility = [window titleVisibility];
    saved.frame = [window frame];
    saved.level = [window level];
    saved.parentWindow = [window parentWindow];
    saved.titlebarAppearsTransparent =
      [window titlebarAppearsTransparent];
    saved.movable = [window isMovable];
    saved.movableByWindowBackground =
      [window isMovableByWindowBackground];
    saved.hasShadow = [window hasShadow];
    g_fullscreenTeleprompts.emplace(swellWindow, saved);

    // Dialogs SWELL normalmente nascem como child/owned windows do REAPER.
    // Em tela cheia o Teleprompt e uma saida independente: remove o vinculo
    // para que minimizar o REAPER nao esconda o segundo monitor.
    if (saved.parentWindow) {
      [saved.parentWindow removeChildWindow:window];
    }

    NSScreen* screen = [window screen];
    if (!screen) screen = [NSScreen mainScreen];
    [window setStyleMask:NSWindowStyleMaskBorderless];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovable:NO];
    [window setMovableByWindowBackground:NO];
    [window setHasShadow:NO];
    [window setLevel:NSMainMenuWindowLevel + 1];
    [window setCollectionBehavior:
      saved.collectionBehavior |
      NSWindowCollectionBehaviorCanJoinAllSpaces |
      NSWindowCollectionBehaviorFullScreenAuxiliary];
    if (screen) {
      [window setFrame:[screen frame] display:YES animate:NO];
    }
    [window makeKeyAndOrderFront:nil];
    return true;
  }

  const auto savedIt =
    g_fullscreenTeleprompts.find(swellWindow);
  if (savedIt == g_fullscreenTeleprompts.end()) {
    return true;
  }
  const SavedWindowState saved = savedIt->second;
  g_fullscreenTeleprompts.erase(swellWindow);
  [window setStyleMask:saved.styleMask];
  [window setTitleVisibility:saved.titleVisibility];
  [window setTitlebarAppearsTransparent:
    saved.titlebarAppearsTransparent];
  [window setMovable:saved.movable];
  [window setMovableByWindowBackground:
    saved.movableByWindowBackground];
  [window setHasShadow:saved.hasShadow];
  [window setLevel:saved.level];
  [window setCollectionBehavior:saved.collectionBehavior];
  [window setFrame:saved.frame display:YES animate:NO];
  if (saved.parentWindow) {
    [saved.parentWindow addChildWindow:window ordered:NSWindowAbove];
  }
  [window makeKeyAndOrderFront:nil];
  return true;
}

extern "C" void VSHookMacReleaseTelepromptFullscreen(
  void* swellWindow)
{
  g_fullscreenTeleprompts.erase(swellWindow);
}

extern "C" double VSHookMacTelepromptBackingScale(
  void* swellWindow)
{
  NSWindow* window =
    telepromptWindowFromSwellHandle(swellWindow);
  if (!window) return 1.0;
  return std::max(
    1.0, static_cast<double>([window backingScaleFactor]));
}

extern "C" bool VSHookMacDrawTelepromptBitmap(
  void* swellDeviceContext,
  const void* bgraPixels,
  int sourceWidth,
  int sourceHeight,
  int sourceRowSpanPixels,
  bool sourceFlipped,
  int destinationX,
  int destinationY,
  int destinationWidth,
  int destinationHeight)
{
  if (!swellDeviceContext || !bgraPixels ||
      sourceWidth <= 0 || sourceHeight <= 0 ||
      sourceRowSpanPixels < sourceWidth ||
      destinationWidth <= 0 || destinationHeight <= 0 ||
      !SWELL_GetCtxGC) {
    return false;
  }

  CGContextRef context = (CGContextRef)(
    SWELL_GetCtxGC(
      (HDC)swellDeviceContext));
  if (!context) return false;

  const std::size_t bytesPerRow =
    static_cast<std::size_t>(sourceRowSpanPixels) * 4u;
  const std::size_t byteCount =
    bytesPerRow * static_cast<std::size_t>(sourceHeight);
  CGDataProviderRef provider =
    CGDataProviderCreateWithData(
      nullptr, bgraPixels, byteCount, nullptr);
  if (!provider) return false;

  CGColorSpaceRef colorSpace =
    CGColorSpaceCreateDeviceRGB();
  CGImageRef image = colorSpace
    ? CGImageCreate(
        static_cast<std::size_t>(sourceWidth),
        static_cast<std::size_t>(sourceHeight),
        8, 32, bytesPerRow, colorSpace,
        kCGBitmapByteOrder32Little |
          kCGImageAlphaNoneSkipFirst,
        provider, nullptr, false,
        kCGRenderingIntentDefault)
    : nullptr;
  if (colorSpace) CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);
  if (!image) return false;

  CGContextSaveGState(context);
  CGContextSetInterpolationQuality(
    context, kCGInterpolationHigh);
  CGContextSetShouldAntialias(context, true);
  CGContextTranslateCTM(
    context,
    static_cast<CGFloat>(destinationX),
    static_cast<CGFloat>(
      destinationY + destinationHeight));
  CGContextScaleCTM(context, 1.0, -1.0);
  if (sourceFlipped) {
    CGContextTranslateCTM(
      context, 0.0,
      static_cast<CGFloat>(destinationHeight));
    CGContextScaleCTM(context, 1.0, -1.0);
  }
  CGContextDrawImage(
    context,
    CGRectMake(
      0.0, 0.0,
      static_cast<CGFloat>(destinationWidth),
      static_cast<CGFloat>(destinationHeight)),
    image);
  CGContextRestoreGState(context);
  CGImageRelease(image);
  return true;
}

#endif

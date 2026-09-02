#include "native_teleprompt_mac.h"

#ifdef __APPLE__

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>

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
  bool canHide = true;
  bool hidesOnDeactivate = false;
};

std::map<void*, SavedWindowState> g_fullscreenTeleprompts;
std::map<void*, CALayer*> g_videoPresentationLayers;

struct VideoFrameProviderStorage {
  std::shared_ptr<const std::vector<std::uint8_t>> pixels;
};

void releaseVideoFrameProvider(
  void* info,
  const void*,
  std::size_t)
{
  delete static_cast<VideoFrameProviderStorage*>(info);
}

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

NSView* telepromptViewFromSwellHandle(void* swellWindow)
{
  if (!swellWindow) return nil;
  id object = (id)swellWindow;
  if ([object isKindOfClass:[NSWindow class]]) {
    return [(NSWindow*)object contentView];
  }
  if ([object isKindOfClass:[NSView class]]) {
    return (NSView*)object;
  }
  return nil;
}

NSInteger telepromptFullscreenLevel()
{
  // Esse nível permanece acima da barra de menus inclusive quando outro
  // aplicativo assume o foco. Diferente das presentationOptions, ele afeta
  // somente a janela do Teleprompt e não remove a barra da tela principal.
  return static_cast<NSInteger>(
    CGWindowLevelForKey(kCGScreenSaverWindowLevelKey));
}

void maintainFullscreenWindow(NSWindow* window)
{
  if (!window) return;
  const NSInteger requiredLevel = telepromptFullscreenLevel();
  if ([window level] != requiredLevel) {
    [window setLevel:requiredLevel];
  }
  if ([window hidesOnDeactivate]) {
    [window setHidesOnDeactivate:NO];
  }
  if ([window canHide]) {
    [window setCanHide:NO];
  }
  if (![window isVisible]) {
    [window orderFrontRegardless];
  }
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
    saved.canHide = [window canHide];
    saved.hidesOnDeactivate = [window hidesOnDeactivate];
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
    [window setCanHide:NO];
    [window setHidesOnDeactivate:NO];
    [window setLevel:telepromptFullscreenLevel()];
    [window setCollectionBehavior:
      saved.collectionBehavior |
      NSWindowCollectionBehaviorCanJoinAllSpaces |
      NSWindowCollectionBehaviorFullScreenAuxiliary |
      NSWindowCollectionBehaviorStationary |
      NSWindowCollectionBehaviorIgnoresCycle];
    if (screen) {
      [window setFrame:[screen frame] display:YES animate:NO];
    }
    // Ao entrar em tela cheia, esta janela precisa receber o teclado para o
    // Esc chegar ao WndProc. A manutencao posterior nao rouba foco.
    [window makeKeyAndOrderFront:nil];
    maintainFullscreenWindow(window);
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
  [window setCanHide:saved.canHide];
  [window setHidesOnDeactivate:saved.hidesOnDeactivate];
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

extern "C" void VSHookMacMaintainTelepromptFullscreen(
  void* swellWindow)
{
  if (![NSThread isMainThread]) return;
  if (g_fullscreenTeleprompts.find(swellWindow) ==
      g_fullscreenTeleprompts.end()) {
    return;
  }
  NSWindow* window =
    telepromptWindowFromSwellHandle(swellWindow);
  if (!window) {
    g_fullscreenTeleprompts.erase(swellWindow);
    return;
  }
  maintainFullscreenWindow(window);
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

bool VSHookMacPresentVideoFrame(
  void* swellWindow,
  const std::shared_ptr<const std::vector<std::uint8_t>>& storage,
  std::size_t pixelOffset,
  int sourceWidth,
  int sourceHeight,
  int sourceRowSpanPixels,
  bool stretch)
{
  if (![NSThread isMainThread] || !storage ||
      sourceWidth <= 0 || sourceHeight <= 0 ||
      sourceRowSpanPixels < sourceWidth) {
    return false;
  }
  const std::size_t bytesPerRow =
    static_cast<std::size_t>(sourceRowSpanPixels) * 4u;
  const std::size_t byteCount =
    bytesPerRow * static_cast<std::size_t>(sourceHeight);
  if (pixelOffset > storage->size() ||
      byteCount > storage->size() - pixelOffset) {
    return false;
  }

  NSView* view = telepromptViewFromSwellHandle(swellWindow);
  if (!view) return false;
  [view setWantsLayer:YES];
  CALayer* hostLayer = [view layer];
  if (!hostLayer) return false;

  CALayer* videoLayer = nil;
  const auto existing = g_videoPresentationLayers.find(swellWindow);
  if (existing != g_videoPresentationLayers.end()) {
    videoLayer = existing->second;
  }
  if (!videoLayer || [videoLayer superlayer] != hostLayer) {
    if (videoLayer) [videoLayer removeFromSuperlayer];
    videoLayer = [CALayer layer];
    [videoLayer setAutoresizingMask:
      kCALayerWidthSizable | kCALayerHeightSizable];
    [videoLayer setBackgroundColor:
      CGColorGetConstantColor(kCGColorBlack)];
    [videoLayer setMinificationFilter:kCAFilterLinear];
    [videoLayer setMagnificationFilter:kCAFilterLinear];
    [hostLayer addSublayer:videoLayer];
    g_videoPresentationLayers[swellWindow] = videoLayer;
  }

  auto* providerStorage = new VideoFrameProviderStorage{storage};
  CGDataProviderRef provider = CGDataProviderCreateWithData(
    providerStorage,
    storage->data() + pixelOffset,
    byteCount,
    releaseVideoFrameProvider);
  if (!provider) {
    delete providerStorage;
    return false;
  }
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGImageRef image = colorSpace
    ? CGImageCreate(
        static_cast<std::size_t>(sourceWidth),
        static_cast<std::size_t>(sourceHeight),
        8, 32, bytesPerRow, colorSpace,
        kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
        provider, nullptr, false, kCGRenderingIntentDefault)
    : nullptr;
  if (colorSpace) CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);
  if (!image) return false;

  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [videoLayer setFrame:[hostLayer bounds]];
  [videoLayer setContentsScale:std::max(
    1.0, static_cast<double>([[view window] backingScaleFactor]))];
  [videoLayer setContentsGravity:
    stretch ? kCAGravityResize : kCAGravityResizeAspect];
  [videoLayer setContents:(id)image];
  [videoLayer setHidden:NO];
  [CATransaction commit];
  CGImageRelease(image);
  return true;
}

void VSHookMacClearVideoFrame(void* swellWindow)
{
  const auto existing = g_videoPresentationLayers.find(swellWindow);
  if (existing == g_videoPresentationLayers.end()) return;
  CALayer* layer = existing->second;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [layer setContents:nil];
  [layer removeFromSuperlayer];
  [CATransaction commit];
  g_videoPresentationLayers.erase(existing);
}

extern "C" bool VSHookMacDrawTelepromptBitmap(
  void* swellDeviceContext,
  const void* bgraPixels,
  int sourceWidth,
  int sourceHeight,
  int sourceRowSpanPixels,
  bool sourceFlipped,
  bool fastScaling,
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
  // O quadro de video e opaco. Copia-lo diretamente evita que o estado de
  // composicao herdado do SWELL misture suas cores com o frame anterior.
  // Textos, relogios e demais overlays sao desenhados normalmente depois.
  if (fastScaling) {
    CGContextSetBlendMode(context, kCGBlendModeCopy);
  }
  CGContextSetInterpolationQuality(
    context, fastScaling
      ? kCGInterpolationMedium
      : kCGInterpolationHigh);
  CGContextSetShouldAntialias(context, !fastScaling);
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

#include "native_teleprompt_mac.h"

#ifdef __APPLE__

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <dispatch/dispatch.h>

#include "swell/swell.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>

// A superficie de video fica atras do overlay SWELL. Se o AppKit, durante uma
// troca de level/tela cheia, entregar o hit a ela em vez do overlay, o evento
// deve continuar pertencendo ao Teleprompt e jamais atravessar ao REAPER.
@interface VSHookTelepromptVideoWindow : NSWindow {
@public
  NSWindow* vshOverlayWindow;
}
@end

@implementation VSHookTelepromptVideoWindow
- (BOOL)canBecomeKeyWindow { return NO; }
- (BOOL)canBecomeMainWindow { return NO; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event
{
  (void)event;
  return YES;
}
- (void)sendEvent:(NSEvent*)event
{
  const NSEventType type = [event type];
  const bool isMouseEvent =
    type == NSEventTypeLeftMouseDown ||
    type == NSEventTypeLeftMouseUp ||
    type == NSEventTypeLeftMouseDragged ||
    type == NSEventTypeRightMouseDown ||
    type == NSEventTypeRightMouseUp ||
    type == NSEventTypeRightMouseDragged ||
    type == NSEventTypeOtherMouseDown ||
    type == NSEventTypeOtherMouseUp ||
    type == NSEventTypeOtherMouseDragged;
  if (isMouseEvent && vshOverlayWindow) {
    if (type == NSEventTypeLeftMouseDown ||
        type == NSEventTypeRightMouseDown ||
        type == NSEventTypeOtherMouseDown) {
      [vshOverlayWindow makeKeyWindow];
    }
    [vshOverlayWindow sendEvent:event];
    return;
  }
  [super sendEvent:event];
}
@end

@interface VSHookTelepromptTitlebarBackground : NSView
@end

@implementation VSHookTelepromptTitlebarBackground
- (BOOL)isOpaque { return YES; }
- (NSView*)hitTest:(NSPoint)point
{
  (void)point;
  return nil;
}
- (void)drawRect:(NSRect)dirtyRect
{
  [[NSColor windowBackgroundColor] setFill];
  NSRectFill(dirtyRect);
}
@end

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

struct TelepromptVideoPresentation {
  NSWindow* overlayWindow = nil;
  NSWindow* videoWindow = nil;
  CALayer* videoLayer = nil;
  NSView* titlebarBackground = nil;
  bool overlayViewWasOpaque = true;
  bool overlayWasOpaque = true;
  NSColor* overlayBackgroundColor = nil;
  const void* presentedStorageIdentity = nullptr;
  std::size_t presentedPixelOffset = 0;
  int presentedWidth = 0;
  int presentedHeight = 0;
  int presentedRowSpanPixels = 0;
};

std::map<void*, TelepromptVideoPresentation>
  g_telepromptVideoPresentations;

struct VideoFrameProviderStorage {
  std::shared_ptr<const std::vector<std::uint8_t>> pixels;
};

struct ScheduledSwellMessage {
  void* window = nullptr;
  unsigned int message = 0;
};

void deliverScheduledSwellMessage(void* rawMessage)
{
  std::unique_ptr<ScheduledSwellMessage> scheduled(
    static_cast<ScheduledSwellMessage*>(rawMessage));
  if (!scheduled || !scheduled->window) return;
  HWND window = (HWND)scheduled->window;
  if (!IsWindow(window)) return;
  SendMessage(window, scheduled->message, 0, 0);
}

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

NSRect telepromptViewFrameOnScreen(NSView* view)
{
  if (!view || ![view window]) return NSZeroRect;
  const NSRect inWindow =
    [view convertRect:[view bounds] toView:nil];
  return [[view window] convertRectToScreen:inWindow];
}

void synchronizeTelepromptVideoWindow(
  TelepromptVideoPresentation& presentation,
  NSView* overlayView)
{
  if (!presentation.videoWindow || !overlayView) return;
  NSWindow* overlayWindow = [overlayView window];
  if (!overlayWindow) return;
  [overlayWindow setIgnoresMouseEvents:NO];
  const bool hasNormalTitlebar =
    ([overlayWindow styleMask] & NSWindowStyleMaskTitled) != 0;
  if (hasNormalTitlebar &&
      [overlayWindow titlebarAppearsTransparent]) {
    // Somente o client precisa revelar a camada de video. A barra normal nao
    // pode herdar a transparencia do backing da janela nem variar de cor com
    // os frames que passam por baixo dela.
    [overlayWindow setTitlebarAppearsTransparent:NO];
  }
  if (hasNormalTitlebar) {
    NSButton* closeButton = [overlayWindow
      standardWindowButton:NSWindowCloseButton];
    NSView* titlebarView = [closeButton superview];
    if (titlebarView) {
      if (!presentation.titlebarBackground) {
        presentation.titlebarBackground =
          [[VSHookTelepromptTitlebarBackground alloc]
            initWithFrame:[titlebarView bounds]];
        [presentation.titlebarBackground setAutoresizingMask:
          NSViewWidthSizable | NSViewHeightSizable];
      }
      if ([presentation.titlebarBackground superview] !=
          titlebarView) {
        [presentation.titlebarBackground removeFromSuperview];
        [presentation.titlebarBackground
          setFrame:[titlebarView bounds]];
        [titlebarView
          addSubview:presentation.titlebarBackground
          positioned:NSWindowBelow
          relativeTo:nil];
      }
      [presentation.titlebarBackground setHidden:NO];
      [presentation.titlebarBackground setNeedsDisplay:YES];
    }
  } else if (presentation.titlebarBackground) {
    [presentation.titlebarBackground setHidden:YES];
  }
  NSWindow* currentParent =
    [presentation.videoWindow parentWindow];
  if (currentParent != overlayWindow) {
    if (currentParent) {
      [currentParent removeChildWindow:presentation.videoWindow];
    }
    [overlayWindow addChildWindow:presentation.videoWindow
                          ordered:NSWindowBelow];
  }
  const NSRect screenFrame =
    telepromptViewFrameOnScreen(overlayView);
  if (!NSEqualRects(
        [presentation.videoWindow frame], screenFrame)) {
    [presentation.videoWindow
      setFrame:screenFrame display:NO animate:NO];
  }
  if ([presentation.videoWindow level] !=
      [overlayWindow level]) {
    [presentation.videoWindow
      setLevel:[overlayWindow level]];
  }
  if ([presentation.videoWindow hidesOnDeactivate] !=
      [overlayWindow hidesOnDeactivate]) {
    [presentation.videoWindow setHidesOnDeactivate:
      [overlayWindow hidesOnDeactivate]];
  }
  if ([presentation.videoWindow canHide] !=
      [overlayWindow canHide]) {
    [presentation.videoWindow setCanHide:
      [overlayWindow canHide]];
  }
  const NSWindowCollectionBehavior requiredBehavior =
    [overlayWindow collectionBehavior] |
      NSWindowCollectionBehaviorIgnoresCycle;
  if ([presentation.videoWindow collectionBehavior] !=
      requiredBehavior) {
    [presentation.videoWindow
      setCollectionBehavior:requiredBehavior];
  }
  if ([overlayWindow isVisible]) {
    if (![presentation.videoWindow isVisible]) {
      [presentation.videoWindow
        orderWindow:NSWindowBelow
        relativeTo:[overlayWindow windowNumber]];
    }
  } else if ([presentation.videoWindow isVisible]) {
    [presentation.videoWindow orderOut:nil];
  }
}

TelepromptVideoPresentation*
ensureTelepromptVideoPresentation(void* swellWindow)
{
  if (![NSThread isMainThread]) return nullptr;
  NSView* overlayView =
    telepromptViewFromSwellHandle(swellWindow);
  NSWindow* overlayWindow = [overlayView window];
  if (!overlayView || !overlayWindow) return nullptr;

  auto existing =
    g_telepromptVideoPresentations.find(swellWindow);
  if (existing != g_telepromptVideoPresentations.end() &&
      existing->second.overlayWindow != overlayWindow) {
    TelepromptVideoPresentation stale = existing->second;
    SetOpaque((HWND)swellWindow, stale.overlayViewWasOpaque);
    if (stale.overlayWindow) {
      [stale.overlayWindow
        setOpaque:stale.overlayWasOpaque ? YES : NO];
      [stale.overlayWindow setBackgroundColor:
        stale.overlayBackgroundColor
          ? stale.overlayBackgroundColor
          : [NSColor windowBackgroundColor]];
    }
    if (stale.videoWindow) {
      NSWindow* parent = [stale.videoWindow parentWindow];
      if (parent) [parent removeChildWindow:stale.videoWindow];
      [stale.videoWindow orderOut:nil];
      [stale.videoWindow close];
      [stale.videoWindow release];
    }
    if (stale.titlebarBackground) {
      [stale.titlebarBackground removeFromSuperview];
      [stale.titlebarBackground release];
    }
    if (stale.overlayBackgroundColor) {
      [stale.overlayBackgroundColor release];
    }
    g_telepromptVideoPresentations.erase(existing);
    existing = g_telepromptVideoPresentations.end();
  }

  if (existing == g_telepromptVideoPresentations.end()) {
    TelepromptVideoPresentation presentation;
    presentation.overlayWindow = overlayWindow;
    presentation.overlayViewWasOpaque =
      [overlayView isOpaque] != NO;
    presentation.overlayWasOpaque =
      [overlayWindow isOpaque] != NO;
    presentation.overlayBackgroundColor =
      [[overlayWindow backgroundColor] retain];

    const NSRect screenFrame =
      telepromptViewFrameOnScreen(overlayView);
    NSScreen* screen = [overlayWindow screen];
    presentation.videoWindow = [[VSHookTelepromptVideoWindow alloc]
      initWithContentRect:screenFrame
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO
                   screen:screen];
    if (!presentation.videoWindow) {
      if (presentation.overlayBackgroundColor) {
        [presentation.overlayBackgroundColor release];
      }
      return nullptr;
    }
    [presentation.videoWindow setReleasedWhenClosed:NO];
    [presentation.videoWindow setOpaque:YES];
    [presentation.videoWindow
      setBackgroundColor:[NSColor blackColor]];
    [presentation.videoWindow setHasShadow:NO];
    ((VSHookTelepromptVideoWindow*)presentation.videoWindow)
      ->vshOverlayWindow = overlayWindow;
    // A janela de baixo funciona como escudo e encaminha qualquer evento ao
    // overlay. ignoresMouseEvents faria o clique cair no grid do REAPER caso
    // a ordenacao variasse por um ciclo durante fullscreen/resize.
    [presentation.videoWindow setIgnoresMouseEvents:NO];
    [presentation.videoWindow setAnimationBehavior:
      NSWindowAnimationBehaviorNone];
    [presentation.videoWindow setHidesOnDeactivate:
      [overlayWindow hidesOnDeactivate]];
    [presentation.videoWindow setCanHide:
      [overlayWindow canHide]];
    [presentation.videoWindow setLevel:[overlayWindow level]];
    [presentation.videoWindow setCollectionBehavior:
      [overlayWindow collectionBehavior] |
        NSWindowCollectionBehaviorIgnoresCycle];

    NSView* videoView = [[NSView alloc]
      initWithFrame:NSMakeRect(
        0.0, 0.0,
        screenFrame.size.width,
        screenFrame.size.height)];
    [videoView setAutoresizingMask:
      NSViewWidthSizable | NSViewHeightSizable];
    [videoView setWantsLayer:YES];
    CALayer* hostLayer = [videoView layer];
    [hostLayer setBackgroundColor:
      CGColorGetConstantColor(kCGColorBlack)];
    [hostLayer setMasksToBounds:YES];

    presentation.videoLayer = [CALayer layer];
    [presentation.videoLayer setBackgroundColor:
      CGColorGetConstantColor(kCGColorBlack)];
    [presentation.videoLayer
      setMinificationFilter:kCAFilterLinear];
    [presentation.videoLayer
      setMagnificationFilter:kCAFilterLinear];
    [hostLayer addSublayer:presentation.videoLayer];
    [presentation.videoWindow setContentView:videoView];
    [videoView release];

    // O SWELL consulta isOpaque na propria view. Tornar somente a NSWindow
    // transparente nao basta: o compositor ainda poderia considerar todo o
    // backing store da view opaco e esconder a janela de video abaixo.
    SetOpaque((HWND)swellWindow, false);
    [overlayWindow setOpaque:NO];
    [overlayWindow setBackgroundColor:[NSColor clearColor]];
    [overlayWindow addChildWindow:presentation.videoWindow
                          ordered:NSWindowBelow];

    const auto inserted =
      g_telepromptVideoPresentations.emplace(
        swellWindow, presentation);
    existing = inserted.first;
  }

  synchronizeTelepromptVideoWindow(existing->second, overlayView);
  return &existing->second;
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
  [window setIgnoresMouseEvents:NO];
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

extern "C" bool VSHookMacScheduleSwellMessage(
  void* swellWindow,
  unsigned int message)
{
  if (!swellWindow || message == 0) return false;
  auto* scheduled = new ScheduledSwellMessage;
  scheduled->window = swellWindow;
  scheduled->message = message;
  dispatch_async_f(
    dispatch_get_main_queue(),
    scheduled,
    deliverScheduledSwellMessage);
  return true;
}

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

extern "C" bool VSHookMacBeginTelepromptVideoComposition(
  void* swellWindow,
  void* swellDeviceContext,
  int clientWidth,
  int clientHeight)
{
  if (![NSThread isMainThread] || !swellWindow ||
      !swellDeviceContext || clientWidth <= 0 ||
      clientHeight <= 0 || !SWELL_GetCtxGC) {
    return false;
  }
  if (!ensureTelepromptVideoPresentation(swellWindow)) {
    return false;
  }

  CGContextRef context = (CGContextRef)(
    SWELL_GetCtxGC((HDC)swellDeviceContext));
  if (!context) return false;
  CGContextSaveGState(context);
  // A janela SWELL vira somente a camada transparente de textos. Limpar o
  // backing store em cada paint evita conservar pixels do frame antigo; a
  // janela filha preta/video permanece visivel imediatamente atras dela.
  CGContextClearRect(
    context,
    CGRectMake(
      0.0, 0.0,
      static_cast<CGFloat>(clientWidth),
      static_cast<CGFloat>(clientHeight)));
  CGContextRestoreGState(context);
  return true;
}

bool VSHookMacPresentTelepromptVideoFrame(
  void* swellWindow,
  const std::shared_ptr<const std::vector<std::uint8_t>>& storage,
  std::size_t pixelOffset,
  int sourceWidth,
  int sourceHeight,
  int sourceRowSpanPixels,
  int destinationX,
  int destinationY,
  int destinationWidth,
  int destinationHeight)
{
  if (![NSThread isMainThread] || !storage ||
      sourceWidth <= 0 || sourceHeight <= 0 ||
      sourceRowSpanPixels < sourceWidth ||
      destinationWidth <= 0 || destinationHeight <= 0) {
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

  TelepromptVideoPresentation* presentation =
    ensureTelepromptVideoPresentation(swellWindow);
  if (!presentation || !presentation->videoLayer ||
      !presentation->videoWindow) {
    return false;
  }

  NSView* videoView = [presentation->videoWindow contentView];
  CALayer* hostLayer = [videoView layer];
  if (!videoView || !hostLayer) return false;
  const bool contentsChanged =
    presentation->presentedStorageIdentity != storage.get() ||
    presentation->presentedPixelOffset != pixelOffset ||
    presentation->presentedWidth != sourceWidth ||
    presentation->presentedHeight != sourceHeight ||
    presentation->presentedRowSpanPixels != sourceRowSpanPixels;
  CGImageRef image = nullptr;
  if (contentsChanged) {
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
    image = colorSpace
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
  }

  const CGFloat hostHeight = [hostLayer bounds].size.height;
  // Os RECT/HDC do SWELL usam origem no canto superior esquerdo; CALayer usa
  // a origem inferior nessa NSView comum.
  const CGRect frame = CGRectMake(
    static_cast<CGFloat>(destinationX),
    hostHeight - static_cast<CGFloat>(
      destinationY + destinationHeight),
    static_cast<CGFloat>(destinationWidth),
    static_cast<CGFloat>(destinationHeight));
  const CGFloat contentsScale = std::max(
    1.0,
    static_cast<double>(
      [[presentation->videoWindow screen] backingScaleFactor]));
  if (!contentsChanged &&
      CGRectEqualToRect(
        [presentation->videoLayer frame], frame) &&
      std::abs(
        [presentation->videoLayer contentsScale] - contentsScale) <
          0.001 &&
      ![presentation->videoLayer isHidden]) {
    return true;
  }

  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [presentation->videoLayer setFrame:frame];
  [presentation->videoLayer setContentsScale:contentsScale];
  // O aspecto e o zoom ja foram resolvidos no mesmo calculo usado pelo
  // Windows. O compositor apenas encaixa o frame na area final.
  [presentation->videoLayer setContentsGravity:kCAGravityResize];
  if (image) {
    [presentation->videoLayer setContents:(id)image];
  }
  [presentation->videoLayer setHidden:NO];
  [CATransaction commit];
  if (image) {
    presentation->presentedStorageIdentity = storage.get();
    presentation->presentedPixelOffset = pixelOffset;
    presentation->presentedWidth = sourceWidth;
    presentation->presentedHeight = sourceHeight;
    presentation->presentedRowSpanPixels = sourceRowSpanPixels;
    CGImageRelease(image);
  }
  return true;
}

void VSHookMacClearTelepromptVideoFrame(void* swellWindow)
{
  if (![NSThread isMainThread]) return;
  const auto existing =
    g_telepromptVideoPresentations.find(swellWindow);
  if (existing == g_telepromptVideoPresentations.end()) return;
  TelepromptVideoPresentation presentation = existing->second;
  g_telepromptVideoPresentations.erase(existing);

  if (presentation.overlayWindow) {
    SetOpaque(
      (HWND)swellWindow,
      presentation.overlayViewWasOpaque);
    [presentation.overlayWindow
      setOpaque:presentation.overlayWasOpaque ? YES : NO];
    [presentation.overlayWindow setBackgroundColor:
      presentation.overlayBackgroundColor
        ? presentation.overlayBackgroundColor
        : [NSColor windowBackgroundColor]];
  }
  if (presentation.videoWindow) {
    NSWindow* parent = [presentation.videoWindow parentWindow];
    if (parent) [parent removeChildWindow:presentation.videoWindow];
    [presentation.videoWindow orderOut:nil];
    [presentation.videoWindow close];
    [presentation.videoWindow release];
  }
  if (presentation.titlebarBackground) {
    [presentation.titlebarBackground removeFromSuperview];
    [presentation.titlebarBackground release];
  }
  if (presentation.overlayBackgroundColor) {
    [presentation.overlayBackgroundColor release];
  }
}

extern "C" void VSHookMacSynchronizeTelepromptVideoWindow(
  void* swellWindow)
{
  if (![NSThread isMainThread] || !swellWindow) return;
  const auto existing =
    g_telepromptVideoPresentations.find(swellWindow);
  if (existing == g_telepromptVideoPresentations.end()) return;
  NSView* overlayView =
    telepromptViewFromSwellHandle(swellWindow);
  if (!overlayView) return;
  synchronizeTelepromptVideoWindow(existing->second, overlayView);
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

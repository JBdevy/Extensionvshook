#pragma once

#ifdef __APPLE__
extern "C" bool VSHookMacSetTelepromptFullscreen(
  void* swellWindow,
  bool fullscreen);

extern "C" void VSHookMacReleaseTelepromptFullscreen(
  void* swellWindow);

extern "C" void VSHookMacMaintainTelepromptFullscreen(
  void* swellWindow);

extern "C" double VSHookMacTelepromptBackingScale(
  void* swellWindow);

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
  int destinationHeight);
#endif

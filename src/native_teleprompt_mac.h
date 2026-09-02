#pragma once

#ifdef __APPLE__
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

extern "C" bool VSHookMacSetTelepromptFullscreen(
  void* swellWindow,
  bool fullscreen);

extern "C" void VSHookMacReleaseTelepromptFullscreen(
  void* swellWindow);

extern "C" void VSHookMacMaintainTelepromptFullscreen(
  void* swellWindow);

extern "C" double VSHookMacTelepromptBackingScale(
  void* swellWindow);

// Apresentacao exclusiva da janela de video. O quadro permanece vivo ate o
// Core Animation terminar de usa-lo e a ampliacao para tela cheia acontece
// no compositor, sem redesenhar o bitmap inteiro pelo CoreGraphics.
bool VSHookMacPresentVideoFrame(
  void* swellWindow,
  const std::shared_ptr<const std::vector<std::uint8_t>>& storage,
  std::size_t pixelOffset,
  int sourceWidth,
  int sourceHeight,
  int sourceRowSpanPixels,
  bool stretch);

void VSHookMacClearVideoFrame(void* swellWindow);

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
  int destinationHeight);
#endif

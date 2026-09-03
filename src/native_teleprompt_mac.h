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

// Agenda uma mensagem SWELL diretamente na fila principal do AppKit. No Mac,
// PostMessage e drenado por um timer interno de baixa frequencia; para quadros
// de video e scrubbing isso limita artificialmente a cadencia da janela.
extern "C" bool VSHookMacScheduleSwellMessage(
  void* swellWindow,
  unsigned int message);

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

// No Teleprompt, o video precisa permanecer abaixo dos textos/relogios. Uma
// janela transparente continua recebendo o desenho SWELL dos overlays,
// enquanto uma janela filha imediatamente atras dela hospeda o CALayer do
// video. Assim o quadro nao volta a ser redimensionado pelo CoreGraphics em
// toda pintura da interface.
extern "C" bool VSHookMacBeginTelepromptVideoComposition(
  void* swellWindow,
  void* swellDeviceContext,
  int clientWidth,
  int clientHeight);

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
  int destinationHeight);

void VSHookMacClearTelepromptVideoFrame(void* swellWindow);

// Mantem a superficie de video alinhada ao client do overlay durante resize,
// troca de monitor e entrada/saida da tela cheia.
extern "C" void VSHookMacSynchronizeTelepromptVideoWindow(
  void* swellWindow);

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

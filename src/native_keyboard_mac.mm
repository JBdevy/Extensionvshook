#include "native_keyboard_mac.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <cstring>

extern "C" bool VSHookMacCurrentEventHasCommandModifier()
{
  @autoreleasepool {
    NSEvent* event = [NSApp currentEvent];
    if (!event) return false;
    return ([event modifierFlags] & NSEventModifierFlagCommand) != 0;
  }
}

extern "C" bool VSHookMacIsArrowKeyPressed(int direction)
{
  // Key codes físicos do macOS: seta para baixo = 125; para cima = 126.
  const CGKeyCode keyCode = direction < 0 ? 126 : 125;
  return CGEventSourceKeyState(
    kCGEventSourceStateCombinedSessionState, keyCode);
}

extern "C" std::size_t VSHookMacReadCurrentKeyText(
  char* destination,
  std::size_t capacity)
{
  if (!destination || capacity == 0) return 0;
  destination[0] = '\0';

  @autoreleasepool {
    NSEvent* event = [NSApp currentEvent];
    if (!event) return 0;

    NSString* characters = [event characters];
    if (!characters || [characters length] == 0) return 0;

    // Enter, Tab, Backspace, Escape e as teclas de funcao continuam sendo
    // tratados pelo WndProc. Eles nao podem virar texto dentro da lupa ou de
    // um campo de renomeacao.
    const unichar first = [characters characterAtIndex:0];
    if (first < 0x20 || first == 0x7f ||
        (first >= 0xf700 && first <= 0xf8ff)) {
      return 0;
    }

    const char* utf8 = [characters UTF8String];
    if (!utf8 || !*utf8) return 0;
    const std::size_t length = std::strlen(utf8);
    if (length + 1 > capacity) return 0;
    std::memcpy(destination, utf8, length + 1);
    return length;
  }
}

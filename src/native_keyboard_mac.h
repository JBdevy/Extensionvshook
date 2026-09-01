#pragma once

#include <cstddef>

#ifdef __APPLE__
extern "C" std::size_t VSHookMacReadCurrentKeyText(
  char* destination,
  std::size_t capacity);
extern "C" bool VSHookMacCurrentEventHasCommandModifier();
extern "C" bool VSHookMacIsArrowKeyPressed(int direction);
#endif

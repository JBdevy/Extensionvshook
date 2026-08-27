#include "native_video_decoder.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

bool setRuntimeDirectory(const std::string& directory)
{
#ifdef _WIN32
  const int length = MultiByteToWideChar(
    CP_UTF8, 0, directory.c_str(), -1, nullptr, 0);
  if (length <= 1) return false;
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(
    CP_UTF8, 0, directory.c_str(), -1, wide.data(), length);
  return SetEnvironmentVariableW(
    L"VSHOOK_VLC_RUNTIME", wide.c_str()) != FALSE;
#else
  return setenv(
    "VSHOOK_VLC_RUNTIME", directory.c_str(), 1) == 0;
#endif
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::cerr << "uso: teste <diretorio-vlc> <video>\n";
    return 2;
  }
  if (!setRuntimeDirectory(argv[1])) {
    std::cerr << "nao foi possivel configurar o runtime\n";
    return 3;
  }

  vshook_video::Decoder decoder;
  const auto startedAt = std::chrono::steady_clock::now();
  bool receivedFrame = false;
  std::uint64_t checksum = 0;
  int width = 0;
  int height = 0;
  for (;;) {
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - startedAt).count();
    if (elapsed > 8.0) break;
    vshook_video::DecodedFrame frame;
    if (decoder.frameAt(
          argv[2], "smoke-test", elapsed, true, 1.0,
          1280, 720, frame) && frame.pixels &&
        frame.width > 0 && frame.height > 0) {
      receivedFrame = true;
      width = frame.width;
      height = frame.height;
      const std::size_t bytes = std::min<std::size_t>(
        static_cast<std::size_t>(frame.stride) * frame.height,
        65536u);
      for (std::size_t index = 0; index < bytes; index += 97) {
        checksum = checksum * 131u + frame.pixels[index];
      }
      if (elapsed > 1.0 && checksum != 0) break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::cout << "status=" << decoder.status()
            << " frame=" << width << 'x' << height
            << " checksum=" << checksum << '\n';
  return receivedFrame && checksum != 0 ? 0 : 1;
}

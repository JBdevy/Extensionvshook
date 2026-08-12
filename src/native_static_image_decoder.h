#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vshook_static_image {

// A imagem publicada nunca e alterada depois de decodeFile() retornar. Uma
// copia deste struct mantem os pixels vivos mesmo se outra thread limpar o
// cache ou se o arquivo no disco for substituido.
struct DecodedImage {
  std::shared_ptr<const std::vector<std::uint8_t>> storage;
  const std::uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
  int stride = 0;

  explicit operator bool() const noexcept
  {
    return storage && pixels && width > 0 && height > 0 &&
      stride >= width * 4;
  }
};

enum class DecodeStatus : int {
  ok = 0,
  invalidArgument = -1,
  fileUnavailable = -2,
  unsupportedFormat = -3,
  decodeFailed = -4,
  outOfMemory = -5,
  unsupportedPlatform = -6,
  fileChangedDuringDecode = -7,
};

// Decodifica o primeiro quadro diretamente do arquivo, sem PCM_source nem o
// cache de thumbnails do REAPER. A saida e BGRA8 premultiplicada, top-down,
// sem upscale e limitada a 1920x1080 (ou 1080x1920 para retrato).
//
// A chamada e sincrona, pode ser feita na thread de paint ou numa worker e e
// protegida contra chamadas concorrentes. Depois do primeiro decode estavel,
// o cache global por caminho + tamanho + mtime apenas publica o shared_ptr.
bool decodeFile(
  const std::string& utf8Path,
  DecodedImage& output,
  DecodeStatus* status = nullptr) noexcept;

// Remove todas as versoes em cache de um caminho. Snapshots ja entregues
// continuam validos por shared_ptr.
void invalidate(const std::string& utf8Path) noexcept;

// Libera as referencias mantidas pelo cache global. Snapshots em uso pelo
// renderer continuam validos.
void clearCache() noexcept;

} // namespace vshook_static_image

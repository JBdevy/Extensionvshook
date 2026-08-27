#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vshook_video {

struct DecodedFrame {
  // Mantém o quadro publicado vivo enquanto a janela o desenha. A
  // decodificação acontece em paralelo e nunca altera este buffer.
  std::shared_ptr<const std::vector<std::uint8_t>> storage;
  const std::uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
  int stride = 0;
  double timestamp = 0.0;
};

// Reprodutor/decoder baseado exclusivamente no libVLC. O vídeo roda pelo relógio local
// depois de carregado; frameAt() envia somente mudanças de transporte e
// correções de sincronismo, e devolve imediatamente o último quadro pronto.
// Todo carregamento, seek e decode acontece fora da thread da interface.
class Decoder {
public:
  Decoder();
  ~Decoder();

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

  bool frameAt(
    const std::string& utf8Path,
    const std::string& playbackKey,
    double sourceTime,
    bool playing,
    double playbackRate,
    int requestedWidth,
    int requestedHeight,
    DecodedFrame& output);
  void reset();
  int status() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vshook_video

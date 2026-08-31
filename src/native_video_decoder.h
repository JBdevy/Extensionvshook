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
  std::uint64_t sequence = 0;
};

// Decoder FFmpeg dirigido exclusivamente pelo relogio do REAPER. frameAt()
// devolve imediatamente o ultimo quadro pronto; abertura, seek e decode
// acontecem na worker. O FFmpeg nunca possui um relogio de transporte
// independente, evitando drift, quadro antecipado no Stop e estado Ended.
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

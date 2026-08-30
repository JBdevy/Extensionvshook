#include "native_video_decoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using SteadyClock = std::chrono::steady_clock;

struct FrameObservation {
  double timestamp = -1.0;
  std::uint64_t checksum = 0;
  int width = 0;
  int height = 0;
};

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

bool observeFrame(
  vshook_video::Decoder& decoder,
  const std::string& path,
  const std::string& playbackKey,
  double sourceTime,
  bool playing,
  FrameObservation& observation)
{
  vshook_video::DecodedFrame frame;
  if (!decoder.frameAt(
        path, playbackKey, sourceTime, playing, 1.0,
        1280, 720, frame) ||
      !frame.pixels || frame.width <= 0 || frame.height <= 0 ||
      frame.stride < frame.width * 4) {
    return false;
  }

  observation.timestamp = frame.timestamp;
  observation.width = frame.width;
  observation.height = frame.height;
  const std::size_t bytes = std::min<std::size_t>(
    static_cast<std::size_t>(frame.stride) *
      static_cast<std::size_t>(frame.height),
    65536u);
  std::uint64_t checksum = 0;
  for (std::size_t index = 0; index < bytes; index += 97) {
    checksum = checksum * 131u + frame.pixels[index];
  }
  observation.checksum = checksum;
  return true;
}

void waitForDecoder()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 3 || argc > 4) {
    std::cerr <<
      "uso: teste <diretorio-vlc> <video> [duracao-segundos]\n";
    return 2;
  }
  if (!setRuntimeDirectory(argv[1])) {
    std::cerr << "nao foi possivel configurar o runtime\n";
    return 3;
  }

  const std::string videoPath = argv[2];
  const double sourceDuration = argc == 4
    ? std::strtod(argv[3], nullptr) : 0.0;
  const std::string playbackKey = "smoke-test";
  vshook_video::Decoder decoder;

  // 1. Nao basta receber um unico snapshot: durante Play o decoder precisa
  // publicar varios quadros com timestamps realmente progressivos.
  const auto playStartedAt = SteadyClock::now();
  double firstPlayTimestamp = -1.0;
  double lastPlayTimestamp = -1.0;
  int distinctPlayFrames = 0;
  std::uint64_t checksum = 0;
  int width = 0;
  int height = 0;
  bool playAdvanced = false;
  while (std::chrono::duration<double>(
           SteadyClock::now() - playStartedAt).count() <= 8.0) {
    const double elapsed = std::chrono::duration<double>(
      SteadyClock::now() - playStartedAt).count();
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          elapsed, true, observation)) {
      width = observation.width;
      height = observation.height;
      checksum = checksum * 131u + observation.checksum;
      if (firstPlayTimestamp < 0.0) {
        firstPlayTimestamp = observation.timestamp;
        lastPlayTimestamp = observation.timestamp;
        distinctPlayFrames = 1;
      } else if (observation.timestamp >
                   lastPlayTimestamp + 0.005) {
        lastPlayTimestamp = observation.timestamp;
        ++distinctPlayFrames;
      }
      if (distinctPlayFrames >= 3 &&
          lastPlayTimestamp - firstPlayTimestamp >= 0.35) {
        playAdvanced = true;
        break;
      }
    }
    waitForDecoder();
  }
  if (!playAdvanced) {
    std::cerr << "video nao avancou durante o primeiro Play; status="
              << decoder.status() << " frames=" << distinctPlayFrames
              << " intervalo="
              << (firstPlayTimestamp >= 0.0
                    ? lastPlayTimestamp - firstPlayTimestamp
                    : 0.0)
              << '\n';
    return 4;
  }

  // 2. Com a mesma playbackKey, parar e buscar para tras deve substituir o
  // quadro do Play pelo quadro exato do cursor, sem herdar o snapshot antigo.
  constexpr double seekTarget = 0.10;
  const auto seekStartedAt = SteadyClock::now();
  FrameObservation pausedFrame;
  bool seekCompleted = false;
  while (std::chrono::duration<double>(
           SteadyClock::now() - seekStartedAt).count() <= 3.0) {
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          seekTarget, false, observation) &&
        std::abs(observation.timestamp - seekTarget) <= 0.03 &&
        lastPlayTimestamp - observation.timestamp >= 0.10) {
      pausedFrame = observation;
      seekCompleted = true;
      break;
    }
    waitForDecoder();
  }
  if (!seekCompleted) {
    std::cerr << "pausa/seek nao publicou o quadro solicitado; status="
              << decoder.status() << " ultimoPlay="
              << lastPlayTimestamp << '\n';
    return 5;
  }

  // Deixa qualquer callback que ja estava em voo terminar e confirma que o
  // quadro nao continua mudando enquanto o transporte permanece parado.
  const auto pauseSettlingAt = SteadyClock::now();
  while (std::chrono::duration<double>(
           SteadyClock::now() - pauseSettlingAt).count() <= 0.25) {
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          seekTarget, false, observation)) {
      pausedFrame = observation;
    }
    waitForDecoder();
  }
  const auto pauseHoldAt = SteadyClock::now();
  bool pauseStable = true;
  while (std::chrono::duration<double>(
           SteadyClock::now() - pauseHoldAt).count() <= 0.35) {
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          seekTarget, false, observation) &&
        (std::abs(observation.timestamp - seekTarget) > 0.03 ||
         observation.checksum != pausedFrame.checksum)) {
      pauseStable = false;
      break;
    }
    waitForDecoder();
  }
  if (!pauseStable) {
    std::cerr << "quadro continuou avancando durante a pausa; status="
              << decoder.status() << '\n';
    return 6;
  }

  // 3. A retomada da mesma midia/chave precisa voltar a publicar quadros. E
  // este trecho que detecta o bug em que activePlaying ficava true, mas o VLC
  // permanecia parado no quadro obtido pelo seek.
  const auto resumeStartedAt = SteadyClock::now();
  double firstResumeTimestamp = pausedFrame.timestamp;
  double lastResumeTimestamp = pausedFrame.timestamp;
  int distinctResumeFrames = 1;
  bool resumeAdvanced = false;
  while (std::chrono::duration<double>(
           SteadyClock::now() - resumeStartedAt).count() <= 5.0) {
    const double elapsed = std::chrono::duration<double>(
      SteadyClock::now() - resumeStartedAt).count();
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          seekTarget + elapsed, true, observation)) {
      checksum = checksum * 131u + observation.checksum;
      if (observation.timestamp > lastResumeTimestamp + 0.005) {
        lastResumeTimestamp = observation.timestamp;
        ++distinctResumeFrames;
      }
      if (distinctResumeFrames >= 3 &&
          lastResumeTimestamp - firstResumeTimestamp >= 0.35) {
        resumeAdvanced = true;
        break;
      }
    }
    waitForDecoder();
  }
  if (!resumeAdvanced) {
    std::cerr << "video nao voltou a avancar depois da pausa; status="
              << decoder.status() << " frames=" << distinctResumeFrames
              << " intervalo="
              << lastResumeTimestamp - firstResumeTimestamp << '\n';
    return 7;
  }

  // 4. Um item com B_LOOPSRC volta do fim da fonte para o inicio sem trocar a
  // playbackKey. Essa emenda precisa tirar o VLC de Ended e substituir o
  // ultimo quadro pelo primeiro quadro do novo ciclo.
  const auto beforeLoopStartedAt = SteadyClock::now();
  double beforeLoopTimestamp = -1.0;
  while (std::chrono::duration<double>(
           SteadyClock::now() - beforeLoopStartedAt).count() <= 3.0) {
    const double elapsed = std::chrono::duration<double>(
      SteadyClock::now() - beforeLoopStartedAt).count();
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          1.0 + elapsed, true, observation)) {
      beforeLoopTimestamp = observation.timestamp;
      if (beforeLoopTimestamp >= 1.20) break;
    }
    waitForDecoder();
  }
  if (beforeLoopTimestamp < 1.20) {
    std::cerr << "nao foi possivel preparar a emenda de loop; status="
              << decoder.status() << " timestamp="
              << beforeLoopTimestamp << '\n';
    return 8;
  }

  const auto loopStartedAt = SteadyClock::now();
  double firstLoopTimestamp = -1.0;
  double lastLoopTimestamp = -1.0;
  int distinctLoopFrames = 0;
  bool loopAdvanced = false;
  while (std::chrono::duration<double>(
           SteadyClock::now() - loopStartedAt).count() <= 5.0) {
    const double elapsed = std::chrono::duration<double>(
      SteadyClock::now() - loopStartedAt).count();
    FrameObservation observation;
    if (observeFrame(
          decoder, videoPath, playbackKey,
          0.05 + elapsed, true, observation)) {
      const bool crossedLoop =
        observation.timestamp < beforeLoopTimestamp - 0.30;
      if (firstLoopTimestamp < 0.0 && crossedLoop) {
        firstLoopTimestamp = observation.timestamp;
        lastLoopTimestamp = observation.timestamp;
        distinctLoopFrames = 1;
      } else if (firstLoopTimestamp >= 0.0 &&
                 observation.timestamp >
                   lastLoopTimestamp + 0.005) {
        lastLoopTimestamp = observation.timestamp;
        ++distinctLoopFrames;
      }
      if (distinctLoopFrames >= 3 &&
          lastLoopTimestamp - firstLoopTimestamp >= 0.25) {
        loopAdvanced = true;
        break;
      }
    }
    waitForDecoder();
  }
  if (!loopAdvanced) {
    std::cerr << "video nao reiniciou na emenda do loop; status="
              << decoder.status() << " antes="
              << beforeLoopTimestamp << " frames="
              << distinctLoopFrames << " intervalo="
              << (firstLoopTimestamp >= 0.0
                    ? lastLoopTimestamp - firstLoopTimestamp
                    : 0.0)
              << '\n';
    return 9;
  }

  // 5. Quando a duracao e fornecida, deixa o player chegar ao fim fisico da
  // fonte (estado Ended do VLC) e volta ao inicio com a mesma chave. Isso
  // reproduz o caso real de um item esticado/repetido no REAPER.
  int physicalLoopFrames = 0;
  double physicalLoopAdvance = 0.0;
  if (std::isfinite(sourceDuration) && sourceDuration > 1.0) {
    const double nearEnd = std::max(0.05, sourceDuration - 0.25);
    const auto endSeekStartedAt = SteadyClock::now();
    bool reachedEndSection = false;
    while (std::chrono::duration<double>(
             SteadyClock::now() - endSeekStartedAt).count() <= 4.0) {
      FrameObservation observation;
      if (observeFrame(
            decoder, videoPath, playbackKey,
            nearEnd, true, observation) &&
          observation.timestamp >= nearEnd - 0.10) {
        reachedEndSection = true;
        break;
      }
      waitForDecoder();
    }
    if (!reachedEndSection) {
      std::cerr << "nao foi possivel buscar o fim fisico; status="
                << decoder.status() << " alvo=" << nearEnd << '\n';
      return 10;
    }

    // Mantem o relogio solicitado avançando alem do fim por tempo suficiente
    // para o VLC publicar Ended, como acontece antes da emenda de B_LOOPSRC.
    const auto endingAt = SteadyClock::now();
    while (std::chrono::duration<double>(
             SteadyClock::now() - endingAt).count() <= 1.10) {
      const double elapsed = std::chrono::duration<double>(
        SteadyClock::now() - endingAt).count();
      FrameObservation ignored;
      observeFrame(
        decoder, videoPath, playbackKey,
        nearEnd + elapsed, true, ignored);
      waitForDecoder();
    }

    const auto physicalLoopAt = SteadyClock::now();
    double firstPhysicalLoopTimestamp = -1.0;
    double lastPhysicalLoopTimestamp = -1.0;
    bool physicalLoopRecovered = false;
    while (std::chrono::duration<double>(
             SteadyClock::now() - physicalLoopAt).count() <= 6.0) {
      const double elapsed = std::chrono::duration<double>(
        SteadyClock::now() - physicalLoopAt).count();
      FrameObservation observation;
      if (observeFrame(
            decoder, videoPath, playbackKey,
            0.05 + elapsed, true, observation)) {
        const bool returnedToStart =
          observation.timestamp < sourceDuration - 0.50;
        if (firstPhysicalLoopTimestamp < 0.0 && returnedToStart) {
          firstPhysicalLoopTimestamp = observation.timestamp;
          lastPhysicalLoopTimestamp = observation.timestamp;
          physicalLoopFrames = 1;
        } else if (firstPhysicalLoopTimestamp >= 0.0 &&
                   observation.timestamp >
                     lastPhysicalLoopTimestamp + 0.005) {
          lastPhysicalLoopTimestamp = observation.timestamp;
          ++physicalLoopFrames;
        }
        physicalLoopAdvance = firstPhysicalLoopTimestamp >= 0.0
          ? lastPhysicalLoopTimestamp - firstPhysicalLoopTimestamp
          : 0.0;
        if (physicalLoopFrames >= 3 &&
            physicalLoopAdvance >= 0.25) {
          physicalLoopRecovered = true;
          break;
        }
      }
      waitForDecoder();
    }
    if (!physicalLoopRecovered) {
      std::cerr <<
        "video nao saiu de Ended no loop fisico; status="
        << decoder.status() << " frames=" << physicalLoopFrames
        << " intervalo=" << physicalLoopAdvance << '\n';
      return 11;
    }
  }

  std::cout << "status=" << decoder.status()
            << " frame=" << width << 'x' << height
            << " checksum=" << checksum
            << " playFrames=" << distinctPlayFrames
            << " playAdvance="
            << lastPlayTimestamp - firstPlayTimestamp
            << " seek=" << pausedFrame.timestamp
            << " resumeFrames=" << distinctResumeFrames
            << " resumeAdvance="
            << lastResumeTimestamp - firstResumeTimestamp
            << " loopFrames=" << distinctLoopFrames
            << " loopAdvance="
            << lastLoopTimestamp - firstLoopTimestamp
            << " physicalLoopFrames=" << physicalLoopFrames
            << " physicalLoopAdvance=" << physicalLoopAdvance
            << '\n';
  return 0;
}

#include "native_video_decoder.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
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
  std::uint64_t sequence = 0;
  int width = 0;
  int height = 0;
  bool hasNonzeroPixel = false;
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
    L"VSHOOK_FFMPEG_RUNTIME", wide.c_str()) != FALSE;
#else
  return setenv(
    "VSHOOK_FFMPEG_RUNTIME", directory.c_str(), 1) == 0;
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
  observation.sequence = frame.sequence;
  observation.width = frame.width;
  observation.height = frame.height;
  const std::size_t bytes = std::min<std::size_t>(
    static_cast<std::size_t>(frame.stride) *
      static_cast<std::size_t>(frame.height),
    65536u);
  std::uint64_t checksum = 0;
  for (std::size_t index = 0; index < bytes; index += 97) {
    const std::uint8_t value = frame.pixels[index];
    checksum = checksum * 131u + value;
    observation.hasNonzeroPixel =
      observation.hasNonzeroPixel || value != 0;
  }
  observation.checksum = checksum;
  return true;
}

void waitForDecoder()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

bool waitForStoppedTarget(
  vshook_video::Decoder& decoder,
  const std::string& path,
  const std::string& playbackKey,
  double target,
  FrameObservation& observation,
  double timeoutSeconds = 3.0)
{
  const auto startedAt = SteadyClock::now();
  while (std::chrono::duration<double>(
           SteadyClock::now() - startedAt).count() <=
         timeoutSeconds) {
    FrameObservation candidate;
    if (observeFrame(
          decoder, path, playbackKey,
          target, false, candidate) &&
        std::abs(candidate.timestamp - target) <= 0.03) {
      observation = candidate;
      return true;
    }
    waitForDecoder();
  }
  return false;
}

bool settleAndHoldStoppedTarget(
  vshook_video::Decoder& decoder,
  const std::string& path,
  const std::string& playbackKey,
  double target,
  FrameObservation& settledFrame)
{
  // Um callback que ja estava em voo pode chegar logo depois do seek. Deixa
  // o decoder estabilizar e usa o ultimo quadro correto como referencia.
  const auto settlingAt = SteadyClock::now();
  while (std::chrono::duration<double>(
           SteadyClock::now() - settlingAt).count() <= 0.25) {
    FrameObservation observation;
    if (observeFrame(
          decoder, path, playbackKey,
          target, false, observation) &&
        std::abs(observation.timestamp - target) <= 0.03) {
      settledFrame = observation;
    }
    waitForDecoder();
  }

  const auto holdAt = SteadyClock::now();
  while (std::chrono::duration<double>(
           SteadyClock::now() - holdAt).count() <= 0.35) {
    FrameObservation observation;
    if (observeFrame(
          decoder, path, playbackKey,
          target, false, observation) &&
        (std::abs(observation.timestamp - target) > 0.03 ||
         observation.checksum != settledFrame.checksum)) {
      return false;
    }
    waitForDecoder();
  }
  return true;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 3 || argc > 5) {
    std::cerr <<
      "uso: teste <diretorio-ffmpeg> <video> [duracao-segundos] "
      "[--cadence]\n";
    return 2;
  }
  if (!setRuntimeDirectory(argv[1])) {
    std::cerr << "nao foi possivel configurar o runtime\n";
    return 3;
  }

  const std::string videoPath = argv[2];
  const double sourceDuration = argc == 4
    ? std::strtod(argv[3], nullptr)
    : (argc == 5 ? std::strtod(argv[3], nullptr) : 0.0);
  const bool measureCadence =
    argc == 5 && std::string(argv[4]) == "--cadence";
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
  bool receivedNonzeroPixel = false;
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
      receivedNonzeroPixel =
        receivedNonzeroPixel || observation.hasNonzeroPixel;
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
    std::cerr << "nenhum fluxo de quadros durante o primeiro Play; status="
              << decoder.status() << " frames=" << distinctPlayFrames
              << " intervalo="
              << (firstPlayTimestamp >= 0.0
                    ? lastPlayTimestamp - firstPlayTimestamp
                    : 0.0)
              << '\n';
    return 4;
  }
  if (!receivedNonzeroPixel) {
    std::cerr <<
      "decoder publicou somente buffers vazios; status="
      << decoder.status() << " frame=" << width << 'x' << height
      << '\n';
    return 5;
  }

  // 2. Com a mesma playbackKey, parar e buscar para tras deve substituir o
  // quadro do Play pelo quadro exato do cursor, sem herdar o snapshot antigo.
  constexpr double seekTarget = 0.10;
  FrameObservation pausedFrame;
  if (!waitForStoppedTarget(
        decoder, videoPath, playbackKey,
        seekTarget, pausedFrame) ||
      lastPlayTimestamp - pausedFrame.timestamp < 0.10) {
    std::cerr << "pausa/seek nao publicou o quadro solicitado; status="
              << decoder.status() << " ultimoPlay="
              << lastPlayTimestamp << '\n';
    return 6;
  }

  if (!settleAndHoldStoppedTarget(
        decoder, videoPath, playbackKey,
        seekTarget, pausedFrame)) {
    std::cerr << "quadro continuou avancando durante a pausa; status="
              << decoder.status() << '\n';
    return 7;
  }

  // 3. Com o transporte ainda parado, cada mudanca do cursor precisa
  // publicar o quadro do novo alvo e estabilizar nele. Isso protege contra o
  // ciclo set_time + next_frame que fazia a imagem iniciar e voltar sozinha.
  constexpr std::array<double, 3> stoppedCursorTargets = {
    0.35, 0.75, 0.20
  };
  int stoppedCursorChanges = 0;
  for (const double target : stoppedCursorTargets) {
    FrameObservation targetFrame;
    if (!waitForStoppedTarget(
          decoder, videoPath, playbackKey,
          target, targetFrame)) {
      std::cerr <<
        "cursor parado nao publicou o novo alvo; status="
        << decoder.status() << " alvo=" << target
        << " alteracoes=" << stoppedCursorChanges << '\n';
      return 8;
    }
    if (!settleAndHoldStoppedTarget(
          decoder, videoPath, playbackKey,
          target, targetFrame)) {
      std::cerr <<
        "quadro oscilou depois de mudar o cursor parado; status="
        << decoder.status() << " alvo=" << target
        << " alteracoes=" << stoppedCursorChanges << '\n';
      return 9;
    }
    pausedFrame = targetFrame;
    ++stoppedCursorChanges;
  }

  // 4. A retomada da mesma midia/chave precisa voltar a publicar quadros. E
  // este trecho detecta se o decoder deixa de acompanhar o relogio solicitado
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
          pausedFrame.timestamp + elapsed, true, observation)) {
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
    return 10;
  }

  // 5. Um item com B_LOOPSRC volta do fim da fonte para o inicio sem trocar a
  // playbackKey. Essa emenda precisa substituir imediatamente o
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
    return 11;
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
    return 12;
  }

  // 6. Quando a duracao e fornecida, deixa o player chegar repetidas vezes ao
  // fim fisico da fonte e volta ao inicio com a mesma
  // chave. Isso reproduz B_LOOPSRC em um item esticado e tambem limita o tempo
  // em que o ultimo quadro pode ficar preso antes do ciclo seguinte aparecer.
  constexpr int requiredPhysicalLoops = 3;
  constexpr double maximumLoopRecoverySeconds = 0.85;
  int physicalLoopCycles = 0;
  int physicalLoopFrames = 0;
  double physicalLoopAdvance = 0.0;
  double maximumPhysicalLoopRecovery = 0.0;
  if (std::isfinite(sourceDuration) && sourceDuration > 1.0) {
    const double nearEnd = std::max(0.05, sourceDuration - 0.25);
    for (int cycle = 1; cycle <= requiredPhysicalLoops; ++cycle) {
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
                  << decoder.status() << " alvo=" << nearEnd
                  << " ciclo=" << cycle << '\n';
        return 13;
      }

      // Mantem o relogio alem do fim o bastante para detectar quadro preso
      // em Ended. O cronometro da recuperacao so comeca quando B_LOOPSRC volta
      // para o inicio, portanto essa preparacao nao mascara a latencia medida.
      const auto endingAt = SteadyClock::now();
      while (std::chrono::duration<double>(
               SteadyClock::now() - endingAt).count() <= 0.80) {
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
      double recoverySeconds = -1.0;
      int cycleFrames = 0;
      int cycleObservations = 0;
      double minimumObservedTimestamp =
        std::numeric_limits<double>::infinity();
      double maximumObservedTimestamp = -1.0;
      bool physicalLoopRecovered = false;
      while (std::chrono::duration<double>(
               SteadyClock::now() - physicalLoopAt).count() <= 2.0) {
        const double elapsed = std::chrono::duration<double>(
          SteadyClock::now() - physicalLoopAt).count();
        FrameObservation observation;
        if (observeFrame(
              decoder, videoPath, playbackKey,
              0.05 + elapsed, true, observation)) {
          ++cycleObservations;
          minimumObservedTimestamp = std::min(
            minimumObservedTimestamp, observation.timestamp);
          maximumObservedTimestamp = std::max(
            maximumObservedTimestamp, observation.timestamp);
          const bool returnedToStart = observation.timestamp <
            std::min(1.0, sourceDuration - 0.50);
          if (firstPhysicalLoopTimestamp < 0.0 && returnedToStart) {
            firstPhysicalLoopTimestamp = observation.timestamp;
            lastPhysicalLoopTimestamp = observation.timestamp;
            recoverySeconds = elapsed;
            cycleFrames = 1;
          } else if (firstPhysicalLoopTimestamp >= 0.0 &&
                     observation.timestamp >
                       lastPhysicalLoopTimestamp + 0.005) {
            lastPhysicalLoopTimestamp = observation.timestamp;
            ++cycleFrames;
          }
          const double cycleAdvance =
            firstPhysicalLoopTimestamp >= 0.0
              ? lastPhysicalLoopTimestamp -
                  firstPhysicalLoopTimestamp
              : 0.0;
          if (cycleFrames >= 3 && cycleAdvance >= 0.25) {
            physicalLoopRecovered = true;
            physicalLoopFrames += cycleFrames;
            physicalLoopAdvance += cycleAdvance;
            break;
          }
        }
        waitForDecoder();
      }
      if (!physicalLoopRecovered) {
        std::cerr <<
          "video nao saiu de Ended no loop fisico; status="
          << decoder.status() << " ciclo=" << cycle
          << " frames=" << cycleFrames << " intervalo="
          << (firstPhysicalLoopTimestamp >= 0.0
                ? lastPhysicalLoopTimestamp -
                    firstPhysicalLoopTimestamp
                : 0.0)
          << " recuperacao=" << recoverySeconds << '\n';
        std::cerr << "observacoes=" << cycleObservations
                  << " menorTimestamp="
                  << (std::isfinite(minimumObservedTimestamp)
                        ? minimumObservedTimestamp : -1.0)
                  << " maiorTimestamp="
                  << maximumObservedTimestamp << '\n';
        return 14;
      }
      if (recoverySeconds > maximumLoopRecoverySeconds) {
        std::cerr <<
          "ultimo quadro ficou preso na emenda de B_LOOPSRC; status="
          << decoder.status() << " ciclo=" << cycle
          << " recuperacao=" << recoverySeconds
          << " limite=" << maximumLoopRecoverySeconds << '\n';
        return 15;
      }
      maximumPhysicalLoopRecovery = std::max(
        maximumPhysicalLoopRecovery, recoverySeconds);
      ++physicalLoopCycles;
    }
  }

  int cadenceRequests = 0;
  int cadenceUniqueFrames = 0;
  int cadenceRepeatedFrames = 0;
  int cadenceSkippedIntervals = 0;
  int cadenceBackwardFrames = 0;
  double cadenceMaximumTimestampStep = 0.0;
  if (measureCadence) {
    vshook_video::Decoder cadenceDecoder;
    constexpr double cadenceSeconds = 5.0;
    constexpr double cadencePeriodSeconds = 1.0 / 120.0;
    constexpr double sourceFramePeriodSeconds = 1.0 / 60.0;
    const std::string cadenceKey = "cadence-test";
    FrameObservation warmFrame;
    const auto warmStartedAt = SteadyClock::now();
    while (!observeFrame(
             cadenceDecoder, videoPath, cadenceKey,
             0.0, false, warmFrame) &&
           std::chrono::duration<double>(
             SteadyClock::now() - warmStartedAt).count() < 3.0) {
      waitForDecoder();
    }

    const auto cadenceStartedAt = SteadyClock::now();
    auto nextRequestAt = cadenceStartedAt;
    double lastTimestamp = -1.0;
    while (std::chrono::duration<double>(
             SteadyClock::now() - cadenceStartedAt).count() <
           cadenceSeconds) {
      const double elapsed = std::chrono::duration<double>(
        SteadyClock::now() - cadenceStartedAt).count();
      FrameObservation observation;
      if (observeFrame(
            cadenceDecoder, videoPath, cadenceKey,
            elapsed, true, observation)) {
        if (lastTimestamp < 0.0 ||
            observation.timestamp > lastTimestamp + 0.005) {
          ++cadenceUniqueFrames;
          if (lastTimestamp >= 0.0) {
            const double step = observation.timestamp - lastTimestamp;
            cadenceMaximumTimestampStep = std::max(
              cadenceMaximumTimestampStep, step);
            if (step > sourceFramePeriodSeconds * 1.55) {
              ++cadenceSkippedIntervals;
            }
          }
          lastTimestamp = observation.timestamp;
        } else if (observation.timestamp < lastTimestamp - 0.005) {
          ++cadenceBackwardFrames;
        } else {
          ++cadenceRepeatedFrames;
        }
      }
      ++cadenceRequests;
      nextRequestAt += std::chrono::duration_cast<
        SteadyClock::duration>(
          std::chrono::duration<double>(cadencePeriodSeconds));
      std::this_thread::sleep_until(nextRequestAt);
    }
  }

  std::cout << "status=" << decoder.status()
            << " frame=" << width << 'x' << height
            << " checksum=" << checksum
            << " playFrames=" << distinctPlayFrames
            << " playAdvance="
            << lastPlayTimestamp - firstPlayTimestamp
            << " seek=" << pausedFrame.timestamp
            << " stoppedCursorChanges=" << stoppedCursorChanges
            << " resumeFrames=" << distinctResumeFrames
            << " resumeAdvance="
            << lastResumeTimestamp - firstResumeTimestamp
            << " loopFrames=" << distinctLoopFrames
            << " loopAdvance="
            << lastLoopTimestamp - firstLoopTimestamp
            << " physicalLoopFrames=" << physicalLoopFrames
            << " physicalLoopAdvance=" << physicalLoopAdvance
            << " physicalLoopCycles=" << physicalLoopCycles
            << " maxPhysicalLoopRecovery="
            << maximumPhysicalLoopRecovery
            << " cadenceRequests=" << cadenceRequests
            << " cadenceUniqueFrames=" << cadenceUniqueFrames
            << " cadenceRepeatedFrames=" << cadenceRepeatedFrames
            << " cadenceSkippedIntervals=" << cadenceSkippedIntervals
            << " cadenceBackwardFrames=" << cadenceBackwardFrames
            << " cadenceMaximumTimestampStep="
            << cadenceMaximumTimestampStep
            << '\n';
  return 0;
}

#include "native_video_decoder.h"

#ifdef __APPLE__

#import <Accelerate/Accelerate.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace vshook_video {

struct Decoder::Impl {
  struct PlaybackRequest {
    std::string path;
    std::string playbackKey;
    double sourceTime = 0.0;
    double playbackRate = 1.0;
    int requestedWidth = 1;
    int requestedHeight = 1;
    bool playing = false;
    bool discontinuity = false;
    std::uint64_t serial = 0;
    std::chrono::steady_clock::time_point sampledAt =
      std::chrono::steady_clock::now();
  };

  // AVFoundation e CoreGraphics ficam exclusivamente nesta worker. A thread
  // do REAPER apenas publica o relógio desejado e lê o último snapshot pronto.
  std::mutex stateMutex;
  std::condition_variable stateChanged;
  std::thread worker;
  PlaybackRequest request;
  bool hasRequest = false;
  bool stopRequested = false;
  std::uint64_t handledRequestSerial = 0;
  std::shared_ptr<const std::vector<std::uint8_t>> publishedPixels;
  FrameReadyCallback frameReady;
  std::string publishedPath;
  std::string publishedPlaybackKey;
  int publishedWidth = 0;
  int publishedHeight = 0;
  int publishedStride = 0;
  double publishedTimestamp = -1.0;
  std::uint64_t publishedSequence = 0;
  bool hasObservedClock = false;
  std::string lastObservedPath;
  std::string lastObservedPlaybackKey;
  double lastObservedSourceTime = 0.0;
  double lastObservedPlaybackRate = 1.0;
  bool lastObservedPlaying = false;
  std::chrono::steady_clock::time_point lastObservedAt =
    std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point lastClockCorrectionAt =
    std::chrono::steady_clock::time_point::min();

  // Estado usado apenas pela worker.
  AVURLAsset* asset = nil;
  AVAssetImageGenerator* generator = nil;
  AVVideoComposition* sequentialComposition = nil;
  AVAssetReader* sequentialReader = nil;
  AVAssetReaderVideoCompositionOutput* sequentialOutput = nil;
  double sequentialFrameTimestamp = -1.0;
  double sequentialCursorTimestamp = -1.0;
  double assetDurationSeconds = -1.0;
  bool sequentialUnsupported = false;
  bool sequentialReachedEnd = false;
  bool sequentialNeedsMoreSamples = false;
  bool sequentialReaderProducedFrame = false;
  std::string path;
  std::shared_ptr<std::vector<std::uint8_t>> pixels;
  std::array<
    std::shared_ptr<std::vector<std::uint8_t>>, 3>
    pixelPool;
  std::vector<std::uint8_t> vImageWorkspace;
  std::size_t nextPixelPoolIndex = 0;
  int width = 0;
  int height = 0;
  int sourceDisplayWidth = 0;
  int sourceDisplayHeight = 0;
  int maximumWidth = 0;
  int maximumHeight = 0;
  double frameDuration = 1.0 / 30.0;
  double frameTimestamp = -1.0;
  std::atomic<int> statusCode{0};

  explicit Impl(FrameReadyCallback callback)
    : frameReady(std::move(callback))
  {
    worker = std::thread([this]() { workerLoop(); });
  }

  ~Impl()
  {
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      stopRequested = true;
    }
    stateChanged.notify_all();
    if (worker.joinable()) worker.join();
  }

  void clearSequentialReader()
  {
    [sequentialReader cancelReading];
    [sequentialOutput release];
    sequentialOutput = nil;
    [sequentialReader release];
    sequentialReader = nil;
    sequentialFrameTimestamp = -1.0;
    sequentialCursorTimestamp = -1.0;
  }

  void clearDecoder()
  {
    clearSequentialReader();
    [sequentialComposition release];
    sequentialComposition = nil;
    [generator cancelAllCGImageGeneration];
    [generator release];
    generator = nil;
    [asset release];
    asset = nil;
    path.clear();
    pixels.reset();
    pixelPool = {};
    vImageWorkspace.clear();
    nextPixelPoolIndex = 0;
    width = 0;
    height = 0;
    sourceDisplayWidth = 0;
    sourceDisplayHeight = 0;
    maximumWidth = 0;
    maximumHeight = 0;
    frameDuration = 1.0 / 30.0;
    frameTimestamp = -1.0;
    assetDurationSeconds = -1.0;
    sequentialUnsupported = false;
    sequentialReachedEnd = false;
    sequentialNeedsMoreSamples = false;
    sequentialReaderProducedFrame = false;
  }

  std::shared_ptr<std::vector<std::uint8_t>>
  acquirePixelBuffer(std::size_t byteCount)
  {
    for (std::size_t attempt = 0;
         attempt < pixelPool.size();
         ++attempt) {
      const std::size_t index =
        (nextPixelPoolIndex + attempt) % pixelPool.size();
      auto& candidate = pixelPool[index];
      if (!candidate || candidate.use_count() == 1) {
        if (!candidate) {
          candidate =
            std::make_shared<std::vector<std::uint8_t>>();
        }
        candidate->resize(byteCount);
        nextPixelPoolIndex =
          (index + 1) % pixelPool.size();
        return candidate;
      }
    }

    // A interface ainda pode estar desenhando todos os buffers do pequeno
    // pool. Substituir uma entrada é seguro porque os snapshots publicados
    // mantêm sua própria referência imutável.
    const std::size_t index =
      nextPixelPoolIndex % pixelPool.size();
    auto replacement =
      std::make_shared<std::vector<std::uint8_t>>(
        byteCount);
    pixelPool[index] = replacement;
    nextPixelPoolIndex =
      (index + 1) % pixelPool.size();
    return replacement;
  }

  void clearPublished()
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    publishedPixels.reset();
    publishedPath.clear();
    publishedPlaybackKey.clear();
    publishedWidth = 0;
    publishedHeight = 0;
    publishedStride = 0;
    publishedTimestamp = -1.0;
  }

  void calculateDecodeSize(
    int requestedWidth,
    int requestedHeight,
    int& decodeWidth,
    int& decodeHeight) const
  {
    const int wantedWidth =
      std::max(2, requestedWidth);
    const int wantedHeight =
      std::max(2, requestedHeight);
    const bool portraitSource =
      sourceDisplayHeight > sourceDisplayWidth;
    // A janela nativa do REAPER acompanha a resolucao fisica do monitor. O
    // Teleprompt tambem pode preservar essa nitidez em telas Retina/4K, sem
    // ficar preso ao antigo teto Full HD.
    const int maximumDecodeWidth =
      portraitSource ? 2160 : 3840;
    const int maximumDecodeHeight =
      portraitSource ? 3840 : 2160;
    const int quantizedWidth = std::min(
      maximumDecodeWidth,
      ((std::min(wantedWidth, maximumDecodeWidth) + 63) /
        64) * 64);
    const int quantizedHeight = std::min(
      maximumDecodeHeight,
      ((std::min(wantedHeight, maximumDecodeHeight) + 63) /
        64) * 64);
    const double sourceWidth =
      static_cast<double>(std::max(
        1, sourceDisplayWidth));
    const double sourceHeight =
      static_cast<double>(std::max(
        1, sourceDisplayHeight));
    const double scale = std::min(
      1.0,
      std::min(
        static_cast<double>(quantizedWidth) /
          sourceWidth,
        static_cast<double>(quantizedHeight) /
          sourceHeight));
    int boundedWidth = std::max(
      2, static_cast<int>(std::lround(
        sourceWidth * scale)));
    int boundedHeight = std::max(
      2, static_cast<int>(std::lround(
        sourceHeight * scale)));
    boundedWidth =
      std::min(maximumDecodeWidth, boundedWidth);
    boundedHeight =
      std::min(maximumDecodeHeight, boundedHeight);
    // O limite solicitado é quantizado, mas o render final conserva a
    // proporção da mídia. Isso evita recriar o reader por oscilações de 1 px
    // sem introduzir uma tela 16:9 em volta de conteúdo 4:3 ou ultrawide.
    decodeWidth = std::max(
      2, boundedWidth - (boundedWidth & 1));
    decodeHeight = std::max(
      2, boundedHeight - (boundedHeight & 1));
  }

  bool rebuildSequentialComposition(
    int decodeWidth,
    int decodeHeight)
  {
    [sequentialComposition release];
    sequentialComposition = nil;
    if (!asset ||
        decodeWidth <= 0 || decodeHeight <= 0) {
      return false;
    }

    NSArray<AVAssetTrack*>* videoTracks =
      [asset tracksWithMediaType:AVMediaTypeVideo];
    if ([videoTracks count] == 0) return false;
    AVAssetTrack* videoTrack =
      [videoTracks objectAtIndex:0];
    const CGSize naturalSize =
      [videoTrack naturalSize];
    const CGAffineTransform preferredTransform =
      [videoTrack preferredTransform];
    if (!std::isfinite(naturalSize.width) ||
        !std::isfinite(naturalSize.height) ||
        naturalSize.width <= 0.0 ||
        naturalSize.height <= 0.0) {
      return false;
    }

    const CGRect transformedRect = CGRectStandardize(
      CGRectApplyAffineTransform(
        CGRectMake(
          0.0, 0.0,
          naturalSize.width, naturalSize.height),
        preferredTransform));
    const double displayWidth =
      static_cast<double>(transformedRect.size.width);
    const double displayHeight =
      static_cast<double>(transformedRect.size.height);
    if (!std::isfinite(displayWidth) ||
        !std::isfinite(displayHeight) ||
        !std::isfinite(transformedRect.origin.x) ||
        !std::isfinite(transformedRect.origin.y) ||
        displayWidth <= 0.0 ||
        displayHeight <= 0.0) {
      return false;
    }
    const bool portraitSource =
      displayHeight > displayWidth;
    const int fullHdWidth =
      portraitSource ? 1080 : 1920;
    const int fullHdHeight =
      portraitSource ? 1920 : 1080;
    if (decodeWidth > fullHdWidth ||
        decodeHeight > fullHdHeight) {
      return false;
    }

    const double fittedScale = std::min(
      1.0,
      std::min(
        static_cast<double>(decodeWidth) /
          displayWidth,
        static_cast<double>(decodeHeight) /
          displayHeight));
    const double offsetX = std::max(
      0.0,
      (static_cast<double>(decodeWidth) -
        displayWidth * fittedScale) * 0.5);
    const double offsetY = std::max(
      0.0,
      (static_cast<double>(decodeHeight) -
        displayHeight * fittedScale) * 0.5);
    // Normaliza a origem produzida pelo preferredTransform, escala os seis
    // coeficientes de forma uniforme e centraliza qualquer sobra causada
    // pelo arredondamento para dimensões pares.
    const CGAffineTransform fittedTransform =
      CGAffineTransformMake(
        preferredTransform.a * fittedScale,
        preferredTransform.b * fittedScale,
        preferredTransform.c * fittedScale,
        preferredTransform.d * fittedScale,
        offsetX +
          (preferredTransform.tx -
            CGRectGetMinX(transformedRect)) *
            fittedScale,
        offsetY +
          (preferredTransform.ty -
            CGRectGetMinY(transformedRect)) *
            fittedScale);

    CMTimeRange instructionTimeRange =
      [videoTrack timeRange];
    const CMTime duration = [asset duration];
    if (CMTIME_IS_NUMERIC(duration) &&
        CMTimeCompare(duration, kCMTimeZero) > 0) {
      instructionTimeRange =
        CMTimeRangeMake(kCMTimeZero, duration);
    }
    if (!CMTIMERANGE_IS_VALID(instructionTimeRange) ||
        !CMTIME_IS_NUMERIC(instructionTimeRange.start) ||
        !CMTIME_IS_NUMERIC(instructionTimeRange.duration) ||
        CMTimeCompare(
          instructionTimeRange.duration,
          kCMTimeZero) <= 0) {
      return false;
    }

    AVMutableVideoComposition* nextComposition =
      [[AVMutableVideoComposition
        videoComposition] retain];
    AVMutableVideoCompositionInstruction* instruction =
      [AVMutableVideoCompositionInstruction
        videoCompositionInstruction];
    AVMutableVideoCompositionLayerInstruction*
      layerInstruction =
        [AVMutableVideoCompositionLayerInstruction
          videoCompositionLayerInstructionWithAssetTrack:
            videoTrack];
    if (!nextComposition ||
        !instruction || !layerInstruction) {
      [nextComposition release];
      return false;
    }

    nextComposition.renderSize =
      CGSizeMake(decodeWidth, decodeHeight);
    nextComposition.frameDuration =
      CMTimeMakeWithSeconds(
        std::max(
          1.0 / 240.0,
          std::min(1.0, frameDuration)),
        60000);
    [layerInstruction
      setTransform:fittedTransform
      atTime:instructionTimeRange.start];
    instruction.timeRange = instructionTimeRange;
    instruction.layerInstructions =
      @[layerInstruction];
    nextComposition.instructions =
      @[instruction];
    sequentialComposition = nextComposition;
    return true;
  }

  bool open(
    const std::string& utf8Path,
    int requestedWidth,
    int requestedHeight)
  {
    if (generator &&
        path == utf8Path) {
      int decodeWidth = 2;
      int decodeHeight = 2;
      calculateDecodeSize(
        requestedWidth,
        requestedHeight,
        decodeWidth,
        decodeHeight);
      if (maximumWidth != decodeWidth ||
          maximumHeight != decodeHeight) {
        // Resize não reabre o asset, mas precisa reconstruir o compositor na
        // resolução final. Manter o renderSize original faria um arquivo 4K
        // continuar criando superfícies 4K antes do downscale.
        clearSequentialReader();
        generator.maximumSize =
          CGSizeMake(decodeWidth, decodeHeight);
        maximumWidth = decodeWidth;
        maximumHeight = decodeHeight;
        sequentialUnsupported =
          !rebuildSequentialComposition(
            decodeWidth, decodeHeight);
        sequentialReachedEnd = false;
        sequentialNeedsMoreSamples = false;
        sequentialReaderProducedFrame = false;
        // O cache de pausa depende também do maximumSize. Sem invalidá-lo,
        // entrar em tela cheia parado reutilizava indefinidamente o bitmap
        // pequeno gerado antes do resize.
        pixels.reset();
        width = 0;
        height = 0;
        frameTimestamp = -1.0;
      }
      return true;
    }

    clearDecoder();
    if (utf8Path.empty()) {
      statusCode = -200;
      return false;
    }

    NSString* pathString =
      [NSString stringWithUTF8String:utf8Path.c_str()];
    if (!pathString) {
      statusCode = -201;
      return false;
    }
    NSURL* url = [NSURL fileURLWithPath:pathString];
    asset = [[AVURLAsset alloc] initWithURL:url options:nil];
    NSArray<AVAssetTrack*>* videoTracks =
      asset ? [asset tracksWithMediaType:AVMediaTypeVideo] : nil;
    if (!asset || [videoTracks count] == 0) {
      statusCode = -202;
      clearDecoder();
      return false;
    }
    AVAssetTrack* videoTrack = [videoTracks objectAtIndex:0];
    const CGSize naturalSize =
      videoTrack ? [videoTrack naturalSize] : CGSizeZero;
    const CGAffineTransform preferredTransform =
      videoTrack
        ? [videoTrack preferredTransform]
        : CGAffineTransformIdentity;
    const CGRect displayRect = CGRectApplyAffineTransform(
      CGRectMake(
        0.0, 0.0,
        naturalSize.width, naturalSize.height),
      preferredTransform);
    sourceDisplayWidth = std::max(
      1, static_cast<int>(std::lround(
        std::fabs(displayRect.size.width))));
    sourceDisplayHeight = std::max(
      1, static_cast<int>(std::lround(
        std::fabs(displayRect.size.height))));
    int decodeWidth = 2;
    int decodeHeight = 2;
    calculateDecodeSize(
      requestedWidth,
      requestedHeight,
      decodeWidth,
      decodeHeight);
    const float nominalFrameRate =
      videoTrack ? [videoTrack nominalFrameRate] : 0.0f;
    if (nominalFrameRate > 0.1f) {
      frameDuration = std::max(
        1.0 / 240.0,
        std::min(1.0,
          1.0 / static_cast<double>(nominalFrameRate)));
    }
    const CMTime assetDuration = [asset duration];
    if (CMTIME_IS_NUMERIC(assetDuration)) {
      const double seconds =
        CMTimeGetSeconds(assetDuration);
      if (std::isfinite(seconds) && seconds > 0.0) {
        assetDurationSeconds = seconds;
      }
    }

    generator =
      [[AVAssetImageGenerator alloc] initWithAsset:asset];
    generator.appliesPreferredTrackTransform = YES;
    generator.maximumSize =
      CGSizeMake(decodeWidth, decodeHeight);
    // Aceita o quadro real mais próximo dentro da metade de um frame. Uma
    // tolerância de 1/120 s forçava seeks quase exatos em cada quadro e fazia
    // o AVAssetImageGenerator reprocessar GOPs continuamente.
    const CMTime tolerance = CMTimeMakeWithSeconds(
      std::max(
        1.0 / 600.0,
        std::min(0.05, frameDuration * 0.55)),
      60000);
    generator.requestedTimeToleranceBefore = tolerance;
    generator.requestedTimeToleranceAfter = tolerance;
    // Se a composição não for válida, o ImageGenerator continua sendo um
    // fallback correto, já com a orientação e o mesmo teto Full HD.
    sequentialUnsupported =
      !rebuildSequentialComposition(
        decodeWidth, decodeHeight);
    path = utf8Path;
    maximumWidth = decodeWidth;
    maximumHeight = decodeHeight;
    statusCode = 1;
    return true;
  }

  bool startSequentialReader(double sourceTime)
  {
    if (!asset || sequentialUnsupported) return false;
    clearSequentialReader();

    NSArray<AVAssetTrack*>* videoTracks =
      [asset tracksWithMediaType:AVMediaTypeVideo];
    if ([videoTracks count] == 0) return false;
    NSArray<AVAssetTrack*>* compositionTracks =
      [NSArray arrayWithObject:
        [videoTracks objectAtIndex:0]];

    NSError* error = nil;
    AVAssetReader* nextReader =
      [[AVAssetReader alloc] initWithAsset:asset error:&error];
    if (!nextReader) {
      statusCode = error
        ? static_cast<int>([error code])
        : -206;
      sequentialUnsupported = true;
      return false;
    }

    double displayWidth =
      static_cast<double>(sourceDisplayWidth);
    double displayHeight =
      static_cast<double>(sourceDisplayHeight);
    if (sequentialComposition) {
      const CGSize renderSize =
        [sequentialComposition renderSize];
      if (renderSize.width > 0.0 &&
          renderSize.height > 0.0) {
        displayWidth = renderSize.width;
        displayHeight = renderSize.height;
      }
    }
    const double outputScale = std::min(
      1.0,
      std::min(
        static_cast<double>(
          std::max(2, maximumWidth)) /
          displayWidth,
        static_cast<double>(
          std::max(2, maximumHeight)) /
          displayHeight));
    int readerOutputWidth = std::max(
      2, static_cast<int>(std::lround(
        displayWidth * outputScale)));
    int readerOutputHeight = std::max(
      2, static_cast<int>(std::lround(
        displayHeight * outputScale)));
    readerOutputWidth -= readerOutputWidth & 1;
    readerOutputHeight -= readerOutputHeight & 1;
    NSDictionary* outputSettings = @{
      (id)kCVPixelBufferPixelFormatTypeKey:
        @(kCVPixelFormatType_32BGRA),
      (id)kCVPixelBufferWidthKey:
        @(readerOutputWidth),
      (id)kCVPixelBufferHeightKey:
        @(readerOutputHeight)
    };
    AVAssetReaderVideoCompositionOutput* nextOutput =
      [[AVAssetReaderVideoCompositionOutput alloc]
        initWithVideoTracks:compositionTracks
              videoSettings:outputSettings];
    if (!nextOutput) {
      [nextReader release];
      statusCode = -207;
      sequentialUnsupported = true;
      return false;
    }
    nextOutput.alwaysCopiesSampleData = NO;
    if (sequentialComposition) {
      nextOutput.videoComposition =
        sequentialComposition;
    }
    if (![nextReader canAddOutput:nextOutput]) {
      [nextOutput release];
      [nextReader release];
      statusCode = -208;
      sequentialUnsupported = true;
      return false;
    }
    [nextReader addOutput:nextOutput];

    const double readerStartSeconds = std::max(
      0.0, sourceTime - frameDuration * 1.5);
    const CMTime readerStart =
      CMTimeMakeWithSeconds(readerStartSeconds, 60000);
    const CMTime assetDuration = [asset duration];
    if (CMTIME_IS_NUMERIC(assetDuration) &&
        CMTimeCompare(assetDuration, readerStart) > 0) {
      nextReader.timeRange = CMTimeRangeMake(
        readerStart,
        CMTimeSubtract(assetDuration, readerStart));
    }
    if (![nextReader startReading]) {
      NSError* readerError = [nextReader error];
      statusCode = readerError
        ? static_cast<int>([readerError code])
        : -209;
      [nextOutput release];
      [nextReader release];
      sequentialUnsupported = true;
      return false;
    }

    sequentialReader = nextReader;
    sequentialOutput = nextOutput;
    sequentialFrameTimestamp = -1.0;
    sequentialCursorTimestamp = -1.0;
    sequentialReachedEnd = false;
    sequentialNeedsMoreSamples = false;
    sequentialReaderProducedFrame = false;
    return true;
  }

  bool copySequentialSample(
    CMSampleBufferRef sample,
    double timestamp)
  {
    if (!sample) return false;
    CVPixelBufferRef pixelBuffer =
      CMSampleBufferGetImageBuffer(sample);
    if (!pixelBuffer ||
        CVPixelBufferGetPixelFormatType(pixelBuffer) !=
          kCVPixelFormatType_32BGRA ||
        CVPixelBufferLockBaseAddress(
          pixelBuffer, kCVPixelBufferLock_ReadOnly) !=
          kCVReturnSuccess) {
      return false;
    }

    const std::size_t sampleWidth =
      CVPixelBufferGetWidth(pixelBuffer);
    const std::size_t sampleHeight =
      CVPixelBufferGetHeight(pixelBuffer);
    const std::size_t sourceStride =
      CVPixelBufferGetBytesPerRow(pixelBuffer);
    const auto* source =
      static_cast<const std::uint8_t*>(
        CVPixelBufferGetBaseAddress(pixelBuffer));
    const std::size_t sourceRowBytes =
      sampleWidth * 4u;
    if (!source ||
        sampleWidth == 0 || sampleHeight == 0 ||
        sourceStride < sourceRowBytes ||
        sampleWidth >
          static_cast<std::size_t>(
            std::numeric_limits<int>::max()) ||
        sampleHeight >
          static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
      CVPixelBufferUnlockBaseAddress(
        pixelBuffer, kCVPixelBufferLock_ReadOnly);
      return false;
    }

    const double outputScale = std::min(
      1.0,
      std::min(
        static_cast<double>(
          std::max(2, maximumWidth)) /
          static_cast<double>(sampleWidth),
        static_cast<double>(
          std::max(2, maximumHeight)) /
          static_cast<double>(sampleHeight)));
    const std::size_t outputWidth = std::max<std::size_t>(
      2,
      static_cast<std::size_t>(std::lround(
        sampleWidth * outputScale)));
    const std::size_t outputHeight = std::max<std::size_t>(
      2,
      static_cast<std::size_t>(std::lround(
        sampleHeight * outputScale)));
    const std::size_t outputRowBytes =
      outputWidth * 4u;
    if (outputHeight >
        std::numeric_limits<std::size_t>::max() /
          outputRowBytes) {
      CVPixelBufferUnlockBaseAddress(
        pixelBuffer, kCVPixelBufferLock_ReadOnly);
      return false;
    }
    auto nextPixels =
      acquirePixelBuffer(
        outputRowBytes * outputHeight);
    bool copied = false;
    if (outputWidth == sampleWidth &&
        outputHeight == sampleHeight) {
      for (std::size_t row = 0;
           row < sampleHeight;
           ++row) {
        std::memcpy(
          nextPixels->data() +
            row * outputRowBytes,
          source + row * sourceStride,
          outputRowBytes);
      }
      copied = true;
    } else {
      vImage_Buffer sourceBuffer{
        const_cast<std::uint8_t*>(source),
        static_cast<vImagePixelCount>(sampleHeight),
        static_cast<vImagePixelCount>(sampleWidth),
        sourceStride
      };
      vImage_Buffer destinationBuffer{
        nextPixels->data(),
        static_cast<vImagePixelCount>(outputHeight),
        static_cast<vImagePixelCount>(outputWidth),
        outputRowBytes
      };
      const vImage_Flags scaleFlags =
        kvImageHighQualityResampling;
      const vImage_Error workspaceBytes =
        vImageScale_ARGB8888(
          &sourceBuffer,
          &destinationBuffer,
          nullptr,
          scaleFlags |
            kvImageGetTempBufferSize);
      if (workspaceBytes > 0 &&
          static_cast<std::size_t>(workspaceBytes) >
            vImageWorkspace.size()) {
        vImageWorkspace.resize(
          static_cast<std::size_t>(
            workspaceBytes));
      }
      copied =
        workspaceBytes >= 0 &&
        vImageScale_ARGB8888(
          &sourceBuffer,
          &destinationBuffer,
          vImageWorkspace.empty()
            ? nullptr
            : vImageWorkspace.data(),
          scaleFlags) ==
        kvImageNoError;
    }
    CVPixelBufferUnlockBaseAddress(
      pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (!copied) return false;

    pixels = std::move(nextPixels);
    width = static_cast<int>(outputWidth);
    height = static_cast<int>(outputHeight);
    frameTimestamp = timestamp;
    sequentialFrameTimestamp = timestamp;
    sequentialCursorTimestamp = timestamp;
    sequentialReaderProducedFrame = true;
    statusCode = 4;
    return true;
  }

  double clampSourceTime(double sourceTime) const
  {
    const double safeTime = std::max(0.0, sourceTime);
    if (assetDurationSeconds <= 0.0) {
      return safeTime;
    }
    const double lastFrameTime = std::max(
      0.0,
      assetDurationSeconds -
        std::max(1.0 / 600.0, frameDuration));
    return std::min(safeTime, lastFrameTime);
  }

  bool decodeSequentialFrame(
    double sourceTime,
    bool forceRestart)
  {
    const double safeTime =
      clampSourceTime(sourceTime);
    if (forceRestart) {
      clearSequentialReader();
      sequentialReachedEnd = false;
    }
    sequentialNeedsMoreSamples = false;
    if (sequentialReachedEnd) {
      return sequentialReaderProducedFrame &&
        pixels && !pixels->empty();
    }
    const bool needsRestart =
      !sequentialReader || !sequentialOutput ||
      (sequentialCursorTimestamp >= 0.0 &&
       safeTime <
         sequentialCursorTimestamp -
           frameDuration * 0.75);
    if (needsRestart &&
        !startSequentialReader(safeTime)) {
      return false;
    }

    if (sequentialFrameTimestamp >= 0.0 &&
        safeTime <=
          sequentialFrameTimestamp + frameDuration * 0.75) {
      return pixels && !pixels->empty();
    }

    // O reader é sequencial: atraso normal descarta quadros até alcançar o
    // relógio, sem reconstruir asset/composição. Um teto amplo impede arquivo
    // corrompido de prender a worker, mas jamais publica um quadro atrasado
    // só porque o teto foi atingido.
    constexpr int maximumSamplesPerPass = 240;
    for (int attempt = 0;
         attempt < maximumSamplesPerPass;
         ++attempt) {
      CMSampleBufferRef sample =
        [sequentialOutput copyNextSampleBuffer];
      if (!sample) {
        AVAssetReaderStatus readerStatus =
          [sequentialReader status];
        NSError* readerError = [sequentialReader error];
        if (readerStatus ==
            AVAssetReaderStatusCompleted) {
          sequentialReachedEnd = true;
          statusCode = 5;
          return sequentialReaderProducedFrame &&
            pixels && !pixels->empty();
        }
        statusCode = readerError
          ? static_cast<int>([readerError code])
          : -211;
        sequentialUnsupported = true;
        clearSequentialReader();
        return false;
      }

      const CMTime presentationTime =
        CMSampleBufferGetPresentationTimeStamp(sample);
      const double timestamp =
        CMTIME_IS_NUMERIC(presentationTime)
          ? CMTimeGetSeconds(presentationTime)
          : safeTime;
      sequentialCursorTimestamp = timestamp;
      const bool reachedTarget =
        timestamp >= safeTime - frameDuration * 0.25;
      if (reachedTarget) {
        const bool copied =
          copySequentialSample(sample, timestamp);
        CFRelease(sample);
        return copied;
      }
      CFRelease(sample);
    }
    // Mantém o último snapshot visível e continua drenando imediatamente na
    // próxima passagem; não publica deliberadamente um frame defasado.
    sequentialNeedsMoreSamples = true;
    return false;
  }

  bool decodeStillFrame(double sourceTime)
  {
    if (!generator) return false;
    const double safeTime =
      clampSourceTime(sourceTime);
    if (pixels && !pixels->empty() &&
        std::abs(safeTime - frameTimestamp) <
          std::max(1.0 / 240.0, frameDuration * 0.90)) {
      return true;
    }

    const CMTime requested =
      CMTimeMakeWithSeconds(
        safeTime, 60000);
    CMTime actual = kCMTimeZero;
    NSError* error = nil;
    CGImageRef image =
      [generator copyCGImageAtTime:requested
                        actualTime:&actual
                             error:&error];
    if (!image) {
      statusCode = error
        ? static_cast<int>([error code])
        : -203;
      return false;
    }

    width = static_cast<int>(CGImageGetWidth(image));
    height = static_cast<int>(CGImageGetHeight(image));
    if (width <= 0 || height <= 0) {
      CGImageRelease(image);
      statusCode = -204;
      return false;
    }

    const std::size_t stride =
      static_cast<std::size_t>(width) * 4u;
    if (static_cast<std::size_t>(height) >
        std::numeric_limits<std::size_t>::max() /
          stride) {
      CGImageRelease(image);
      statusCode = -205;
      return false;
    }
    auto nextPixels = acquirePixelBuffer(
      stride * static_cast<std::size_t>(height));
    std::fill(
      nextPixels->begin(), nextPixels->end(), 0);
    CGColorSpaceRef colorSpace =
      CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
      nextPixels->data(), static_cast<std::size_t>(width),
      static_cast<std::size_t>(height), 8, stride,
      colorSpace,
      kCGBitmapByteOrder32Little |
        kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(colorSpace);
    if (!context) {
      CGImageRelease(image);
      statusCode = -205;
      return false;
    }

    // copyCGImageAtTime já entrega o CGImage com a transformação preferida
    // aplicada. O flip adicional usado aqui invertia somente o vídeo no Mac;
    // imagens vindas do LICE já chegavam na orientação correta.
    CGContextDrawImage(
      context,
      CGRectMake(0.0, 0.0,
        static_cast<CGFloat>(width),
        static_cast<CGFloat>(height)),
      image);
    CGContextRelease(context);
    CGImageRelease(image);
    pixels = std::move(nextPixels);

    frameTimestamp =
      CMTIME_IS_NUMERIC(actual)
        ? CMTimeGetSeconds(actual)
        : safeTime;
    statusCode = 2;
    return true;
  }

  bool decode(
    double sourceTime,
    bool playing,
    bool forceRestart)
  {
    // Depois que existe um quadro, setas e arraste são scrubbing e usam o
    // ImageGenerator: reiniciar o AVAssetReader em cada pequeno passo causa
    // atraso. No primeiro acesso frio ainda deixamos o reader sequencial
    // produzir o quadro inicial; ele acorda o pipeline bem mais rápido do que
    // copyCGImageAtTime em alguns MP4 e evita quase um segundo de tela preta.
    if (!playing && forceRestart &&
        pixels && !pixels->empty()) {
      return decodeStillFrame(sourceTime);
    }
    if (!sequentialUnsupported) {
      // Mantem o AVAssetReader preparado tambem quando o transporte esta
      // parado. Assim Play reutiliza o reader ja posicionado no cursor em vez
      // de pagar a abertura do pipeline depois que o REAPER comecou a tocar.
      if (decodeSequentialFrame(
            sourceTime, forceRestart)) {
        return true;
      }
      if (sequentialReachedEnd &&
          sequentialReaderProducedFrame &&
          pixels && !pixels->empty()) {
        return true;
      }
      if (sequentialReachedEnd) {
        // Seek direto no fim pode não publicar amostra sequencial. O
        // ImageGenerator resolve uma única imagem final sem declarar o
        // formato incompatível nem recriar readers em loop.
        const bool decodedFinal =
          decodeStillFrame(sourceTime);
        sequentialReaderProducedFrame =
          decodedFinal;
        return decodedFinal;
      }
      if (sequentialNeedsMoreSamples) {
        return pixels && !pixels->empty();
      }
      // Formatos que nao aceitem AVAssetReader continuam funcionais pelo
      // extrator de quadro.
      sequentialUnsupported = true;
      clearSequentialReader();
      return decodeStillFrame(sourceTime);
    }
    return decodeStillFrame(sourceTime);
  }

  bool publishCurrentFrame(
    const PlaybackRequest& processed)
  {
    if (!pixels || pixels->empty() ||
        width <= 0 || height <= 0) {
      return false;
    }
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      const bool compatiblePublishedFrame =
        publishedPixels &&
        publishedPath == request.path &&
        publishedPlaybackKey == request.playbackKey &&
        publishedWidth == width &&
        publishedHeight == height;
      const bool compatibleStoppedScrubFrame =
        hasRequest &&
        request.serial > processed.serial &&
        !processed.playing &&
        !request.playing &&
        processed.discontinuity &&
        request.discontinuity &&
        request.path == processed.path &&
        processed.path == path &&
        request.playbackKey == processed.playbackKey &&
        request.requestedWidth == processed.requestedWidth &&
        request.requestedHeight == processed.requestedHeight &&
        std::abs(
          request.playbackRate - processed.playbackRate) <= 0.001 &&
        std::abs(
          request.sourceTime - processed.sourceTime) > 0.000001 &&
        (!compatiblePublishedFrame ||
         std::abs(frameTimestamp - request.sourceTime) + 0.000001 <
           std::abs(
             publishedTimestamp - request.sourceTime));
      if (!hasRequest ||
          request.path != path ||
          request.playbackKey != processed.playbackKey ||
          (request.serial != processed.serial &&
           !compatibleStoppedScrubFrame)) {
        return false;
      }
      // Cada decode cria um buffer novo. Publicar o mesmo shared_ptr elimina
      // uma copia integral por quadro sem permitir que a worker altere um
      // snapshot que a interface ainda esteja desenhando.
      publishedPixels = pixels;
      publishedPath = path;
      publishedPlaybackKey = processed.playbackKey;
      publishedWidth = width;
      publishedHeight = height;
      publishedStride = width * 4;
      publishedTimestamp = frameTimestamp;
      ++publishedSequence;
    }
    if (frameReady) frameReady();
    return true;
  }

  void waitForWork(
    const PlaybackRequest& processed,
    std::chrono::milliseconds duration)
  {
    std::unique_lock<std::mutex> lock(stateMutex);
    stateChanged.wait_for(lock, duration, [&]() {
      return stopRequested ||
        !hasRequest ||
        request.serial != processed.serial;
    });
  }

  void workerLoop()
  {
    double lastPublishedTimestamp = -1.0;
    std::string lastPublishedPath;
    std::string lastPublishedPlaybackKey;
    int lastPublishedWidth = 0;
    int lastPublishedHeight = 0;
    std::uint64_t lastHandledSerial = 0;
    for (;;) {
      PlaybackRequest current;
      {
        std::unique_lock<std::mutex> lock(stateMutex);
        stateChanged.wait(lock, [&]() {
          return stopRequested || hasRequest;
        });
        if (stopRequested) break;
        current = request;
      }

      if (current.path.empty()) {
        @autoreleasepool {
          clearDecoder();
        }
        clearPublished();
        lastPublishedTimestamp = -1.0;
        lastPublishedPath.clear();
        lastPublishedPlaybackKey.clear();
        lastPublishedWidth = 0;
        lastPublishedHeight = 0;
        lastHandledSerial = current.serial;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          handledRequestSerial =
            std::max(
              handledRequestSerial, current.serial);
          if (request.serial == current.serial) {
            hasRequest = false;
          }
        }
        continue;
      }

      const auto now = std::chrono::steady_clock::now();
      const double elapsed =
        current.playing
          ? std::chrono::duration<double>(
              now - current.sampledAt).count()
          : 0.0;
      // Assim como no Windows, o alvo nasce sempre do relógio do REAPER.
      // A worker pode atrasar um frame, mas nunca acumula um segundo relógio.
      const double targetTime = std::max(
        0.0,
        current.sourceTime +
          elapsed * current.playbackRate);

      bool decoded = false;
      const bool forceRestart =
        current.discontinuity &&
        current.serial != lastHandledSerial;
      @autoreleasepool {
        decoded =
          open(
            current.path,
            current.requestedWidth,
            current.requestedHeight) &&
          decode(
            targetTime,
            current.playing,
            forceRestart);
      }
      lastHandledSerial = current.serial;
      {
        std::lock_guard<std::mutex> lock(stateMutex);
        handledRequestSerial =
          std::max(
            handledRequestSerial, current.serial);
      }
      if (decoded &&
          (lastPublishedPath != path ||
           lastPublishedPlaybackKey != current.playbackKey ||
           lastPublishedWidth != width ||
           lastPublishedHeight != height ||
           std::abs(
             lastPublishedTimestamp - frameTimestamp) >
             0.000001)) {
        if (publishCurrentFrame(current)) {
          lastPublishedPath = path;
          lastPublishedPlaybackKey =
            current.playbackKey;
          lastPublishedWidth = width;
          lastPublishedHeight = height;
          lastPublishedTimestamp = frameTimestamp;
        }
      }

      {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (stopRequested) break;
      }
      const bool atSourceEnd =
        assetDurationSeconds > 0.0 &&
        targetTime >=
          assetDurationSeconds -
            frameDuration * 0.5;
      if (current.playing &&
          (sequentialReachedEnd || atSourceEnd)) {
        waitForWork(
          current, std::chrono::milliseconds(80));
      } else if (current.playing &&
          sequentialFrameTimestamp >= 0.0) {
        const auto afterDecode =
          std::chrono::steady_clock::now();
        const double liveTarget = std::max(
          0.0,
          current.sourceTime +
            std::chrono::duration<double>(
              afterDecode - current.sampledAt).count() *
            current.playbackRate);
        const double secondsUntilNext =
          (sequentialFrameTimestamp +
             frameDuration * 0.75 -
             liveTarget) /
          std::max(0.1, current.playbackRate);
        const int waitMs = std::max(
          1,
          std::min(
            20,
            static_cast<int>(std::lround(
              secondsUntilNext * 1000.0))));
        waitForWork(
          current, std::chrono::milliseconds(waitMs));
      } else {
        waitForWork(
          current,
          std::chrono::milliseconds(
            current.playing ? 12 : 40));
      }
    }

    @autoreleasepool {
      clearDecoder();
    }
  }

  void submit(
    const std::string& utf8Path,
    const std::string& playbackKey,
    double sourceTime,
    bool playing,
    double playbackRate,
    int requestedWidth,
    int requestedHeight,
    std::chrono::steady_clock::time_point sourceSampledAt)
  {
    const double safeTime = std::max(0.0, sourceTime);
    const double safeRate =
      std::max(0.1, std::min(4.0, playbackRate));
    const int safeWidth = std::max(1, requestedWidth);
    const int safeHeight = std::max(1, requestedHeight);
    const auto now =
      sourceSampledAt !=
        std::chrono::steady_clock::time_point{}
        ? sourceSampledAt
        : std::chrono::steady_clock::now();
    {
      std::lock_guard<std::mutex> lock(stateMutex);

      const bool hadActiveRequest =
        hasRequest && !request.path.empty();
      const bool identityChanged =
        !hadActiveRequest ||
        request.path != utf8Path ||
        request.playbackKey != playbackKey;
      const bool playbackStateChanged =
        !hadActiveRequest ||
        request.playing != playing ||
        std::abs(request.playbackRate - safeRate) > 0.001;
      const bool outputSizeChanged =
        !hadActiveRequest ||
        request.requestedWidth != safeWidth ||
        request.requestedHeight != safeHeight;

      bool explicitTransportJump = false;
      if (hasObservedClock &&
          lastObservedPath == utf8Path &&
          lastObservedPlaybackKey == playbackKey &&
          lastObservedPlaying && playing &&
          std::abs(lastObservedPlaybackRate - safeRate) <= 0.001) {
        const double expectedExternalTime =
          lastObservedSourceTime +
          std::chrono::duration<double>(
            now - lastObservedAt).count() * safeRate;
        const bool clockRegressed =
          safeTime <
            lastObservedSourceTime - 0.010;
        explicitTransportJump =
          clockRegressed ||
          std::abs(safeTime - expectedExternalTime) > 0.30;
      }

      double localTime = request.sourceTime;
      if (hadActiveRequest && request.playing) {
        localTime +=
          std::chrono::duration<double>(
            now - request.sampledAt).count() *
          request.playbackRate;
      }
      const double localDrift = safeTime - localTime;
      const bool correctionWindowOpen =
        lastClockCorrectionAt ==
          std::chrono::steady_clock::time_point::min() ||
        std::chrono::duration<double>(
          now - lastClockCorrectionAt).count() >= 1.8;
      const bool driftNeedsCorrection =
        hadActiveRequest && playing &&
        std::abs(localDrift) > 2.0 &&
        correctionWindowOpen;
      const bool stoppedPositionChanged =
        hadActiveRequest && !playing &&
        std::abs(request.sourceTime - safeTime) > 0.000001;
      const bool synchronizeClock =
        identityChanged ||
        playbackStateChanged ||
        explicitTransportJump ||
        driftNeedsCorrection ||
        stoppedPositionChanged;

      lastObservedPath = utf8Path;
      lastObservedPlaybackKey = playbackKey;
      lastObservedSourceTime = safeTime;
      lastObservedPlaybackRate = safeRate;
      lastObservedPlaying = playing;
      lastObservedAt = now;
      hasObservedClock = !utf8Path.empty();

      const bool changed =
        synchronizeClock ||
        outputSizeChanged ||
        !hasRequest;
      if (!changed) return;
      const bool pendingDiscontinuity =
        request.discontinuity &&
        request.serial > handledRequestSerial;
      request.path = utf8Path;
      request.playbackKey = playbackKey;
      request.discontinuity =
        pendingDiscontinuity ||
        identityChanged ||
        playbackStateChanged ||
        explicitTransportJump ||
        driftNeedsCorrection ||
        stoppedPositionChanged;
      if (synchronizeClock) {
        request.sourceTime = safeTime;
        request.sampledAt = now;
        lastClockCorrectionAt = now;
      }
      request.playing = playing;
      request.playbackRate = safeRate;
      request.requestedWidth = safeWidth;
      request.requestedHeight = safeHeight;
      ++request.serial;
      hasRequest = true;
    }
    stateChanged.notify_one();
  }

  bool currentFrame(
    const std::string& requestedPath,
    const std::string& requestedPlaybackKey,
    DecodedFrame& output)
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!publishedPixels ||
        publishedPixels->empty() ||
        publishedPath != requestedPath ||
        publishedPlaybackKey != requestedPlaybackKey ||
        publishedWidth <= 0 ||
        publishedHeight <= 0 ||
        publishedStride < publishedWidth * 4) {
      return false;
    }
    output.storage = publishedPixels;
    output.pixels = output.storage->data();
    output.width = publishedWidth;
    output.height = publishedHeight;
    output.stride = publishedStride;
    output.timestamp = publishedTimestamp;
    output.sequence = publishedSequence;
    return true;
  }

  void requestReset()
  {
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      const std::uint64_t nextSerial = request.serial + 1;
      request = {};
      request.serial = nextSerial;
      request.path.clear();
      hasRequest = true;
      publishedPixels.reset();
      publishedPath.clear();
      publishedPlaybackKey.clear();
      publishedWidth = 0;
      publishedHeight = 0;
      publishedStride = 0;
      publishedTimestamp = -1.0;
      hasObservedClock = false;
      lastObservedPath.clear();
      lastObservedPlaybackKey.clear();
      lastObservedSourceTime = 0.0;
      lastObservedPlaybackRate = 1.0;
      lastObservedPlaying = false;
      lastObservedAt = std::chrono::steady_clock::now();
      lastClockCorrectionAt =
        std::chrono::steady_clock::time_point::min();
    }
    stateChanged.notify_one();
  }
};

Decoder::Decoder(FrameReadyCallback frameReady)
  : impl_(std::make_unique<Impl>(std::move(frameReady)))
{
}

Decoder::~Decoder() = default;

bool Decoder::frameAt(
  const std::string& utf8Path,
  const std::string& playbackKey,
  double sourceTime,
  bool playing,
  double playbackRate,
  int requestedWidth,
  int requestedHeight,
  DecodedFrame& output)
{
  output = {};
  if (!impl_) return false;
  impl_->submit(
    utf8Path,
    playbackKey,
    sourceTime,
    playing,
    playbackRate,
    requestedWidth,
    requestedHeight,
    std::chrono::steady_clock::now());
  return impl_->currentFrame(
    utf8Path, playbackKey, output);
}

bool Decoder::frameAt(
  const std::string& utf8Path,
  const std::string& playbackKey,
  double sourceTime,
  bool playing,
  double playbackRate,
  int requestedWidth,
  int requestedHeight,
  std::chrono::steady_clock::time_point sourceSampledAt,
  DecodedFrame& output)
{
  output = {};
  if (!impl_) return false;
  impl_->submit(
    utf8Path,
    playbackKey,
    sourceTime,
    playing,
    playbackRate,
    requestedWidth,
    requestedHeight,
    sourceSampledAt);
  return impl_->currentFrame(
    utf8Path, playbackKey, output);
}

void Decoder::reset()
{
  if (impl_) impl_->requestReset();
}

int Decoder::status() const
{
  return impl_ ? impl_->statusCode.load() : -299;
}

} // namespace vshook_video

#endif

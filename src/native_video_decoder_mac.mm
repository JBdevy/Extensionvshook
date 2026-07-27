#include "native_video_decoder.h"

#ifdef __APPLE__

#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
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
  std::shared_ptr<const std::vector<std::uint8_t>> publishedPixels;
  std::string publishedPath;
  std::string publishedPlaybackKey;
  int publishedWidth = 0;
  int publishedHeight = 0;
  int publishedStride = 0;
  double publishedTimestamp = -1.0;
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
  std::string path;
  std::vector<std::uint8_t> pixels;
  int width = 0;
  int height = 0;
  int maximumWidth = 0;
  int maximumHeight = 0;
  double frameDuration = 1.0 / 30.0;
  double frameTimestamp = -1.0;
  std::atomic<int> statusCode{0};

  Impl()
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

  void clearDecoder()
  {
    [generator cancelAllCGImageGeneration];
    [generator release];
    generator = nil;
    [asset release];
    asset = nil;
    path.clear();
    pixels.clear();
    width = 0;
    height = 0;
    maximumWidth = 0;
    maximumHeight = 0;
    frameDuration = 1.0 / 30.0;
    frameTimestamp = -1.0;
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

  bool open(
    const std::string& utf8Path,
    int requestedWidth,
    int requestedHeight)
  {
    const int wantedWidth = std::max(2, requestedWidth);
    const int wantedHeight = std::max(2, requestedHeight);
    constexpr double maximumDecodedPixels =
      960.0 * 540.0;
    const double requestedPixels =
      static_cast<double>(wantedWidth) *
      static_cast<double>(wantedHeight);
    const double scale = requestedPixels > maximumDecodedPixels
      ? std::sqrt(maximumDecodedPixels / requestedPixels)
      : 1.0;
    const int decodeWidth = std::max(
      2, static_cast<int>(std::lround(wantedWidth * scale)));
    const int decodeHeight = std::max(
      2, static_cast<int>(std::lround(wantedHeight * scale)));

    if (generator &&
        path == utf8Path &&
        maximumWidth == decodeWidth &&
        maximumHeight == decodeHeight) {
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
    const float nominalFrameRate =
      videoTrack ? [videoTrack nominalFrameRate] : 0.0f;
    if (nominalFrameRate > 0.1f) {
      frameDuration = std::max(
        1.0 / 240.0,
        std::min(1.0,
          1.0 / static_cast<double>(nominalFrameRate)));
    }

    generator =
      [[AVAssetImageGenerator alloc] initWithAsset:asset];
    generator.appliesPreferredTrackTransform = YES;
    generator.maximumSize =
      CGSizeMake(decodeWidth, decodeHeight);
    // Aceita o quadro real mais próximo, mas nunca um salto grande. Isso
    // evita buscar novamente o mesmo GOP por diferenças sub-frame do relógio.
    const CMTime tolerance = CMTimeMakeWithSeconds(
      std::min(1.0 / 120.0, frameDuration * 0.45),
      60000);
    generator.requestedTimeToleranceBefore = tolerance;
    generator.requestedTimeToleranceAfter = tolerance;
    path = utf8Path;
    maximumWidth = decodeWidth;
    maximumHeight = decodeHeight;
    statusCode = 1;
    return true;
  }

  bool decode(double sourceTime)
  {
    if (!generator) return false;
    if (!pixels.empty() &&
        std::abs(sourceTime - frameTimestamp) <
          std::max(1.0 / 240.0, frameDuration * 0.65)) {
      return true;
    }

    const CMTime requested =
      CMTimeMakeWithSeconds(
        std::max(0.0, sourceTime), 60000);
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
    pixels.assign(
      stride * static_cast<std::size_t>(height), 0);
    CGColorSpaceRef colorSpace =
      CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
      pixels.data(), static_cast<std::size_t>(width),
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

    CGContextTranslateCTM(
      context, 0.0, static_cast<CGFloat>(height));
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextDrawImage(
      context,
      CGRectMake(0.0, 0.0,
        static_cast<CGFloat>(width),
        static_cast<CGFloat>(height)),
      image);
    CGContextRelease(context);
    CGImageRelease(image);

    frameTimestamp =
      CMTIME_IS_NUMERIC(actual)
        ? CMTimeGetSeconds(actual)
        : sourceTime;
    statusCode = 2;
    return true;
  }

  void publishCurrentFrame(
    const PlaybackRequest& processed)
  {
    if (pixels.empty() ||
        width <= 0 || height <= 0) {
      return;
    }
    auto snapshot =
      std::make_shared<std::vector<std::uint8_t>>(pixels);
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!hasRequest ||
        request.path != path ||
        request.playbackKey != processed.playbackKey) {
      return;
    }
    publishedPixels = std::move(snapshot);
    publishedPath = path;
    publishedPlaybackKey = processed.playbackKey;
    publishedWidth = width;
    publishedHeight = height;
    publishedStride = width * 4;
    publishedTimestamp = frameTimestamp;
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
        {
          std::lock_guard<std::mutex> lock(stateMutex);
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
      @autoreleasepool {
        decoded =
          open(
            current.path,
            current.requestedWidth,
            current.requestedHeight) &&
          decode(targetTime);
      }
      if (decoded &&
          (lastPublishedPath != path ||
           lastPublishedPlaybackKey != current.playbackKey ||
           lastPublishedWidth != width ||
           lastPublishedHeight != height ||
           std::abs(
             lastPublishedTimestamp - frameTimestamp) >
             0.000001)) {
        publishCurrentFrame(current);
        lastPublishedPath = path;
        lastPublishedPlaybackKey = current.playbackKey;
        lastPublishedWidth = width;
        lastPublishedHeight = height;
        lastPublishedTimestamp = frameTimestamp;
      }

      {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (stopRequested) break;
      }
      waitForWork(
        current,
        std::chrono::milliseconds(
          current.playing ? 8 : 40));
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
    int requestedHeight)
  {
    const double safeTime = std::max(0.0, sourceTime);
    const double safeRate =
      std::max(0.1, std::min(4.0, playbackRate));
    const int safeWidth = std::max(1, requestedWidth);
    const int safeHeight = std::max(1, requestedHeight);
    const auto now = std::chrono::steady_clock::now();
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
        explicitTransportJump =
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
        std::abs(request.sourceTime - safeTime) > 0.008;
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
      request.path = utf8Path;
      request.playbackKey = playbackKey;
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

Decoder::Decoder()
  : impl_(std::make_unique<Impl>())
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
    requestedHeight);
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

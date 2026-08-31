#include "native_video_decoder.h"

#if defined(_WIN32) || defined(__APPLE__)

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vshook_video {
namespace {

std::string parentPath(const std::string& value)
{
  const std::size_t slash = value.find_last_of("/\\");
  return slash == std::string::npos
    ? std::string() : value.substr(0, slash);
}

std::string joinPath(
  const std::string& left,
  const std::string& right)
{
  if (left.empty()) return right;
  if (right.empty()) return left;
#ifdef _WIN32
  constexpr char separator = '\\';
#else
  constexpr char separator = '/';
#endif
  const char last = left.back();
  if (last == '/' || last == '\\') return left + right;
  return left + separator + right;
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value)
{
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(
    CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (size <= 1) return {};
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(
    CP_UTF8, 0, value.c_str(), -1, result.data(), size);
  if (!result.empty() && result.back() == L'\0') result.pop_back();
  return result;
}

std::string wideToUtf8(const std::wstring& value)
{
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(
    CP_UTF8, 0, value.c_str(), -1, nullptr, 0,
    nullptr, nullptr);
  if (size <= 1) return {};
  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(
    CP_UTF8, 0, value.c_str(), -1, result.data(), size,
    nullptr, nullptr);
  if (!result.empty() && result.back() == '\0') result.pop_back();
  return result;
}

std::string moduleDirectory()
{
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory), &module) ||
      !module) {
    return {};
  }
  std::wstring buffer(32768, L'\0');
  const DWORD length = GetModuleFileNameW(
    module, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) return {};
  buffer.resize(length);
  return parentPath(wideToUtf8(buffer));
}

bool fileExists(const std::string& path)
{
  const std::wstring wide = utf8ToWide(path);
  if (wide.empty()) return false;
  const DWORD attributes = GetFileAttributesW(wide.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
    (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
#else
std::string moduleDirectory()
{
  static const int moduleAnchor = 0;
  Dl_info information{};
  if (dladdr(&moduleAnchor, &information) == 0 ||
      !information.dli_fname) {
    return {};
  }
  return parentPath(information.dli_fname);
}

bool fileExists(const std::string& path)
{
  return !path.empty() && access(path.c_str(), F_OK) == 0;
}
#endif

std::string environmentValue(const char* name)
{
  if (!name || !*name) return {};
#ifdef _WIN32
  const std::wstring wideName = utf8ToWide(name);
  const DWORD size = GetEnvironmentVariableW(
    wideName.c_str(), nullptr, 0);
  if (size <= 1) return {};
  std::wstring value(static_cast<std::size_t>(size), L'\0');
  const DWORD copied = GetEnvironmentVariableW(
    wideName.c_str(), value.data(), size);
  if (copied == 0 || copied >= size) return {};
  value.resize(copied);
  return wideToUtf8(value);
#else
  const char* value = std::getenv(name);
  return value ? value : "";
#endif
}

struct FfmpegLocation {
  std::string root;
  std::string avutil;
  std::string swresample;
  std::string avcodec;
  std::string avformat;
  std::string swscale;
};

FfmpegLocation locationAtRoot(const std::string& candidate)
{
  if (candidate.empty()) return {};
  std::array<std::string, 2> roots = {
    candidate, joinPath(candidate, "lib")
  };
  for (const std::string& root : roots) {
#ifdef _WIN32
    const FfmpegLocation location{
      root,
      joinPath(root, "avutil-60.dll"),
      joinPath(root, "swresample-6.dll"),
      joinPath(root, "avcodec-62.dll"),
      joinPath(root, "avformat-62.dll"),
      joinPath(root, "swscale-9.dll")
    };
#else
    const FfmpegLocation location{
      root,
      joinPath(root, "libavutil.60.dylib"),
      joinPath(root, "libswresample.6.dylib"),
      joinPath(root, "libavcodec.62.dylib"),
      joinPath(root, "libavformat.62.dylib"),
      joinPath(root, "libswscale.9.dylib")
    };
#endif
    if (fileExists(location.avutil) &&
        fileExists(location.avcodec) &&
        fileExists(location.avformat) &&
        fileExists(location.swscale)) {
      return location;
    }
  }
  return {};
}

FfmpegLocation locateFfmpeg()
{
  std::vector<std::string> roots;
  const std::string explicitRoot =
    environmentValue("VSHOOK_FFMPEG_RUNTIME");
  if (!explicitRoot.empty()) roots.push_back(explicitRoot);

  const std::string extensionDirectory = moduleDirectory();
  if (!extensionDirectory.empty()) {
    roots.push_back(joinPath(
      extensionDirectory, "VSHookRuntime/FFmpeg"));
#ifdef _WIN32
    roots.push_back(joinPath(
      extensionDirectory, "VSHookRuntime/FFmpeg/win-x64"));
#else
    roots.push_back(joinPath(
      extensionDirectory, "VSHookRuntime/FFmpeg/macos-universal"));
#endif
  }

#ifdef _WIN32
  const std::string appData = environmentValue("APPDATA");
  if (!appData.empty()) {
    roots.push_back(joinPath(
      appData, "REAPER/UserPlugins/VSHookRuntime/FFmpeg"));
  }
#else
  roots.push_back(
    "/Library/Application Support/REAPER/UserPlugins/"
    "VSHookRuntime/FFmpeg");
  const std::string userHome = environmentValue("HOME");
  if (!userHome.empty()) {
    roots.push_back(joinPath(
      userHome,
      "Library/Application Support/REAPER/UserPlugins/"
      "VSHookRuntime/FFmpeg"));
  }
#endif

  for (const std::string& root : roots) {
    FfmpegLocation location = locationAtRoot(root);
    if (!location.avcodec.empty()) return location;
  }
  return {};
}

struct FfmpegApi {
#ifdef _WIN32
  using Library = HMODULE;
#else
  using Library = void*;
#endif
  std::array<Library, 5> libraries{};

  int (*formatOpenInput)(
    AVFormatContext**, const char*, const AVInputFormat*,
    AVDictionary**) = nullptr;
  int (*formatFindStreamInfo)(AVFormatContext*, AVDictionary**) = nullptr;
  int (*findBestStream)(
    AVFormatContext*, AVMediaType, int, int,
    const AVCodec**, int) = nullptr;
  int (*readFrame)(AVFormatContext*, AVPacket*) = nullptr;
  int (*formatSeekFile)(
    AVFormatContext*, int, std::int64_t, std::int64_t,
    std::int64_t, int) = nullptr;
  void (*formatCloseInput)(AVFormatContext**) = nullptr;

  const AVCodec* (*codecFindDecoder)(AVCodecID) = nullptr;
  const AVCodecHWConfig* (*codecGetHwConfig)(
    const AVCodec*, int) = nullptr;
  AVCodecContext* (*codecAllocContext)(const AVCodec*) = nullptr;
  int (*codecParametersToContext)(
    AVCodecContext*, const AVCodecParameters*) = nullptr;
  int (*codecOpen)(AVCodecContext*, const AVCodec*, AVDictionary**) = nullptr;
  int (*codecSendPacket)(AVCodecContext*, const AVPacket*) = nullptr;
  int (*codecReceiveFrame)(AVCodecContext*, AVFrame*) = nullptr;
  void (*codecFlushBuffers)(AVCodecContext*) = nullptr;
  void (*codecFreeContext)(AVCodecContext**) = nullptr;

  AVPacket* (*packetAlloc)() = nullptr;
  void (*packetUnref)(AVPacket*) = nullptr;
  void (*packetFree)(AVPacket**) = nullptr;
  AVFrame* (*frameAlloc)() = nullptr;
  int (*frameRef)(AVFrame*, const AVFrame*) = nullptr;
  void (*frameMoveRef)(AVFrame*, AVFrame*) = nullptr;
  void (*frameUnref)(AVFrame*) = nullptr;
  void (*frameFree)(AVFrame**) = nullptr;
  AVBufferRef* (*bufferRef)(const AVBufferRef*) = nullptr;
  void (*bufferUnref)(AVBufferRef**) = nullptr;
  AVHWDeviceType (*hwDeviceFindTypeByName)(const char*) = nullptr;
  int (*hwDeviceCreate)(
    AVBufferRef**, AVHWDeviceType, const char*, AVDictionary*, int) = nullptr;
  int (*hwFrameTransferData)(AVFrame*, const AVFrame*, int) = nullptr;

  SwsContext* (*swsGetCachedContext)(
    SwsContext*, int, int, AVPixelFormat,
    int, int, AVPixelFormat, int,
    SwsFilter*, SwsFilter*, const double*) = nullptr;
  int (*swsScale)(
    SwsContext*, const std::uint8_t* const[], const int[],
    int, int, std::uint8_t* const[], const int[]) = nullptr;
  void (*swsFreeContext)(SwsContext*) = nullptr;

  ~FfmpegApi() { unload(); }

  void unload()
  {
    for (auto iterator = libraries.rbegin();
         iterator != libraries.rend(); ++iterator) {
      if (!*iterator) continue;
#ifdef _WIN32
      FreeLibrary(*iterator);
#else
      dlclose(*iterator);
#endif
      *iterator = nullptr;
    }
  }

  void* symbol(const char* name) const
  {
    if (!name) return nullptr;
    for (auto iterator = libraries.rbegin();
         iterator != libraries.rend(); ++iterator) {
      if (!*iterator) continue;
#ifdef _WIN32
      void* result = reinterpret_cast<void*>(
        GetProcAddress(*iterator, name));
#else
      void* result = dlsym(*iterator, name);
#endif
      if (result) return result;
    }
    return nullptr;
  }

  template<typename T>
  bool bind(T& target, const char* name)
  {
    target = reinterpret_cast<T>(symbol(name));
    return target != nullptr;
  }

  bool loadLibrary(std::size_t index, const std::string& path)
  {
    if (path.empty()) return true;
#ifdef _WIN32
    const std::wstring wide = utf8ToWide(path);
    if (wide.empty()) return false;
    libraries[index] = LoadLibraryExW(
      wide.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    libraries[index] = dlopen(
      path.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
    return libraries[index] != nullptr;
  }

  bool load()
  {
    const FfmpegLocation location = locateFfmpeg();
    if (location.avcodec.empty()) return false;
    if (!loadLibrary(0, location.avutil) ||
        (!location.swresample.empty() && fileExists(location.swresample) &&
         !loadLibrary(1, location.swresample)) ||
        !loadLibrary(2, location.avcodec) ||
        !loadLibrary(3, location.avformat) ||
        !loadLibrary(4, location.swscale)) {
      unload();
      return false;
    }

    const bool complete =
      bind(formatOpenInput, "avformat_open_input") &&
      bind(formatFindStreamInfo, "avformat_find_stream_info") &&
      bind(findBestStream, "av_find_best_stream") &&
      bind(readFrame, "av_read_frame") &&
      bind(formatSeekFile, "avformat_seek_file") &&
      bind(formatCloseInput, "avformat_close_input") &&
      bind(codecFindDecoder, "avcodec_find_decoder") &&
      bind(codecGetHwConfig, "avcodec_get_hw_config") &&
      bind(codecAllocContext, "avcodec_alloc_context3") &&
      bind(codecParametersToContext, "avcodec_parameters_to_context") &&
      bind(codecOpen, "avcodec_open2") &&
      bind(codecSendPacket, "avcodec_send_packet") &&
      bind(codecReceiveFrame, "avcodec_receive_frame") &&
      bind(codecFlushBuffers, "avcodec_flush_buffers") &&
      bind(codecFreeContext, "avcodec_free_context") &&
      bind(packetAlloc, "av_packet_alloc") &&
      bind(packetUnref, "av_packet_unref") &&
      bind(packetFree, "av_packet_free") &&
      bind(frameAlloc, "av_frame_alloc") &&
      bind(frameRef, "av_frame_ref") &&
      bind(frameMoveRef, "av_frame_move_ref") &&
      bind(frameUnref, "av_frame_unref") &&
      bind(frameFree, "av_frame_free") &&
      bind(bufferRef, "av_buffer_ref") &&
      bind(bufferUnref, "av_buffer_unref") &&
      bind(hwDeviceFindTypeByName, "av_hwdevice_find_type_by_name") &&
      bind(hwDeviceCreate, "av_hwdevice_ctx_create") &&
      bind(hwFrameTransferData, "av_hwframe_transfer_data") &&
      bind(swsGetCachedContext, "sws_getCachedContext") &&
      bind(swsScale, "sws_scale") &&
      bind(swsFreeContext, "sws_freeContext");
    if (!complete) {
      unload();
      return false;
    }
    return true;
  }
};

double rationalToDouble(AVRational value)
{
  return value.den != 0
    ? static_cast<double>(value.num) /
        static_cast<double>(value.den)
    : 0.0;
}

std::string nativeMediaPath(const std::string& path)
{
#ifdef _WIN32
  std::string native = path;
  std::replace(native.begin(), native.end(), '/', '\\');
  return native;
#else
  return path;
#endif
}

} // namespace

struct Decoder::Impl {
  struct Request {
    std::string path;
    std::string playbackKey;
    double sourceTime = 0.0;
    double playbackRate = 1.0;
    int requestedWidth = 2;
    int requestedHeight = 2;
    bool playing = false;
    std::uint64_t serial = 0;
  };

  FfmpegApi api;
  std::mutex requestMutex;
  std::condition_variable requestChanged;
  std::thread worker;
  Request request;
  bool hasRequest = false;
  bool stopRequested = false;
  std::atomic<int> statusCode{0};

  std::mutex frameMutex;
  std::array<std::shared_ptr<std::vector<std::uint8_t>>, 4> framePool;
  std::size_t nextFramePoolIndex = 0;
  std::shared_ptr<const std::vector<std::uint8_t>> publishedPixels;
  std::string publishedPath;
  std::string publishedPlaybackKey;
  int publishedWidth = 0;
  int publishedHeight = 0;
  int publishedStride = 0;
  double publishedTimestamp = -1.0;
  std::uint64_t publishedSequence = 0;

  AVFormatContext* format = nullptr;
  AVCodecContext* codec = nullptr;
  AVPacket* packet = nullptr;
  AVFrame* decodedFrame = nullptr;
  AVFrame* displayFrame = nullptr;
  AVFrame* lookaheadFrame = nullptr;
  AVFrame* softwareFrame = nullptr;
  AVBufferRef* hardwareDevice = nullptr;
  AVPixelFormat hardwarePixelFormat = AV_PIX_FMT_NONE;
  SwsContext* scaler = nullptr;
  int videoStreamIndex = -1;
  AVStream* videoStream = nullptr;
  double timeBase = 0.0;
  double streamStart = 0.0;
  double nominalFrameDuration = 1.0 / 30.0;
  double displayTimestamp = -1.0;
  double lookaheadTimestamp = -1.0;
  double lastDecodedTimestamp = -1.0;
  bool draining = false;
  bool reachedEnd = false;
  std::string activePath;
  std::string activePlaybackKey;
  bool previousRequestValid = false;
  bool previousPlaying = false;
  double previousSourceTime = 0.0;
  double previousPlaybackRate = 1.0;
  std::chrono::steady_clock::time_point previousRequestAt{};
  int convertedWidth = 0;
  int convertedHeight = 0;
  double convertedTimestamp = -1.0;

  Impl()
  {
    worker = std::thread([this]() { workerLoop(); });
  }

  ~Impl()
  {
    {
      std::lock_guard<std::mutex> lock(requestMutex);
      stopRequested = true;
    }
    requestChanged.notify_all();
    if (worker.joinable()) worker.join();
    closeMedia();
  }

  static AVPixelFormat selectHardwareFormat(
    AVCodecContext* context,
    const AVPixelFormat* formats)
  {
    auto* self = context
      ? static_cast<Impl*>(context->opaque) : nullptr;
    if (!formats) return AV_PIX_FMT_NONE;
    if (self && self->hardwarePixelFormat != AV_PIX_FMT_NONE) {
      for (const AVPixelFormat* item = formats;
           *item != AV_PIX_FMT_NONE; ++item) {
        if (*item == self->hardwarePixelFormat) return *item;
      }
    }
    return formats[0];
  }

  void clearPublished()
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    publishedPixels.reset();
    publishedPath.clear();
    publishedPlaybackKey.clear();
    publishedWidth = 0;
    publishedHeight = 0;
    publishedStride = 0;
    publishedTimestamp = -1.0;
  }

  void closeMedia()
  {
    if (scaler && api.swsFreeContext) api.swsFreeContext(scaler);
    scaler = nullptr;
    if (decodedFrame && api.frameFree) api.frameFree(&decodedFrame);
    if (displayFrame && api.frameFree) api.frameFree(&displayFrame);
    if (lookaheadFrame && api.frameFree) api.frameFree(&lookaheadFrame);
    if (softwareFrame && api.frameFree) api.frameFree(&softwareFrame);
    if (packet && api.packetFree) api.packetFree(&packet);
    if (codec && api.codecFreeContext) api.codecFreeContext(&codec);
    if (hardwareDevice && api.bufferUnref) api.bufferUnref(&hardwareDevice);
    if (format && api.formatCloseInput) api.formatCloseInput(&format);
    videoStreamIndex = -1;
    videoStream = nullptr;
    hardwarePixelFormat = AV_PIX_FMT_NONE;
    timeBase = 0.0;
    streamStart = 0.0;
    nominalFrameDuration = 1.0 / 30.0;
    displayTimestamp = -1.0;
    lookaheadTimestamp = -1.0;
    lastDecodedTimestamp = -1.0;
    draining = false;
    reachedEnd = false;
    activePath.clear();
    activePlaybackKey.clear();
    previousRequestValid = false;
    convertedWidth = 0;
    convertedHeight = 0;
    convertedTimestamp = -1.0;
  }

  bool configureHardware(const AVCodec* decoder)
  {
#ifdef _WIN32
    constexpr const char* hardwareName = "d3d11va";
#else
    constexpr const char* hardwareName = "videotoolbox";
#endif
    const AVHWDeviceType deviceType =
      api.hwDeviceFindTypeByName(hardwareName);
    if (deviceType == AV_HWDEVICE_TYPE_NONE) return false;
    AVPixelFormat selected = AV_PIX_FMT_NONE;
    for (int index = 0;; ++index) {
      const AVCodecHWConfig* configuration =
        api.codecGetHwConfig(decoder, index);
      if (!configuration) break;
      if (configuration->device_type == deviceType &&
          (configuration->methods &
            AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0) {
        selected = configuration->pix_fmt;
        break;
      }
    }
    if (selected == AV_PIX_FMT_NONE) return false;
    if (api.hwDeviceCreate(
          &hardwareDevice, deviceType, nullptr, nullptr, 0) < 0 ||
        !hardwareDevice) {
      if (hardwareDevice) api.bufferUnref(&hardwareDevice);
      return false;
    }
    hardwarePixelFormat = selected;
    codec->opaque = this;
    codec->get_format = &Impl::selectHardwareFormat;
    codec->hw_device_ctx = api.bufferRef(hardwareDevice);
    if (!codec->hw_device_ctx) {
      api.bufferUnref(&hardwareDevice);
      hardwarePixelFormat = AV_PIX_FMT_NONE;
      codec->opaque = nullptr;
      codec->get_format = nullptr;
      return false;
    }
    return true;
  }

  bool openMedia(const Request& current)
  {
    closeMedia();
    clearPublished();
    statusCode.store(1, std::memory_order_release);
    const std::string path = nativeMediaPath(current.path);
    if (api.formatOpenInput(
          &format, path.c_str(), nullptr, nullptr) < 0 || !format) {
      statusCode.store(-302, std::memory_order_release);
      closeMedia();
      return false;
    }
    if (api.formatFindStreamInfo(format, nullptr) < 0) {
      statusCode.store(-303, std::memory_order_release);
      closeMedia();
      return false;
    }
    const AVCodec* suggestedDecoder = nullptr;
    videoStreamIndex = api.findBestStream(
      format, AVMEDIA_TYPE_VIDEO, -1, -1,
      &suggestedDecoder, 0);
    if (videoStreamIndex < 0 ||
        videoStreamIndex >= static_cast<int>(format->nb_streams)) {
      statusCode.store(-303, std::memory_order_release);
      closeMedia();
      return false;
    }
    videoStream = format->streams[videoStreamIndex];
    if (!videoStream || !videoStream->codecpar) {
      statusCode.store(-303, std::memory_order_release);
      closeMedia();
      return false;
    }
    const AVCodec* decoder = suggestedDecoder
      ? suggestedDecoder
      : api.codecFindDecoder(videoStream->codecpar->codec_id);
    if (!decoder) {
      statusCode.store(-304, std::memory_order_release);
      closeMedia();
      return false;
    }
    codec = api.codecAllocContext(decoder);
    if (!codec ||
        api.codecParametersToContext(codec, videoStream->codecpar) < 0) {
      statusCode.store(-304, std::memory_order_release);
      closeMedia();
      return false;
    }
    configureHardware(decoder);
    if (api.codecOpen(codec, decoder, nullptr) < 0) {
      // Se o dispositivo recusou este perfil, reabre o mesmo decoder FFmpeg
      // em software. Nao ha troca para outro player nem outro relogio.
      api.codecFreeContext(&codec);
      if (hardwareDevice) api.bufferUnref(&hardwareDevice);
      hardwarePixelFormat = AV_PIX_FMT_NONE;
      codec = api.codecAllocContext(decoder);
      if (!codec ||
          api.codecParametersToContext(codec, videoStream->codecpar) < 0 ||
          api.codecOpen(codec, decoder, nullptr) < 0) {
        statusCode.store(-304, std::memory_order_release);
        closeMedia();
        return false;
      }
    }

    packet = api.packetAlloc();
    decodedFrame = api.frameAlloc();
    displayFrame = api.frameAlloc();
    lookaheadFrame = api.frameAlloc();
    softwareFrame = api.frameAlloc();
    if (!packet || !decodedFrame || !displayFrame ||
        !lookaheadFrame || !softwareFrame) {
      statusCode.store(-305, std::memory_order_release);
      closeMedia();
      return false;
    }
    timeBase = rationalToDouble(videoStream->time_base);
    if (!(timeBase > 0.0)) {
      statusCode.store(-303, std::memory_order_release);
      closeMedia();
      return false;
    }
    streamStart = videoStream->start_time != AV_NOPTS_VALUE
      ? static_cast<double>(videoStream->start_time) * timeBase
      : 0.0;
    const double averageRate = rationalToDouble(
      videoStream->avg_frame_rate);
    if (averageRate > 0.5 && averageRate < 1000.0) {
      nominalFrameDuration = 1.0 / averageRate;
    }
    activePath = current.path;
    activePlaybackKey = current.playbackKey;
    statusCode.store(1, std::memory_order_release);
    return true;
  }

  double timestampOf(const AVFrame* frame) const
  {
    if (!frame) return -1.0;
    std::int64_t timestamp = frame->best_effort_timestamp;
    if (timestamp == AV_NOPTS_VALUE) timestamp = frame->pts;
    if (timestamp == AV_NOPTS_VALUE) {
      return lastDecodedTimestamp >= 0.0
        ? lastDecodedTimestamp + nominalFrameDuration : 0.0;
    }
    return std::max(
      0.0, static_cast<double>(timestamp) * timeBase - streamStart);
  }

  bool seekTo(double sourceTime)
  {
    if (!format || !codec || !videoStream || timeBase <= 0.0) {
      return false;
    }
    const double absoluteSeconds =
      std::max(0.0, sourceTime) + streamStart;
    const double rawTimestamp = absoluteSeconds / timeBase;
    const std::int64_t timestamp = rawTimestamp >=
        static_cast<double>(std::numeric_limits<std::int64_t>::max())
      ? std::numeric_limits<std::int64_t>::max()
      : static_cast<std::int64_t>(std::llround(rawTimestamp));
    if (api.formatSeekFile(
          format, videoStreamIndex,
          std::numeric_limits<std::int64_t>::min(),
          timestamp,
          timestamp,
          AVSEEK_FLAG_BACKWARD) < 0) {
      return false;
    }
    api.codecFlushBuffers(codec);
    api.frameUnref(decodedFrame);
    api.frameUnref(displayFrame);
    api.frameUnref(lookaheadFrame);
    displayTimestamp = -1.0;
    lookaheadTimestamp = -1.0;
    lastDecodedTimestamp = -1.0;
    convertedTimestamp = -1.0;
    draining = false;
    reachedEnd = false;
    return true;
  }

  bool decodeNextFrame()
  {
    if (!format || !codec || !packet || !decodedFrame) return false;
    for (;;) {
      api.frameUnref(decodedFrame);
      const int received = api.codecReceiveFrame(codec, decodedFrame);
      if (received == 0) {
        lastDecodedTimestamp = timestampOf(decodedFrame);
        return true;
      }
      if (received == AVERROR_EOF) {
        reachedEnd = true;
        return false;
      }
      if (received != AVERROR(EAGAIN)) {
        reachedEnd = true;
        return false;
      }
      if (draining) {
        reachedEnd = true;
        return false;
      }

      const int readResult = api.readFrame(format, packet);
      if (readResult < 0) {
        api.codecSendPacket(codec, nullptr);
        draining = true;
        continue;
      }
      if (packet->stream_index != videoStreamIndex) {
        api.packetUnref(packet);
        continue;
      }
      const int sent = api.codecSendPacket(codec, packet);
      api.packetUnref(packet);
      if (sent < 0 && sent != AVERROR(EAGAIN)) {
        continue;
      }
    }
  }

  void moveDecodedTo(AVFrame* destination, double& timestamp)
  {
    api.frameUnref(destination);
    timestamp = timestampOf(decodedFrame);
    api.frameMoveRef(destination, decodedFrame);
  }

  bool chooseFrame(double target, bool forceSeek)
  {
    if (!codec || !displayFrame || !lookaheadFrame) return false;
    const double tolerance = std::max(
      0.0005, nominalFrameDuration * 0.08);
    if (forceSeek ||
        (displayTimestamp >= 0.0 &&
         target + tolerance < displayTimestamp) ||
        (lastDecodedTimestamp >= 0.0 &&
         target > lastDecodedTimestamp + 1.0)) {
      if (!seekTo(target)) return false;
    }

    if (lookaheadTimestamp >= 0.0 &&
        lookaheadTimestamp <= target + tolerance) {
      api.frameUnref(displayFrame);
      api.frameMoveRef(displayFrame, lookaheadFrame);
      displayTimestamp = lookaheadTimestamp;
      lookaheadTimestamp = -1.0;
    }

    int decodedCount = 0;
    while (decodedCount < 240) {
      if (displayTimestamp >= 0.0 &&
          lookaheadTimestamp > target + tolerance) {
        break;
      }
      if (!decodeNextFrame()) break;
      ++decodedCount;
      const double timestamp = timestampOf(decodedFrame);
      if (displayTimestamp < 0.0 || timestamp <= target + tolerance) {
        moveDecodedTo(displayFrame, displayTimestamp);
        continue;
      }
      moveDecodedTo(lookaheadFrame, lookaheadTimestamp);
      break;
    }

    // Antes do primeiro PTS, o primeiro quadro e a imagem correta da fonte.
    if (displayTimestamp < 0.0 && lookaheadTimestamp >= 0.0) {
      api.frameMoveRef(displayFrame, lookaheadFrame);
      displayTimestamp = lookaheadTimestamp;
      lookaheadTimestamp = -1.0;
    }
    return displayTimestamp >= 0.0 && displayFrame->width > 0 &&
      displayFrame->height > 0;
  }

  std::shared_ptr<std::vector<std::uint8_t>> acquirePixelBuffer(
    std::size_t byteCount)
  {
    for (std::size_t offset = 0; offset < framePool.size(); ++offset) {
      const std::size_t index =
        (nextFramePoolIndex + offset) % framePool.size();
      auto& candidate = framePool[index];
      if (!candidate || candidate.use_count() == 1) {
        if (!candidate) {
          candidate =
            std::make_shared<std::vector<std::uint8_t>>();
        }
        candidate->resize(byteCount);
        nextFramePoolIndex = (index + 1) % framePool.size();
        return candidate;
      }
    }
    auto replacement =
      std::make_shared<std::vector<std::uint8_t>>(byteCount);
    framePool[nextFramePoolIndex] = replacement;
    nextFramePoolIndex =
      (nextFramePoolIndex + 1) % framePool.size();
    return replacement;
  }

  bool publishDisplayFrame(const Request& current)
  {
    if (!displayFrame || displayTimestamp < 0.0) return false;
    const AVFrame* source = displayFrame;
    if (!source || source->width <= 0 || source->height <= 0) {
      return false;
    }

    double displayAspect = static_cast<double>(source->width) /
      static_cast<double>(source->height);
    const AVRational aspect = source->sample_aspect_ratio;
    if (aspect.num > 0 && aspect.den > 0) {
      displayAspect *= rationalToDouble(aspect);
    }
    int outputWidth = std::max(2, current.requestedWidth);
    int outputHeight = std::max(2, current.requestedHeight);
    const double availableAspect =
      static_cast<double>(outputWidth) /
      static_cast<double>(outputHeight);
    if (availableAspect > displayAspect) {
      outputWidth = std::max(
        2, static_cast<int>(std::lround(
          static_cast<double>(outputHeight) * displayAspect)));
    } else {
      outputHeight = std::max(
        2, static_cast<int>(std::lround(
          static_cast<double>(outputWidth) / displayAspect)));
    }
    // Nao amplia alem da resolucao visivel da fonte. A janela ainda pode
    // apresentar em tela cheia, mas evita gastar CPU criando pixels que o
    // renderer apenas voltaria a copiar.
    if (outputWidth > source->width || outputHeight > source->height) {
      const double scale = std::min(
        static_cast<double>(source->width) /
          static_cast<double>(outputWidth),
        static_cast<double>(source->height) /
          static_cast<double>(outputHeight));
      outputWidth = std::max(
        2, static_cast<int>(std::floor(outputWidth * scale)));
      outputHeight = std::max(
        2, static_cast<int>(std::floor(outputHeight * scale)));
    }
    outputWidth &= ~1;
    outputHeight &= ~1;
    outputWidth = std::max(2, outputWidth);
    outputHeight = std::max(2, outputHeight);

    const bool sameFrame =
      std::abs(convertedTimestamp - displayTimestamp) < 0.000001 &&
      convertedWidth == outputWidth &&
      convertedHeight == outputHeight;
    if (sameFrame) {
      statusCode.store(2, std::memory_order_release);
      return true;
    }

    if (hardwarePixelFormat != AV_PIX_FMT_NONE &&
        displayFrame->format == hardwarePixelFormat) {
      api.frameUnref(softwareFrame);
      if (api.hwFrameTransferData(
            softwareFrame, displayFrame, 0) < 0) {
        statusCode.store(-305, std::memory_order_release);
        return false;
      }
      source = softwareFrame;
    }

    const int stride = outputWidth * 4;
    const std::size_t byteCount =
      static_cast<std::size_t>(stride) *
      static_cast<std::size_t>(outputHeight);
    auto pixels = acquirePixelBuffer(byteCount);
    scaler = api.swsGetCachedContext(
      scaler,
      source->width, source->height,
      static_cast<AVPixelFormat>(source->format),
      outputWidth, outputHeight, AV_PIX_FMT_BGRA,
      SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!scaler) {
      statusCode.store(-305, std::memory_order_release);
      return false;
    }
    std::uint8_t* destinationData[4] = {
      pixels->data(), nullptr, nullptr, nullptr
    };
    const int destinationStride[4] = {stride, 0, 0, 0};
    const int scaled = api.swsScale(
      scaler, source->data, source->linesize,
      0, source->height, destinationData, destinationStride);
    if (scaled != outputHeight) {
      statusCode.store(-305, std::memory_order_release);
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(frameMutex);
      publishedPixels = pixels;
      publishedPath = current.path;
      publishedPlaybackKey = current.playbackKey;
      publishedWidth = outputWidth;
      publishedHeight = outputHeight;
      publishedStride = stride;
      publishedTimestamp = displayTimestamp;
      ++publishedSequence;
    }
    convertedWidth = outputWidth;
    convertedHeight = outputHeight;
    convertedTimestamp = displayTimestamp;
    statusCode.store(2, std::memory_order_release);
    return true;
  }

  bool shouldSeek(const Request& current) const
  {
    if (!previousRequestValid) return current.sourceTime > 0.15;
    if (current.playbackKey != activePlaybackKey) return true;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::max(
      0.0, std::chrono::duration<double>(
        now - previousRequestAt).count());
    if (previousPlaying && current.playing) {
      const double expected = elapsed * std::max(
        0.000001, previousPlaybackRate);
      const double actual =
        current.sourceTime - previousSourceTime;
      return actual < -0.01 || std::abs(actual - expected) > 0.25;
    }
    if (previousPlaying != current.playing) {
      // Stop e Play precisam sempre acordar exatamente no cursor atual.
      return std::abs(current.sourceTime - displayTimestamp) >
        nominalFrameDuration * 0.45;
    }
    const double movement =
      current.sourceTime - previousSourceTime;
    return movement < -0.005 || movement > 0.75;
  }

  void applyRequest(const Request& current)
  {
    if (current.path.empty() || current.playbackKey.empty()) {
      closeMedia();
      clearPublished();
      statusCode.store(0, std::memory_order_release);
      return;
    }
    if (!format || current.path != activePath) {
      if (!openMedia(current)) return;
    }
    const bool identityChanged =
      current.playbackKey != activePlaybackKey;
    const bool forceSeek = identityChanged || shouldSeek(current);
    if (identityChanged) {
      activePlaybackKey = current.playbackKey;
      clearPublished();
    }
    if (forceSeek) {
      statusCode.store(1, std::memory_order_release);
    }
    if (chooseFrame(current.sourceTime, forceSeek)) {
      publishDisplayFrame(current);
    }
    previousRequestValid = true;
    previousPlaying = current.playing;
    previousSourceTime = current.sourceTime;
    previousPlaybackRate = current.playbackRate;
    previousRequestAt = std::chrono::steady_clock::now();
  }

  void workerLoop()
  {
    if (!api.load()) {
      statusCode.store(-300, std::memory_order_release);
      return;
    }
    std::uint64_t handledSerial = 0;
    for (;;) {
      Request current;
      {
        std::unique_lock<std::mutex> lock(requestMutex);
        requestChanged.wait(lock, [&]() {
          return stopRequested ||
            (hasRequest && request.serial != handledSerial);
        });
        if (stopRequested) break;
        current = request;
        handledSerial = current.serial;
      }
      applyRequest(current);
    }
    closeMedia();
  }

  bool frameAt(
    const std::string& path,
    const std::string& playbackKey,
    double sourceTime,
    bool playing,
    double playbackRate,
    int requestedWidth,
    int requestedHeight,
    DecodedFrame& output)
  {
    output = {};
    if (path.empty() || playbackKey.empty()) return false;
    {
      std::lock_guard<std::mutex> lock(requestMutex);
      request.path = path;
      request.playbackKey = playbackKey;
      request.sourceTime = std::max(0.0, sourceTime);
      request.playing = playing;
      request.playbackRate = std::max(0.000001, playbackRate);
      request.requestedWidth = std::max(2, requestedWidth);
      request.requestedHeight = std::max(2, requestedHeight);
      ++request.serial;
      hasRequest = true;
    }
    requestChanged.notify_one();

    std::lock_guard<std::mutex> lock(frameMutex);
    if (!publishedPixels || publishedPath != path ||
        publishedPlaybackKey != playbackKey ||
        publishedWidth <= 0 || publishedHeight <= 0 ||
        publishedStride < publishedWidth * 4) {
      return false;
    }
    output.storage = publishedPixels;
    output.pixels = publishedPixels->data();
    output.width = publishedWidth;
    output.height = publishedHeight;
    output.stride = publishedStride;
    output.timestamp = publishedTimestamp;
    output.sequence = publishedSequence;
    return true;
  }

  void reset()
  {
    {
      std::lock_guard<std::mutex> lock(requestMutex);
      request = Request{};
      request.playbackKey = "__reset__";
      ++request.serial;
      hasRequest = true;
    }
    clearPublished();
    requestChanged.notify_one();
  }
};

Decoder::Decoder()
  : impl_(std::make_unique<Impl>()) {}

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
  return impl_ && impl_->frameAt(
    utf8Path, playbackKey, sourceTime, playing, playbackRate,
    requestedWidth, requestedHeight, output);
}

void Decoder::reset()
{
  if (impl_) impl_->reset();
}

int Decoder::status() const
{
  return impl_
    ? impl_->statusCode.load(std::memory_order_acquire) : -399;
}

} // namespace vshook_video

#endif

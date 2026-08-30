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

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace vshook_video {

namespace {

struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;
using libvlc_time_t = std::int64_t;

using VideoLockCallback = void* (*)(void*, void**);
using VideoUnlockCallback = void (*)(void*, void*, void* const*);
using VideoDisplayCallback = void (*)(void*, void*);
using VideoFormatCallback = unsigned (*)(
  void**, char*, unsigned*, unsigned*, unsigned*, unsigned*);
using VideoCleanupCallback = void (*)(void*);

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
  if (!result.empty() && result.back() == L'\0') {
    result.pop_back();
  }
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
  if (!result.empty() && result.back() == '\0') {
    result.pop_back();
  }
  return result;
}

std::string moduleDirectory()
{
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory),
        &module) || !module) {
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
  if (dladdr(
        &moduleAnchor,
        &information) == 0 ||
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

void setEnvironmentValue(
  const char* name,
  const std::string& value)
{
  if (!name || !*name || value.empty()) return;
#ifdef _WIN32
  const std::wstring wideName = utf8ToWide(name);
  const std::wstring wideValue = utf8ToWide(value);
  if (!wideName.empty() && !wideValue.empty()) {
    SetEnvironmentVariableW(wideName.c_str(), wideValue.c_str());
  }
#else
  setenv(name, value.c_str(), 1);
#endif
}

struct VlcLocation {
  std::string library;
  std::string coreLibrary;
  std::string pluginDirectory;
};

VlcLocation locateVlc()
{
  std::vector<std::string> roots;
  const std::string explicitRoot =
    environmentValue("VSHOOK_VLC_RUNTIME");
  if (!explicitRoot.empty()) roots.push_back(explicitRoot);

  const std::string extensionDirectory = moduleDirectory();
  if (!extensionDirectory.empty()) {
    roots.push_back(joinPath(
      extensionDirectory, "VSHookRuntime/VLC"));
#ifdef _WIN32
    roots.push_back(joinPath(
      extensionDirectory, "VSHookRuntime/VLC/win-x64"));
#else
    roots.push_back(joinPath(
      extensionDirectory, "VSHookRuntime/VLC/macos-universal"));
#endif
  }

#ifdef _WIN32
  const std::string programFiles =
    environmentValue("ProgramFiles");
  if (!programFiles.empty()) {
    roots.push_back(joinPath(programFiles, "VideoLAN/VLC"));
  }
  const std::string localAppData =
    environmentValue("LOCALAPPDATA");
  if (!localAppData.empty()) {
    roots.push_back(joinPath(
      localAppData, "Programs/VideoLAN/VLC"));
  }
  for (const std::string& root : roots) {
    const std::string library = joinPath(root, "libvlc.dll");
    if (!fileExists(library)) continue;
    return {
      library,
      joinPath(root, "libvlccore.dll"),
      joinPath(root, "plugins")
    };
  }
#else
  roots.push_back(
    "/Library/Application Support/REAPER/UserPlugins/"
    "VSHookRuntime/VLC");
  roots.push_back("/Applications/VLC.app/Contents/MacOS");
  roots.push_back(
    "/Applications/VLC.app/Contents/MacOS/lib");
  for (const std::string& root : roots) {
    const std::array<std::string, 2> libraries = {
      joinPath(root, "lib/libvlc.dylib"),
      joinPath(root, "libvlc.dylib")
    };
    for (const std::string& library : libraries) {
      if (!fileExists(library)) continue;
      const std::string libraryDirectory = parentPath(library);
      const std::string runtimeRoot =
        libraryDirectory.size() >= 4 &&
          libraryDirectory.substr(libraryDirectory.size() - 4) == "/lib"
        ? parentPath(libraryDirectory)
        : root;
      return {
        library,
        joinPath(libraryDirectory, "libvlccore.dylib"),
        joinPath(runtimeRoot, "plugins")
      };
    }
  }
#endif
  return {};
}

struct VlcApi {
#ifdef _WIN32
  HMODULE library = nullptr;
  HMODULE coreLibrary = nullptr;
#else
  void* library = nullptr;
  void* coreLibrary = nullptr;
#endif
  std::string pluginDirectory;

  libvlc_instance_t* (*newInstance)(int, const char* const*) = nullptr;
  void (*releaseInstance)(libvlc_instance_t*) = nullptr;
  libvlc_media_t* (*newMediaPath)(libvlc_instance_t*, const char*) = nullptr;
  void (*releaseMedia)(libvlc_media_t*) = nullptr;
  libvlc_media_player_t* (*newPlayerFromMedia)(libvlc_media_t*) = nullptr;
  void (*releasePlayer)(libvlc_media_player_t*) = nullptr;
  int (*play)(libvlc_media_player_t*) = nullptr;
  void (*setPause)(libvlc_media_player_t*, int) = nullptr;
  void (*stop)(libvlc_media_player_t*) = nullptr;
  void (*setTime)(libvlc_media_player_t*, libvlc_time_t) = nullptr;
  void (*nextFrame)(libvlc_media_player_t*) = nullptr;
  libvlc_time_t (*getTime)(libvlc_media_player_t*) = nullptr;
  int (*setRate)(libvlc_media_player_t*, float) = nullptr;
  int (*setMute)(libvlc_media_player_t*, int) = nullptr;
  void (*setVideoCallbacks)(
    libvlc_media_player_t*, VideoLockCallback,
    VideoUnlockCallback, VideoDisplayCallback, void*) = nullptr;
  void (*setVideoFormatCallbacks)(
    libvlc_media_player_t*, VideoFormatCallback,
    VideoCleanupCallback) = nullptr;

  ~VlcApi() { unload(); }

  void unload()
  {
#ifdef _WIN32
    if (library) FreeLibrary(library);
    if (coreLibrary) FreeLibrary(coreLibrary);
#else
    if (library) dlclose(library);
    if (coreLibrary) dlclose(coreLibrary);
#endif
    library = nullptr;
    coreLibrary = nullptr;
  }

  void* symbol(const char* name) const
  {
    if (!library || !name) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(
      GetProcAddress(library, name));
#else
    return dlsym(library, name);
#endif
  }

  template<typename T>
  bool bind(T& target, const char* name)
  {
    target = reinterpret_cast<T>(symbol(name));
    return target != nullptr;
  }

  bool load()
  {
    const VlcLocation location = locateVlc();
    if (location.library.empty()) return false;
    pluginDirectory = location.pluginDirectory;
#ifdef _WIN32
    if (fileExists(location.coreLibrary)) {
      coreLibrary = LoadLibraryExW(
        utf8ToWide(location.coreLibrary).c_str(), nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    library = LoadLibraryExW(
      utf8ToWide(location.library).c_str(), nullptr,
      LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    if (fileExists(location.coreLibrary)) {
      coreLibrary = dlopen(
        location.coreLibrary.c_str(), RTLD_NOW | RTLD_LOCAL);
    }
    library = dlopen(
      location.library.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (!library) {
      unload();
      return false;
    }

    const bool complete =
      bind(newInstance, "libvlc_new") &&
      bind(releaseInstance, "libvlc_release") &&
      bind(newMediaPath, "libvlc_media_new_path") &&
      bind(releaseMedia, "libvlc_media_release") &&
      bind(newPlayerFromMedia, "libvlc_media_player_new_from_media") &&
      bind(releasePlayer, "libvlc_media_player_release") &&
      bind(play, "libvlc_media_player_play") &&
      bind(setPause, "libvlc_media_player_set_pause") &&
      bind(stop, "libvlc_media_player_stop") &&
      bind(setTime, "libvlc_media_player_set_time") &&
      bind(getTime, "libvlc_media_player_get_time") &&
      bind(setRate, "libvlc_media_player_set_rate") &&
      bind(setMute, "libvlc_audio_set_mute") &&
      bind(setVideoCallbacks, "libvlc_video_set_callbacks") &&
      bind(setVideoFormatCallbacks,
        "libvlc_video_set_format_callbacks");
    if (!complete) {
      unload();
      return false;
    }
    // Disponivel no VLC 3 e usado para publicar imediatamente o quadro de
    // um seek enquanto o player esta pausado. Continua opcional para manter
    // compatibilidade com runtimes que nao exportem essa funcao.
    bind(nextFrame, "libvlc_media_player_next_frame");
    return true;
  }
};

std::mutex g_sharedVlcInstanceMutex;
libvlc_instance_t* g_sharedVlcInstance = nullptr;
int g_sharedVlcInstanceUsers = 0;

libvlc_instance_t* acquireSharedVlcInstance(
  VlcApi& api,
  const std::vector<const char*>& arguments)
{
  std::lock_guard<std::mutex> lock(g_sharedVlcInstanceMutex);
  if (!g_sharedVlcInstance) {
    g_sharedVlcInstance = api.newInstance(
      static_cast<int>(arguments.size()), arguments.data());
  }
  if (g_sharedVlcInstance) ++g_sharedVlcInstanceUsers;
  return g_sharedVlcInstance;
}

void releaseSharedVlcInstance(
  VlcApi& api,
  libvlc_instance_t*& instance)
{
  if (!instance) return;
  std::lock_guard<std::mutex> lock(g_sharedVlcInstanceMutex);
  instance = nullptr;
  if (g_sharedVlcInstanceUsers > 0) --g_sharedVlcInstanceUsers;
  if (g_sharedVlcInstanceUsers == 0 && g_sharedVlcInstance) {
    api.releaseInstance(g_sharedVlcInstance);
    g_sharedVlcInstance = nullptr;
  }
}

std::int64_t secondsToMilliseconds(double seconds)
{
  const double safe = std::max(0.0, seconds);
  const double value = safe * 1000.0;
  if (value >= static_cast<double>(
        std::numeric_limits<std::int64_t>::max())) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return static_cast<std::int64_t>(std::llround(value));
}

} // namespace

struct Decoder::Impl {
  struct Request {
    std::string path;
    std::string playbackKey;
    double sourceTime = 0.0;
    double playbackRate = 1.0;
    int requestedWidth = 1;
    int requestedHeight = 1;
    bool playing = false;
    std::uint64_t serial = 0;
  };

  VlcApi api;
  libvlc_instance_t* instance = nullptr;
  libvlc_media_t* media = nullptr;
  libvlc_media_player_t* player = nullptr;
  std::mutex stateMutex;
  std::condition_variable stateChanged;
  std::thread worker;
  Request request;
  bool hasRequest = false;
  bool stopRequested = false;
  std::atomic<int> statusCode{0};
  std::atomic<double> latestRequestedTime{0.0};

  std::mutex frameMutex;
  std::array<std::shared_ptr<std::vector<std::uint8_t>>, 4> framePool;
  std::array<bool, 4> frameLocked{{false, false, false, false}};
  std::size_t nextPoolIndex = 0;
  std::shared_ptr<const std::vector<std::uint8_t>> publishedFrame;
  std::size_t publishedPixelOffset = 0;
  std::string publishedPath;
  std::string publishedPlaybackKey;
  int publishedWidth = 0;
  int publishedHeight = 0;
  int publishedStride = 0;
  double publishedTimestamp = -1.0;
  int formatWidth = 1;
  int formatHeight = 1;
  int formatStride = 4;
  int formatLines = 1;
  int maximumWidth = 1;
  int maximumHeight = 1;
  std::string activePath;
  std::string activePlaybackKey;
  bool activePlaying = false;
  double activeRate = 1.0;
  double activeStoppedSourceTime = -1.0;
  bool playerStarted = false;
  bool previousRequestValid = false;
  double previousRequestSourceTime = 0.0;
  std::chrono::steady_clock::time_point previousRequestWall{};

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

  static unsigned formatCallback(
    void** opaque,
    char* chroma,
    unsigned* width,
    unsigned* height,
    unsigned* pitches,
    unsigned* lines)
  {
    if (!opaque || !*opaque || !chroma || !width || !height ||
        !pitches || !lines || *width == 0 || *height == 0) {
      return 0;
    }
    auto* self = static_cast<Impl*>(*opaque);
    const double scale = std::min(
      static_cast<double>(std::max(1, self->maximumWidth)) /
        static_cast<double>(*width),
      static_cast<double>(std::max(1, self->maximumHeight)) /
        static_cast<double>(*height));
    const double boundedScale = std::max(
      0.0001, std::min(1.0, scale));
    unsigned outputWidth = std::max(
      2u, static_cast<unsigned>(
        std::lround(static_cast<double>(*width) * boundedScale)));
    unsigned outputHeight = std::max(
      2u, static_cast<unsigned>(
        std::lround(static_cast<double>(*height) * boundedScale)));
    // Mantem dimensoes pares para evitar a faixa verde que alguns decoders
    // produzem ao converter videos com altura impar.
    outputWidth &= ~1u;
    outputHeight &= ~1u;
    // Os renderizadores Win32 e CoreGraphics consomem BGRA top-down. Usar o
    // formato explicito evita que a ordem dos canais de RV32 varie conforme
    // a plataforma ou o conversor escolhido pelo VLC.
    std::memcpy(chroma, "BGRA", 4);
    *width = outputWidth;
    *height = outputHeight;
    // Uma linha BGRA ja e naturalmente alinhada para GDI/CoreGraphics. Nao
    // anuncia linhas extras ao vmem: alguns conversores preenchem esse espaco
    // como imagem valida e o resultado aparece corrompido no Windows.
    const unsigned outputPitch = outputWidth * 4u;
    const unsigned outputLines = outputHeight;
    pitches[0] = outputPitch;
    lines[0] = outputLines;
    {
      std::lock_guard<std::mutex> lock(self->frameMutex);
      self->formatWidth = static_cast<int>(outputWidth);
      self->formatHeight = static_cast<int>(outputHeight);
      self->formatStride = static_cast<int>(outputPitch);
      self->formatLines = static_cast<int>(outputLines);
      self->publishedFrame.reset();
      self->publishedWidth = 0;
      self->publishedHeight = 0;
      self->publishedStride = 0;
      self->frameLocked.fill(false);
    }
    return static_cast<unsigned>(self->framePool.size());
  }

  static void cleanupCallback(void* opaque)
  {
    auto* self = static_cast<Impl*>(opaque);
    if (!self) return;
    std::lock_guard<std::mutex> lock(self->frameMutex);
    self->frameLocked.fill(false);
  }

  std::shared_ptr<std::vector<std::uint8_t>> acquireFrame(
    std::size_t bytes,
    std::size_t& selectedIndex)
  {
    for (std::size_t attempt = 0;
         attempt < framePool.size(); ++attempt) {
      const std::size_t index =
        (nextPoolIndex + attempt) % framePool.size();
      if (frameLocked[index]) continue;
      auto& candidate = framePool[index];
      if (!candidate || candidate.use_count() == 1) {
        if (!candidate) {
          candidate =
            std::make_shared<std::vector<std::uint8_t>>();
        }
        candidate->resize(bytes);
        nextPoolIndex = (index + 1) % framePool.size();
        selectedIndex = index;
        return candidate;
      }
    }
    for (std::size_t attempt = 0;
         attempt < framePool.size(); ++attempt) {
      const std::size_t index =
        (nextPoolIndex + attempt) % framePool.size();
      if (frameLocked[index]) continue;
      auto replacement =
        std::make_shared<std::vector<std::uint8_t>>(bytes);
      framePool[index] = replacement;
      nextPoolIndex = (index + 1) % framePool.size();
      selectedIndex = index;
      return replacement;
    }
    return {};
  }

  static void* lockCallback(void* opaque, void** planes)
  {
    auto* self = static_cast<Impl*>(opaque);
    if (!self || !planes) return nullptr;
    std::lock_guard<std::mutex> lock(self->frameMutex);
    const std::size_t stride = static_cast<std::size_t>(
      std::max(4, self->formatStride));
    const std::size_t bytes = stride * static_cast<std::size_t>(
      std::max(1, self->formatLines));
    std::size_t bufferIndex = 0;
    const auto frame = self->acquireFrame(
      bytes + 31u, bufferIndex);
    if (!frame) return nullptr;
    self->frameLocked[bufferIndex] = true;
    const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(
      frame->data());
    const std::uintptr_t aligned = (raw + 31u) & ~std::uintptr_t(31u);
    *planes = reinterpret_cast<std::uint8_t*>(aligned);
    return frame.get();
  }

  static void unlockCallback(
    void*, void*, void* const*) {}

  static void displayCallback(void* opaque, void* picture)
  {
    auto* self = static_cast<Impl*>(opaque);
    if (!self || !picture) return;
    std::lock_guard<std::mutex> lock(self->frameMutex);
    std::size_t bufferIndex = self->framePool.size();
    for (std::size_t index = 0;
         index < self->framePool.size(); ++index) {
      if (self->framePool[index].get() == picture) {
        bufferIndex = index;
        break;
      }
    }
    if (bufferIndex >= self->framePool.size()) return;
    const auto& frame = self->framePool[bufferIndex];
    self->frameLocked[bufferIndex] = false;
    if (!frame || self->activePath.empty()) return;
    self->publishedFrame = frame;
    const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(
      frame->data());
    const std::uintptr_t aligned = (raw + 31u) & ~std::uintptr_t(31u);
    self->publishedPixelOffset = static_cast<std::size_t>(
      reinterpret_cast<const std::uint8_t*>(aligned) - frame->data());
    self->publishedPath = self->activePath;
    self->publishedPlaybackKey = self->activePlaybackKey;
    self->publishedWidth = self->formatWidth;
    self->publishedHeight = self->formatHeight;
    self->publishedStride = self->formatStride;
    self->publishedTimestamp =
      self->latestRequestedTime.load(std::memory_order_relaxed);
    self->statusCode.store(2, std::memory_order_release);
  }

  void clearPublished()
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    frameLocked.fill(false);
    publishedFrame.reset();
    publishedPixelOffset = 0;
    publishedPath.clear();
    publishedPlaybackKey.clear();
    publishedWidth = 0;
    publishedHeight = 0;
    publishedStride = 0;
    publishedTimestamp = -1.0;
  }

  void destroyPlayer()
  {
    if (player) {
      api.stop(player);
      api.releasePlayer(player);
      player = nullptr;
    }
    if (media) {
      api.releaseMedia(media);
      media = nullptr;
    }
    playerStarted = false;
    previousRequestValid = false;
    activeStoppedSourceTime = -1.0;
    {
      std::lock_guard<std::mutex> lock(frameMutex);
      activePath.clear();
      activePlaybackKey.clear();
    }
    clearPublished();
  }

  bool initializeApi()
  {
    if (!api.load()) {
      statusCode.store(-200, std::memory_order_release);
      return false;
    }
    setEnvironmentValue("VLC_PLUGIN_PATH", api.pluginDirectory);
    std::vector<std::string> ownedArguments = {
      "--intf=dummy",
      "--no-audio",
      "--no-video-title-show",
      "--no-osd",
      "--no-stats",
      "--quiet"
    };
    std::vector<const char*> arguments;
    arguments.reserve(ownedArguments.size());
    for (const std::string& argument : ownedArguments) {
      arguments.push_back(argument.c_str());
    }
    // TP1 e TP2 usam a mesma instancia do VLC. Cada janela conserva seu
    // media player independente, mas deixa de inicializar um segundo motor
    // completo de decodificacao, evitando que a TP2 perca fluidez.
    instance = acquireSharedVlcInstance(api, arguments);
    if (!instance) {
      statusCode.store(-201, std::memory_order_release);
      return false;
    }
    return true;
  }

  bool createPlayer(const Request& current)
  {
    destroyPlayer();
    maximumWidth = std::max(2, current.requestedWidth);
    maximumHeight = std::max(2, current.requestedHeight);
    {
      std::lock_guard<std::mutex> lock(frameMutex);
      activePath = current.path;
      activePlaybackKey = current.playbackKey;
    }
    media = api.newMediaPath(instance, current.path.c_str());
    if (!media) {
      statusCode.store(-202, std::memory_order_release);
      return false;
    }
    player = api.newPlayerFromMedia(media);
    if (!player) {
      statusCode.store(-203, std::memory_order_release);
      destroyPlayer();
      return false;
    }
    api.setVideoCallbacks(
      player, &Impl::lockCallback, &Impl::unlockCallback,
      &Impl::displayCallback, this);
    api.setVideoFormatCallbacks(
      player, &Impl::formatCallback, &Impl::cleanupCallback);
    api.setMute(player, 1);
    api.setRate(player, static_cast<float>(
      std::max(0.05, std::min(8.0, current.playbackRate))));
    statusCode.store(1, std::memory_order_release);
    if (api.play(player) != 0) {
      statusCode.store(-204, std::memory_order_release);
      destroyPlayer();
      return false;
    }
    playerStarted = true;
    if (current.playing) {
      api.setTime(player, secondsToMilliseconds(current.sourceTime));
      api.setPause(player, 0);
      activeStoppedSourceTime = -1.0;
    } else {
      // Pause antes do seek impede o player recém-criado de avançar alguns
      // quadros por conta própria enquanto o transporte do REAPER está parado.
      api.setPause(player, 1);
      api.setTime(player, secondsToMilliseconds(current.sourceTime));
      if (api.nextFrame) api.nextFrame(player);
      activeStoppedSourceTime = current.sourceTime;
    }
    activePlaying = current.playing;
    activeRate = current.playbackRate;
    previousRequestValid = true;
    previousRequestSourceTime = current.sourceTime;
    previousRequestWall = std::chrono::steady_clock::now();
    return true;
  }

  void applyRequest(const Request& current)
  {
    latestRequestedTime.store(
      current.sourceTime, std::memory_order_relaxed);
    if (current.path.empty()) {
      destroyPlayer();
      statusCode.store(0, std::memory_order_release);
      return;
    }
    const bool identityChanged =
      !player || current.path != activePath ||
      current.playbackKey != activePlaybackKey;
    // Durante o play o tamanho disponivel pode oscilar alguns pixels a cada
    // pintura. Recriar o media player por causa disso interrompe a decodificacao
    // (principalmente no macOS). Porem, quando a janela cresce muito (por
    // exemplo, ao entrar em tela cheia/Retina), manter a resolucao negociada na
    // janela pequena deixa o video visivelmente borrado. Nesse crescimento
    // grande renegocia uma vez, conservando o ultimo frame durante o aquecimento.
    const int widthTolerance = std::max(256, maximumWidth / 2);
    const int heightTolerance = std::max(144, maximumHeight / 2);
    const bool dimensionsChanged = player && !current.playing &&
      (std::abs(current.requestedWidth - maximumWidth) > widthTolerance ||
       std::abs(current.requestedHeight - maximumHeight) > heightTolerance);
    const bool qualityUpgradeNeeded = player && current.playing &&
      (current.requestedWidth > maximumWidth + widthTolerance ||
       current.requestedHeight > maximumHeight + heightTolerance);
    if (identityChanged || dimensionsChanged || qualityUpgradeNeeded) {
      createPlayer(current);
      return;
    }
    if (!player) return;

    const double safeRate =
      std::max(0.05, std::min(8.0, current.playbackRate));
    if (std::abs(safeRate - activeRate) > 0.0001) {
      api.setRate(player, static_cast<float>(safeRate));
      activeRate = safeRate;
    }
    const auto now = std::chrono::steady_clock::now();
    const bool wasPlaying = activePlaying;
    bool playbackJump = false;
    bool playbackRestart = false;
    if (previousRequestValid && wasPlaying && current.playing) {
      const double elapsed = std::chrono::duration<double>(
        now - previousRequestWall).count();
      const double expectedAdvance = elapsed * safeRate;
      const double actualAdvance =
        current.sourceTime - previousRequestSourceTime;
      // Corrige apenas um salto real do transporte (seek, loop ou troca de
      // posicao). Pequenos desvios entre os relogios do REAPER e do VLC nao
      // podem gerar set_time, pois cada chamada interrompe o fluxo de frames.
      playbackJump = std::abs(actualAdvance - expectedAdvance) > 0.30;
      // Se o mesmo item voltar ao inicio antes de o worker observar o Stop,
      // o player do VLC pode continuar no estado Ended. set_time sozinho nao
      // retira esse estado; um novo play e necessario antes do seek.
      playbackRestart = actualAdvance < -0.15;
    }

    if (current.playing != activePlaying) {
      if (current.playing) {
        // libVLC nao sai do estado Ended apenas com set_pause(0). Chamar play
        // tambem funciona como resume quando o player estava pausado.
        if (api.play(player) != 0) {
          createPlayer(current);
          return;
        }
        const libvlc_time_t playerTime = api.getTime(player);
        const double drift = playerTime >= 0
          ? std::abs(
              static_cast<double>(playerTime) / 1000.0 -
              current.sourceTime)
          : std::numeric_limits<double>::infinity();
        if (drift > 0.075) {
          api.setTime(player, secondsToMilliseconds(current.sourceTime));
        }
        api.setPause(player, 0);
        activeStoppedSourceTime = -1.0;
      } else {
        api.setPause(player, 1);
      }
      activePlaying = current.playing;
    }

    if (!current.playing) {
      // A transicao Play -> Stop sempre força o quadro do cursor de edicao.
      // Depois disso, somente uma mudança real no cursor solicita outro
      // quadro. Comparar com get_time() criava um ciclo: next_frame avançava,
      // o avanço era tratado como drift, o código voltava e avançava de novo.
      const bool stoppedTargetChanged =
        activeStoppedSourceTime < 0.0 ||
        std::abs(
          activeStoppedSourceTime - current.sourceTime) > 0.008;
      if (wasPlaying || stoppedTargetChanged) {
        api.setTime(player, secondsToMilliseconds(current.sourceTime));
        if (api.nextFrame) api.nextFrame(player);
        activeStoppedSourceTime = current.sourceTime;
      }
    } else if (wasPlaying && playbackJump) {
      if (playbackRestart && api.play(player) != 0) {
        createPlayer(current);
        return;
      }
      api.setTime(player, secondsToMilliseconds(current.sourceTime));
    }

    previousRequestValid = true;
    previousRequestSourceTime = current.sourceTime;
    previousRequestWall = now;
  }

  void workerLoop()
  {
    if (!initializeApi()) return;
    std::uint64_t handledSerial = 0;
    for (;;) {
      Request current;
      {
        std::unique_lock<std::mutex> lock(stateMutex);
        stateChanged.wait_for(
          lock, std::chrono::milliseconds(15), [&]() {
            return stopRequested ||
              (hasRequest && request.serial != handledSerial);
          });
        if (stopRequested) break;
        if (!hasRequest) continue;
        current = request;
        handledSerial = current.serial;
      }
      applyRequest(current);
    }
    destroyPlayer();
    releaseSharedVlcInstance(api, instance);
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
    latestRequestedTime.store(
      std::max(0.0, sourceTime), std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      request.path = path;
      request.playbackKey = playbackKey;
      request.sourceTime = std::max(0.0, sourceTime);
      request.playing = playing;
      request.playbackRate = playbackRate;
      request.requestedWidth = std::max(2, requestedWidth);
      request.requestedHeight = std::max(2, requestedHeight);
      ++request.serial;
      hasRequest = true;
    }
    stateChanged.notify_one();

    {
      std::lock_guard<std::mutex> lock(frameMutex);
      if (publishedFrame && publishedPath == path &&
          publishedPlaybackKey == playbackKey &&
          publishedWidth > 0 && publishedHeight > 0 &&
          publishedStride >= publishedWidth * 4) {
        output.storage = publishedFrame;
        output.pixels =
          publishedFrame->data() + publishedPixelOffset;
        output.width = publishedWidth;
        output.height = publishedHeight;
        output.stride = publishedStride;
        output.timestamp = publishedTimestamp;
        return true;
      }
    }
    return false;
  }

  void reset()
  {
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      request.path.clear();
      request.playbackKey = "__reset__";
      request.sourceTime = 0.0;
      request.playing = false;
      ++request.serial;
      hasRequest = true;
    }
    clearPublished();
    stateChanged.notify_one();
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
    ? impl_->statusCode.load(std::memory_order_acquire)
    : -299;
}

} // namespace vshook_video

#endif

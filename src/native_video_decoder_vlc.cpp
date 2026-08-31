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

// Mantem os mesmos valores de libvlc_state_t sem exigir os headers do VLC no
// computador que compila a extensao. O runtime continua carregado dinamicamente.
enum VlcPlayerState : int {
  kVlcNothingSpecial = 0,
  kVlcOpening = 1,
  kVlcBuffering = 2,
  kVlcPlaying = 3,
  kVlcPaused = 4,
  kVlcStopped = 5,
  kVlcEnded = 6,
  kVlcError = 7
};

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
  // A DLL da extensao normalmente esta em %APPDATA%\\REAPER\\UserPlugins.
  // Este caminho explicito cobre hosts que carregam a DLL de um alias e nao
  // deixam GetModuleFileName resolver a pasta esperada.
  const std::string appData = environmentValue("APPDATA");
  if (!appData.empty()) {
    roots.push_back(joinPath(
      appData, "REAPER/UserPlugins/VSHookRuntime/VLC"));
  }
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
  int (*getState)(libvlc_media_player_t*) = nullptr;
  int (*setRate)(libvlc_media_player_t*, float) = nullptr;
  int (*setMute)(libvlc_media_player_t*, int) = nullptr;
#ifdef _WIN32
  void (*setNativeWindow)(libvlc_media_player_t*, void*) = nullptr;
#else
  void (*setNativeWindow)(libvlc_media_player_t*, void*) = nullptr;
#endif
  void (*setAspectRatio)(libvlc_media_player_t*, const char*) = nullptr;
  void (*setScale)(libvlc_media_player_t*, float) = nullptr;
  void (*setMouseInput)(libvlc_media_player_t*, unsigned) = nullptr;
  void (*setKeyInput)(libvlc_media_player_t*, unsigned) = nullptr;
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
    // Tambem e opcional: o relogio e o heartbeat de video continuam sendo a
    // verificacao de seguranca quando um runtime compativel nao exporta state.
    bind(getState, "libvlc_media_player_get_state");
#ifdef _WIN32
    bind(setNativeWindow, "libvlc_media_player_set_hwnd");
#else
    bind(setNativeWindow, "libvlc_media_player_set_nsobject");
#endif
    bind(setAspectRatio, "libvlc_video_set_aspect_ratio");
    bind(setScale, "libvlc_video_set_scale");
    bind(setMouseInput, "libvlc_video_set_mouse_input");
    bind(setKeyInput, "libvlc_video_set_key_input");
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

std::string mediaPathForVlc(const std::string& path)
{
#ifdef _WIN32
  // O estado do Teleprompt usa barras de URL para permanecer portavel entre
  // Windows e macOS. libvlc_media_new_path, porem, exige o caminho nativo no
  // Windows e devolve nullptr para alguns caminhos absolutos com '/'.
  std::string nativePath = path;
  std::replace(nativePath.begin(), nativePath.end(), '/', '\\');
  return nativePath;
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
    int requestedWidth = 1;
    int requestedHeight = 1;
    bool playing = false;
    std::uint64_t serial = 0;
  };

  struct FrameHeartbeat {
    bool hasFrame = false;
    std::uint64_t sequence = 0;
    std::chrono::steady_clock::time_point publishedAt{};
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
  std::uint64_t publishedFrameSequence = 0;
  std::uint64_t publishedTimestampSequence = 0;
  std::chrono::steady_clock::time_point lastFramePublishedAt{};
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
  bool initialSyncPending = false;
  std::chrono::steady_clock::time_point initialSyncForceAt{};
  std::uint64_t initialSyncFrameSequence = 0;
  std::chrono::steady_clock::time_point lastInitialStoppedSeekAt{};
  std::chrono::steady_clock::time_point playerCreatedAt{};
  std::chrono::steady_clock::time_point watchdogGraceUntil{};
  std::chrono::steady_clock::time_point lastWatchdogCheckAt{};
  std::chrono::steady_clock::time_point lastWatchdogRecoveryAt{};
  double lastWatchdogRequestedTime = 0.0;
  double lastWatchdogPlayerTime = -1.0;
  std::uint64_t lastWatchdogFrameSequence = 0;
  int watchdogClockStallChecks = 0;
  int watchdogRecoveryStage = 0;
  bool loopRestartPending = false;
  double loopRestartTarget = 0.0;
  std::uint64_t loopRestartFrameSequence = 0;
  std::chrono::steady_clock::time_point loopRestartRequestedAt{};
  std::chrono::steady_clock::time_point loopRestartGuardUntil{};

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
    ++self->publishedFrameSequence;
    // Nao chama nenhuma API do libVLC dentro do callback de video: no Windows
    // isso pode reentrar no lock interno do vout e prender o decoder. A worker
    // associa o relogio real do player a esta sequencia logo em seguida.
    self->publishedTimestamp = -1.0;
    self->lastFramePublishedAt =
      std::chrono::steady_clock::now();
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
    publishedTimestampSequence = 0;
    lastFramePublishedAt = {};
  }

  void stampPublishedFrameFromPlayer()
  {
    if (!player || !api.getTime) return;
    std::uint64_t sequence = 0;
    {
      std::lock_guard<std::mutex> lock(frameMutex);
      if (!publishedFrame ||
          publishedTimestampSequence == publishedFrameSequence) {
        return;
      }
      sequence = publishedFrameSequence;
    }
    // Esta consulta roda exclusivamente na worker, fora do callback do vout.
    const libvlc_time_t playerTime = api.getTime(player);
    if (playerTime < 0) return;
    const double playerTimeSeconds =
      static_cast<double>(playerTime) / 1000.0;
    const double requestedTime =
      latestRequestedTime.load(std::memory_order_relaxed);
    // Um callback que ja estava na fila pode chegar depois do seek para tras.
    // Durante a emenda, descarta qualquer quadro cujo relogio ainda pertence
    // ao ciclo anterior, como a janela de video do REAPER faz pela timeline.
    if (loopRestartGuardUntil !=
          std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() < loopRestartGuardUntil &&
        std::abs(playerTimeSeconds - requestedTime) > 0.75) {
      return;
    }
    std::lock_guard<std::mutex> lock(frameMutex);
    if (publishedFrame && publishedFrameSequence == sequence) {
      publishedTimestamp = playerTimeSeconds;
      publishedTimestampSequence = sequence;
    }
  }

  FrameHeartbeat readFrameHeartbeat()
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    FrameHeartbeat heartbeat;
    heartbeat.hasFrame = publishedFrame != nullptr;
    heartbeat.sequence = publishedFrameSequence;
    heartbeat.publishedAt = lastFramePublishedAt;
    return heartbeat;
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
    activePlaying = false;
    previousRequestValid = false;
    activeStoppedSourceTime = -1.0;
    initialSyncPending = false;
    initialSyncForceAt = {};
    initialSyncFrameSequence = 0;
    lastInitialStoppedSeekAt = {};
    playerCreatedAt = {};
    watchdogGraceUntil = {};
    lastWatchdogCheckAt = {};
    lastWatchdogRecoveryAt = {};
    lastWatchdogRequestedTime = 0.0;
    lastWatchdogPlayerTime = -1.0;
    lastWatchdogFrameSequence = 0;
    watchdogClockStallChecks = 0;
    watchdogRecoveryStage = 0;
    loopRestartPending = false;
    loopRestartTarget = 0.0;
    loopRestartFrameSequence = 0;
    loopRestartRequestedAt = {};
    loopRestartGuardUntil = {};
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
      "--avcodec-hw=any",
      "--drop-late-frames",
      "--skip-frames",
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

  bool createPlayer(
    const Request& current,
    int recoveryStage = 0)
  {
    const int requestedRecoveryStage =
      std::max(0, recoveryStage);
    destroyPlayer();
    maximumWidth = std::max(2, current.requestedWidth);
    maximumHeight = std::max(2, current.requestedHeight);
    {
      std::lock_guard<std::mutex> lock(frameMutex);
      activePath = current.path;
      activePlaybackKey = current.playbackKey;
    }
    const std::string mediaPath = mediaPathForVlc(current.path);
    media = api.newMediaPath(instance, mediaPath.c_str());
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
    const double safeRate =
      std::max(0.05, std::min(8.0, current.playbackRate));
    api.setRate(player, static_cast<float>(safeRate));
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
      // Um player novo ainda nao possui quadro para uma pausa. Mantem o vout
      // acordado somente durante a busca inicial; finishInitialSync pausa assim
      // que o primeiro quadro correspondente ao cursor for realmente publicado.
      api.setTime(player, secondsToMilliseconds(current.sourceTime));
      api.setPause(player, 0);
      activeStoppedSourceTime = -1.0;
    }
    activePlaying = current.playing;
    activeRate = safeRate;
    const auto now = std::chrono::steady_clock::now();
    const FrameHeartbeat heartbeat = readFrameHeartbeat();
    // play() e assíncrono. O primeiro seek pode chegar enquanto o VLC ainda
    // esta em Opening e ser simplesmente ignorado. Assim que o player ficar
    // pronto, applyRequest reaplica uma unica vez o alvo mais recente.
    initialSyncPending = true;
    initialSyncForceAt = now + std::chrono::milliseconds(1500);
    initialSyncFrameSequence = heartbeat.sequence;
    lastInitialStoppedSeekAt = now;
    playerCreatedAt = now;
    watchdogGraceUntil = now + std::chrono::milliseconds(1500);
    lastWatchdogCheckAt = now;
    lastWatchdogRecoveryAt = requestedRecoveryStage > 0
      ? now : std::chrono::steady_clock::time_point{};
    lastWatchdogRequestedTime = current.sourceTime;
    lastWatchdogPlayerTime = -1.0;
    lastWatchdogFrameSequence = heartbeat.sequence;
    watchdogClockStallChecks = 0;
    watchdogRecoveryStage = requestedRecoveryStage;
    previousRequestValid = true;
    previousRequestSourceTime = current.sourceTime;
    previousRequestWall = now;
    return true;
  }

  void armWatchdogGrace(
    const Request& current,
    const std::chrono::steady_clock::time_point now,
    std::chrono::milliseconds grace)
  {
    const FrameHeartbeat heartbeat = readFrameHeartbeat();
    watchdogGraceUntil = now + grace;
    lastWatchdogCheckAt = now;
    lastWatchdogRequestedTime = current.sourceTime;
    lastWatchdogPlayerTime = -1.0;
    lastWatchdogFrameSequence = heartbeat.sequence;
    watchdogClockStallChecks = 0;
  }

  bool finishLoopRestart(
    const Request& current,
    const std::chrono::steady_clock::time_point now)
  {
    if (!loopRestartPending || !player || !current.playing) {
      return false;
    }
    const FrameHeartbeat heartbeat = readFrameHeartbeat();
    const libvlc_time_t rawPlayerTime = api.getTime(player);
    const double playerTime = rawPlayerTime >= 0
      ? static_cast<double>(rawPlayerTime) / 1000.0 : -1.0;
    const bool newFrameArrived =
      heartbeat.sequence != loopRestartFrameSequence;
    const bool clockReachedNewCycle = playerTime >= 0.0 &&
      std::abs(playerTime - current.sourceTime) <= 0.35 &&
      playerTime < loopRestartTarget + 1.0;
    if (newFrameArrived && clockReachedNewCycle) {
      loopRestartPending = false;
      loopRestartRequestedAt = {};
      watchdogRecoveryStage = 0;
      return false;
    }
    if (loopRestartRequestedAt !=
          std::chrono::steady_clock::time_point{} &&
        now - loopRestartRequestedAt >=
          std::chrono::milliseconds(90)) {
      // Alguns demuxers aceitam play() em Ended, mas continuam entregando o
      // ultimo quadro. A janela do REAPER nao espera esse estado indefinido;
      // apos uma tolerancia de poucos frames, abre outro player VLC ja no
      // tempo correto. A janela conserva o ultimo snapshot durante o aquecimento.
      createPlayer(current, 1);
      return true;
    }
    return false;
  }

  bool finishInitialSync(
    const Request& current,
    const std::chrono::steady_clock::time_point now,
    double safeRate)
  {
    if (!initialSyncPending || !player) return false;

    const int playerState = api.getState
      ? api.getState(player) : -1;
    const libvlc_time_t playerTime = api.getTime(player);
    const bool stateReady = current.playing
      ? playerState == kVlcPlaying
      : (playerState == kVlcPlaying ||
         playerState == kVlcPaused ||
         playerState == kVlcStopped ||
         playerState == kVlcEnded);
    const bool stateUnavailableButClockReady =
      playerState < 0 && playerTime >= 0;
    const bool openingTimedOut =
      initialSyncForceAt !=
        std::chrono::steady_clock::time_point{} &&
      now >= initialSyncForceAt;

    // Aguarda o play assincrono sair de Opening/Buffering. Enquanto isso o
    // comando inicial continua valendo como best effort e a UI nunca bloqueia.
    if (!stateReady && !stateUnavailableButClockReady &&
        !openingTimedOut) {
      return true;
    }

    if (playerState == kVlcError) {
      createPlayer(
        current, std::max(1, watchdogRecoveryStage + 1));
      return true;
    }

    api.setRate(player, static_cast<float>(safeRate));
    activeRate = safeRate;
    if (current.playing) {
      if (api.play(player) != 0) {
        createPlayer(
          current, std::max(1, watchdogRecoveryStage + 1));
        return true;
      }
      api.setTime(
        player, secondsToMilliseconds(current.sourceTime));
      api.setPause(player, 0);
      activeStoppedSourceTime = -1.0;
    } else {
      const FrameHeartbeat heartbeat = readFrameHeartbeat();
      const double playerTimeSeconds = playerTime >= 0
        ? static_cast<double>(playerTime) / 1000.0 : -1.0;
      const bool targetFramePublished =
        heartbeat.sequence != initialSyncFrameSequence &&
        playerTimeSeconds >= 0.0 &&
        std::abs(playerTimeSeconds - current.sourceTime) <= 0.035;
      if (!targetFramePublished) {
        // O seek enviado durante Opening pode ser ignorado. Reaplica de forma
        // espaçada quando o player estiver pronto, mantendo-o ativo apenas até
        // o callback do quadro correto. Isso reproduz o frame parado do REAPER
        // sem o ciclo visual causado por next_frame.
        const bool canSeekNow = stateReady ||
          stateUnavailableButClockReady || openingTimedOut;
        if (canSeekNow &&
            (lastInitialStoppedSeekAt ==
               std::chrono::steady_clock::time_point{} ||
             now - lastInitialStoppedSeekAt >=
               std::chrono::milliseconds(75))) {
          if (api.play(player) != 0) {
            createPlayer(
              current, std::max(1, watchdogRecoveryStage + 1));
            return true;
          }
          api.setTime(
            player, secondsToMilliseconds(current.sourceTime));
          api.setPause(player, 0);
          lastInitialStoppedSeekAt = now;
        }
        return true;
      }
      api.setPause(player, 1);
      activeStoppedSourceTime = current.sourceTime;
    }
    activePlaying = current.playing;
    initialSyncPending = false;
    initialSyncForceAt = {};
    initialSyncFrameSequence = 0;
    lastInitialStoppedSeekAt = {};
    armWatchdogGrace(
      current, now, std::chrono::milliseconds(900));
    return true;
  }

  void runPlaybackWatchdog(
    const Request& current,
    const std::chrono::steady_clock::time_point now,
    double safeRate)
  {
    if (!player || !current.playing || initialSyncPending ||
        now < watchdogGraceUntil) {
      return;
    }
    if (lastWatchdogCheckAt !=
          std::chrono::steady_clock::time_point{} &&
        now - lastWatchdogCheckAt <
          std::chrono::milliseconds(250)) {
      return;
    }

    const double checkElapsed = lastWatchdogCheckAt ==
        std::chrono::steady_clock::time_point{}
      ? 0.25
      : std::max(
          0.001,
          std::chrono::duration<double>(
            now - lastWatchdogCheckAt).count());
    const double requestedAdvance =
      current.sourceTime - lastWatchdogRequestedTime;
    const libvlc_time_t rawPlayerTime = api.getTime(player);
    const double playerTime = rawPlayerTime >= 0
      ? static_cast<double>(rawPlayerTime) / 1000.0 : -1.0;
    const double playerAdvance =
      playerTime >= 0.0 && lastWatchdogPlayerTime >= 0.0
        ? playerTime - lastWatchdogPlayerTime : 0.0;
    const int playerState = api.getState
      ? api.getState(player) : -1;
    const FrameHeartbeat heartbeat = readFrameHeartbeat();

    // Um seek/loop para tras ja e tratado pelo caminho de playbackJump. Aqui
    // apenas troca a linha de base para nao confundir esse salto com stall.
    if (requestedAdvance < -0.05) {
      lastWatchdogCheckAt = now;
      lastWatchdogRequestedTime = current.sourceTime;
      lastWatchdogPlayerTime = playerTime;
      lastWatchdogFrameSequence = heartbeat.sequence;
      watchdogClockStallChecks = 0;
      watchdogGraceUntil = now + std::chrono::milliseconds(600);
      return;
    }

    const double minimumRequestedAdvance = std::max(
      0.003, checkElapsed * safeRate * 0.15);
    const bool sourceAdvancing =
      requestedAdvance > minimumRequestedAdvance;
    const double minimumPlayerAdvance = std::max(
      0.002, std::max(0.0, requestedAdvance) * 0.10);
    const bool playerClockAdvanced =
      playerTime >= 0.0 && lastWatchdogPlayerTime >= 0.0 &&
      playerAdvance >= minimumPlayerAdvance;
    const bool frameAdvanced =
      heartbeat.sequence != lastWatchdogFrameSequence;

    if (sourceAdvancing && lastWatchdogPlayerTime >= 0.0 &&
        playerTime >= 0.0 && !playerClockAdvanced) {
      ++watchdogClockStallChecks;
    } else {
      watchdogClockStallChecks = 0;
    }

    const bool invalidPlayingState = sourceAdvancing &&
      (playerState == kVlcPaused ||
       playerState == kVlcStopped ||
       playerState == kVlcEnded ||
       playerState == kVlcError);
    const bool openingTooLong = sourceAdvancing &&
      (playerState == kVlcNothingSpecial ||
       playerState == kVlcOpening ||
       playerState == kVlcBuffering) &&
      now - playerCreatedAt >= std::chrono::seconds(5);
    const bool driftTooLarge = sourceAdvancing &&
      playerTime >= 0.0 &&
      std::abs(playerTime - current.sourceTime) > 0.75;
    const bool clockUnavailable = sourceAdvancing &&
      playerTime < 0.0 &&
      now - playerCreatedAt >= std::chrono::seconds(5);
    const bool frameStale = sourceAdvancing &&
      ((heartbeat.hasFrame &&
        heartbeat.publishedAt !=
          std::chrono::steady_clock::time_point{} &&
        now - heartbeat.publishedAt >= std::chrono::seconds(2)) ||
       (!heartbeat.hasFrame &&
        now - playerCreatedAt >= std::chrono::seconds(5)));
    const bool clockStalled = sourceAdvancing &&
      watchdogClockStallChecks >= 2;
    const bool unhealthy = invalidPlayingState || openingTooLong ||
      driftTooLarge || clockUnavailable || frameStale || clockStalled;

    lastWatchdogCheckAt = now;
    lastWatchdogRequestedTime = current.sourceTime;
    lastWatchdogPlayerTime = playerTime;
    lastWatchdogFrameSequence = heartbeat.sequence;

    if (!unhealthy) {
      // So considera a recuperacao concluida quando alguma saida real do VLC
      // voltou a avançar; a intencao activePlaying, sozinha, nao basta.
      if (sourceAdvancing &&
          (playerClockAdvanced || frameAdvanced) &&
          playerState != kVlcPaused &&
          playerState != kVlcStopped &&
          playerState != kVlcEnded &&
          playerState != kVlcError) {
        watchdogRecoveryStage = 0;
      }
      return;
    }

    if (lastWatchdogRecoveryAt !=
          std::chrono::steady_clock::time_point{} &&
        now - lastWatchdogRecoveryAt <
          std::chrono::milliseconds(800)) {
      return;
    }

    if (watchdogRecoveryStage == 0 &&
        playerState != kVlcError) {
      // Primeiro tenta a retomada barata. Isso tira o VLC de Paused/Ended e
      // preserva o player, o ultimo quadro e os buffers ja aquecidos.
      if (api.play(player) == 0) {
        api.setRate(player, static_cast<float>(safeRate));
        api.setTime(
          player, secondsToMilliseconds(current.sourceTime));
        api.setPause(player, 0);
        activePlaying = true;
        activeStoppedSourceTime = -1.0;
        watchdogRecoveryStage = 1;
        lastWatchdogRecoveryAt = now;
        statusCode.store(1, std::memory_order_release);
        armWatchdogGrace(
          current, now, std::chrono::milliseconds(900));
        return;
      }
    }

    // Se play+seek nao produziu progresso, o objeto interno do VLC esta
    // inutilizavel. Recria apenas na worker e conserva o frame desenhado pela
    // janela enquanto o novo player aquece.
    const int nextRecoveryStage =
      std::max(2, watchdogRecoveryStage + 1);
    statusCode.store(1, std::memory_order_release);
    createPlayer(current, nextRecoveryStage);
  }

  void applyRequest(const Request& current)
  {
    latestRequestedTime.store(
      current.sourceTime, std::memory_order_relaxed);
    stampPublishedFrameFromPlayer();
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
    if (finishLoopRestart(current, now)) {
      return;
    }
    // O watchdog pode estar reaquecendo o player exatamente quando B_LOOPSRC
    // volta do fim ao inicio. Se a sincronizacao inicial consumir esse pedido,
    // o salto para tras se perde e o VLC permanece preso em Ended. Reinicia o
    // proprio player VLC imediatamente já no alvo correto do novo ciclo.
    const bool restartDuringInitialSync = initialSyncPending &&
      previousRequestValid && activePlaying && current.playing &&
      current.sourceTime < previousRequestSourceTime - 0.01;
    if (restartDuringInitialSync) {
      createPlayer(
        current, std::max(1, watchdogRecoveryStage));
      return;
    }
    if (finishInitialSync(current, now, safeRate)) {
      previousRequestValid = true;
      previousRequestSourceTime = current.sourceTime;
      previousRequestWall = now;
      return;
    }
    const bool wasPlaying = activePlaying;
    bool playbackJump = false;
    bool playbackRestart = false;
    bool watchdogCommandIssued = false;
    if (previousRequestValid && wasPlaying && current.playing) {
      const double elapsed = std::chrono::duration<double>(
        now - previousRequestWall).count();
      const double expectedAdvance = elapsed * safeRate;
      const double actualAdvance =
        current.sourceTime - previousRequestSourceTime;
      // Corrige apenas um salto real do transporte (seek, loop ou troca de
      // posicao). Pequenos desvios entre os relogios do REAPER e do VLC nao
      // podem gerar set_time, pois cada chamada interrompe o fluxo de frames.
      // Se o mesmo item voltar ao inicio antes de o worker observar o Stop,
      // o player do VLC pode continuar no estado Ended. set_time sozinho nao
      // retira esse estado; um novo play e necessario antes do seek.
      // Usa um limite curto tambem para fontes pequenas/repetidas: nelas o
      // retorno inteiro pode ser menor que o limite geral de seek.
      playbackRestart = actualAdvance < -0.01;
      playbackJump = playbackRestart ||
        std::abs(actualAdvance - expectedAdvance) > 0.30;
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
        watchdogRecoveryStage = 0;
        lastWatchdogRecoveryAt = {};
        armWatchdogGrace(
          current, now, std::chrono::milliseconds(900));
        // play() pode responder sucesso ainda em Ended/Opening. Confirma o
        // reposicionamento assim que o estado real voltar a Playing.
        initialSyncPending = true;
        initialSyncForceAt =
          now + std::chrono::milliseconds(350);
        watchdogCommandIssued = true;
      } else {
        // Nao congela aqui o ultimo quadro do playback. O bloco de Stop logo
        // abaixo mantem o vout acordado somente ate publicar o quadro exato
        // da posicao para a qual o cursor de edicao voltou.
        watchdogRecoveryStage = 0;
        watchdogClockStallChecks = 0;
      }
      activePlaying = current.playing;
    }

    if (!current.playing) {
      // A transicao Play -> Stop sempre força o quadro do cursor de edicao.
      // Um set_time com o VLC pausado pode conservar na tela o ultimo quadro
      // tocado. Faz o mesmo handshake usado na abertura: play + seek, aguarda
      // o callback do quadro alvo e somente entao pausa. Depois disso, apenas
      // uma mudanca real no cursor solicita outro quadro.
      const bool stoppedTargetChanged =
        activeStoppedSourceTime < 0.0 ||
        std::abs(
          activeStoppedSourceTime - current.sourceTime) > 0.008;
      if (wasPlaying || stoppedTargetChanged) {
        const FrameHeartbeat heartbeat = readFrameHeartbeat();
        if (api.play(player) != 0) {
          createPlayer(current);
          return;
        }
        api.setTime(player, secondsToMilliseconds(current.sourceTime));
        api.setPause(player, 0);
        activeStoppedSourceTime = -1.0;
        initialSyncPending = true;
        initialSyncForceAt = now + std::chrono::milliseconds(600);
        initialSyncFrameSequence = heartbeat.sequence;
        lastInitialStoppedSeekAt = now;
        armWatchdogGrace(
          current, now, std::chrono::milliseconds(700));
        previousRequestValid = true;
        previousRequestSourceTime = current.sourceTime;
        previousRequestWall = now;
        return;
      }
    } else if (wasPlaying && playbackJump) {
      if (playbackRestart) {
        const int playerState = api.getState
          ? api.getState(player) : -1;
        // Assim como a janela de video do REAPER, a emenda e dirigida pela
        // nova posicao da fonte: reaproveita o decoder aquecido e volta ao
        // quadro inicial imediatamente. So recria o objeto se o VLC declarou
        // erro real; Ended/Stopped ainda aceitam play + seek no mesmo player.
        if (playerState == kVlcError) {
          createPlayer(
            current, std::max(1, watchdogRecoveryStage));
          return;
        }
        if (api.play(player) != 0) {
          createPlayer(current, 1);
          return;
        }
        const FrameHeartbeat heartbeat = readFrameHeartbeat();
        loopRestartPending = true;
        loopRestartTarget = current.sourceTime;
        loopRestartFrameSequence = heartbeat.sequence;
        loopRestartRequestedAt = now;
        loopRestartGuardUntil =
          now + std::chrono::milliseconds(750);
      }
      api.setTime(player, secondsToMilliseconds(current.sourceTime));
      if (playbackRestart) {
        api.setPause(player, 0);
        initialSyncPending = false;
        initialSyncForceAt = {};
        watchdogRecoveryStage = 0;
        lastWatchdogRecoveryAt = {};
      }
      armWatchdogGrace(
        current, now, playbackRestart
          ? std::chrono::milliseconds(120)
          : std::chrono::milliseconds(600));
      watchdogCommandIssued = true;
    }

    if (!watchdogCommandIssued) {
      runPlaybackWatchdog(current, now, safeRate);
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
        // O callback de video e independente da worker que consulta o relogio
        // do VLC. Exigir que a worker carimbasse cada quadro antes de entrega-lo
        // introduzia ate um ciclo inteiro de atraso e, dependendo da fase entre
        // o timer da janela e o vout, fazia a apresentacao cair visualmente para
        // metade do FPS. O pixel ja e um snapshot imutavel e pode ser apresentado
        // imediatamente; o tempo solicitado e uma referencia segura ate a worker
        // associar o relogio exato do player a esta sequencia.
        output.timestamp =
          publishedTimestampSequence == publishedFrameSequence
            ? publishedTimestamp
            : latestRequestedTime.load(std::memory_order_relaxed);
        output.sequence = publishedFrameSequence;
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

struct NativeWindowPlayer::Impl {
  struct Request {
    std::string path;
    std::string playbackKey;
    double sourceTime = 0.0;
    double playbackRate = 1.0;
    void* nativeWindow = nullptr;
    int windowWidth = 1;
    int windowHeight = 1;
    bool playing = false;
    bool stretch = false;
    std::uint64_t serial = 0;
  };

  VlcApi api;
  libvlc_instance_t* instance = nullptr;
  libvlc_media_t* media = nullptr;
  libvlc_media_player_t* player = nullptr;
  std::mutex mutex;
  std::condition_variable changed;
  std::thread worker;
  Request request;
  bool hasRequest = false;
  bool stopRequested = false;
  std::atomic<int> statusCode{0};

  std::string activePath;
  std::string activePlaybackKey;
  void* activeWindow = nullptr;
  bool activePlaying = false;
  bool activeStretch = false;
  int activeWindowWidth = 1;
  int activeWindowHeight = 1;
  double activeRate = 1.0;
  double activeStoppedTime = -1.0;
  double previousSourceTime = 0.0;
  std::chrono::steady_clock::time_point previousRequestAt{};
  std::chrono::steady_clock::time_point lastClockCorrectionAt{};
  bool previousRequestValid = false;
  bool initialStoppedFramePending = false;
  double initialStoppedTarget = 0.0;
  std::chrono::steady_clock::time_point initialStoppedDeadline{};

  Impl()
  {
    worker = std::thread([this]() { workerLoop(); });
  }

  ~Impl()
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopRequested = true;
    }
    changed.notify_all();
    if (worker.joinable()) worker.join();
  }

  bool initialize()
  {
    if (!api.load()) {
      statusCode.store(-200, std::memory_order_release);
      return false;
    }
    if (!api.setNativeWindow) {
      statusCode.store(-205, std::memory_order_release);
      return false;
    }
    setEnvironmentValue("VLC_PLUGIN_PATH", api.pluginDirectory);
    std::vector<std::string> ownedArguments = {
      "--intf=dummy",
      "--no-audio",
      "--no-video-title-show",
      "--no-osd",
      "--no-stats",
      "--avcodec-hw=any",
      "--drop-late-frames",
      "--quiet"
    };
    std::vector<const char*> arguments;
    arguments.reserve(ownedArguments.size());
    for (const std::string& argument : ownedArguments) {
      arguments.push_back(argument.c_str());
    }
    instance = acquireSharedVlcInstance(api, arguments);
    if (!instance) {
      statusCode.store(-201, std::memory_order_release);
      return false;
    }
    return true;
  }

  void destroyPlayer()
  {
    if (player) {
      api.stop(player);
      // Desanexa antes de liberar para que o vout nao conserve a superficie
      // depois de a janela dedicada ser fechada ou trocar para uma imagem.
      if (api.setNativeWindow) api.setNativeWindow(player, nullptr);
      api.releasePlayer(player);
      player = nullptr;
    }
    if (media) {
      api.releaseMedia(media);
      media = nullptr;
    }
    activePath.clear();
    activePlaybackKey.clear();
    activeWindow = nullptr;
    activePlaying = false;
    activeStoppedTime = -1.0;
    previousRequestValid = false;
    initialStoppedFramePending = false;
    initialStoppedDeadline = {};
  }

  void applyAspect(const Request& current, bool force)
  {
    if (!player || !api.setAspectRatio) return;
    if (!force && activeStretch == current.stretch &&
        activeWindowWidth == current.windowWidth &&
        activeWindowHeight == current.windowHeight) {
      return;
    }
    if (api.setScale) api.setScale(player, 0.0f);
    if (current.stretch) {
      const std::string ratio =
        std::to_string(std::max(1, current.windowWidth)) + ":" +
        std::to_string(std::max(1, current.windowHeight));
      api.setAspectRatio(player, ratio.c_str());
    } else {
      api.setAspectRatio(player, nullptr);
    }
    activeStretch = current.stretch;
    activeWindowWidth = current.windowWidth;
    activeWindowHeight = current.windowHeight;
  }

  bool createPlayer(const Request& current)
  {
    destroyPlayer();
    if (!instance || current.path.empty() || !current.nativeWindow) return false;
    const std::string path = mediaPathForVlc(current.path);
    media = api.newMediaPath(instance, path.c_str());
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
    api.setNativeWindow(player, current.nativeWindow);
    if (api.setMouseInput) api.setMouseInput(player, 0);
    if (api.setKeyInput) api.setKeyInput(player, 0);
    api.setMute(player, 1);
    const double rate = std::max(0.05, std::min(8.0, current.playbackRate));
    api.setRate(player, static_cast<float>(rate));
    activePath = current.path;
    activePlaybackKey = current.playbackKey;
    activeWindow = current.nativeWindow;
    activeRate = rate;
    activeStretch = !current.stretch;
    activeWindowWidth = 0;
    activeWindowHeight = 0;
    applyAspect(current, true);
    statusCode.store(1, std::memory_order_release);
    if (api.play(player) != 0) {
      statusCode.store(-204, std::memory_order_release);
      destroyPlayer();
      return false;
    }
    api.setTime(player, secondsToMilliseconds(current.sourceTime));
    activePlaying = current.playing;
    const auto now = std::chrono::steady_clock::now();
    if (!current.playing) {
      // Um vout novo precisa chegar a Playing antes que Pause produza o quadro
      // do seek. A worker pausa assim que o relogio alcanca o alvo (ou no prazo
      // defensivo), sem bloquear a interface.
      initialStoppedFramePending = true;
      initialStoppedTarget = current.sourceTime;
      initialStoppedDeadline = now + std::chrono::milliseconds(450);
      activeStoppedTime = -1.0;
    }
    previousRequestValid = true;
    previousSourceTime = current.sourceTime;
    previousRequestAt = now;
    lastClockCorrectionAt = now;
    return true;
  }

  void finishInitialStoppedFrame(
    const Request& current,
    const std::chrono::steady_clock::time_point now)
  {
    if (!initialStoppedFramePending || !player || current.playing) return;
    const libvlc_time_t rawTime = api.getTime(player);
    const double playerTime = rawTime >= 0
      ? static_cast<double>(rawTime) / 1000.0 : -1.0;
    const int state = api.getState ? api.getState(player) : kVlcPlaying;
    const bool nearTarget = playerTime >= 0.0 &&
      std::abs(playerTime - initialStoppedTarget) <= 0.035;
    const bool ready = state == kVlcPlaying || state == kVlcPaused;
    if (!(nearTarget && ready) && now < initialStoppedDeadline) return;
    api.setTime(player, secondsToMilliseconds(initialStoppedTarget));
    api.setPause(player, 1);
    activeStoppedTime = initialStoppedTarget;
    initialStoppedFramePending = false;
    initialStoppedDeadline = {};
    statusCode.store(2, std::memory_order_release);
  }

  bool beginStoppedFrame(
    const Request& current,
    const std::chrono::steady_clock::time_point now)
  {
    if (!player) return false;
    if (api.play(player) != 0) return false;
    api.setTime(player, secondsToMilliseconds(current.sourceTime));
    api.setPause(player, 0);
    initialStoppedFramePending = true;
    initialStoppedTarget = current.sourceTime;
    initialStoppedDeadline = now + std::chrono::milliseconds(450);
    activeStoppedTime = -1.0;
    return true;
  }

  void applyRequest(const Request& current)
  {
    if (current.path.empty() || !current.nativeWindow) {
      destroyPlayer();
      statusCode.store(0, std::memory_order_release);
      return;
    }
    if (!player || current.path != activePath ||
        current.playbackKey != activePlaybackKey ||
        current.nativeWindow != activeWindow) {
      createPlayer(current);
      return;
    }

    const auto now = std::chrono::steady_clock::now();
    applyAspect(current, false);
    const double rate = std::max(0.05, std::min(8.0, current.playbackRate));
    if (std::abs(rate - activeRate) > 0.0001) {
      api.setRate(player, static_cast<float>(rate));
      activeRate = rate;
    }

    if (initialStoppedFramePending) {
      if (current.playing) {
        initialStoppedFramePending = false;
        initialStoppedDeadline = {};
        api.play(player);
        api.setPause(player, 0);
        activePlaying = true;
      } else if (std::abs(current.sourceTime - initialStoppedTarget) > 0.008) {
        initialStoppedTarget = current.sourceTime;
        initialStoppedDeadline = now + std::chrono::milliseconds(300);
        api.setTime(player, secondsToMilliseconds(current.sourceTime));
      }
      finishInitialStoppedFrame(current, now);
      previousRequestValid = true;
      previousSourceTime = current.sourceTime;
      previousRequestAt = now;
      return;
    }

    const bool wasPlaying = activePlaying;
    bool stoppedFrameStarted = false;
    if (current.playing != activePlaying) {
      if (current.playing) {
        api.play(player);
        api.setTime(player, secondsToMilliseconds(current.sourceTime));
        api.setPause(player, 0);
        activeStoppedTime = -1.0;
      } else {
        if (!beginStoppedFrame(current, now)) {
          createPlayer(current);
          return;
        }
        stoppedFrameStarted = true;
      }
      activePlaying = current.playing;
    }

    if (!current.playing) {
      if (!stoppedFrameStarted &&
          (activeStoppedTime < 0.0 ||
           std::abs(activeStoppedTime - current.sourceTime) > 0.008)) {
        if (!beginStoppedFrame(current, now)) {
          createPlayer(current);
          return;
        }
      }
    } else {
      bool jump = false;
      bool restart = false;
      if (previousRequestValid && wasPlaying) {
        const double elapsed = std::chrono::duration<double>(
          now - previousRequestAt).count();
        const double expected = elapsed * rate;
        const double actual = current.sourceTime - previousSourceTime;
        restart = actual < -0.01;
        jump = restart || std::abs(actual - expected) > 0.30;
      }
      const int state = api.getState ? api.getState(player) : kVlcPlaying;
      if (restart || state == kVlcEnded || state == kVlcStopped) {
        api.play(player);
        api.setTime(player, secondsToMilliseconds(current.sourceTime));
        api.setPause(player, 0);
      } else if (jump) {
        api.setTime(player, secondsToMilliseconds(current.sourceTime));
      } else if (now - lastClockCorrectionAt >=
                   std::chrono::milliseconds(80)) {
        const libvlc_time_t rawTime = api.getTime(player);
        if (rawTime >= 0) {
          const double playerTime = static_cast<double>(rawTime) / 1000.0;
          // O relogio do VLC e escravo do grid. Uma tolerancia de meio
          // segundo deixava a nossa janela visivelmente adiantada em relacao
          // a janela nativa do REAPER.
          if (std::abs(playerTime - current.sourceTime) > 0.075) {
            api.setTime(player, secondsToMilliseconds(current.sourceTime));
          }
        }
        lastClockCorrectionAt = now;
      }
    }

    previousRequestValid = true;
    previousSourceTime = current.sourceTime;
    previousRequestAt = now;
    statusCode.store(2, std::memory_order_release);
  }

  void workerLoop()
  {
    if (!initialize()) return;
    std::uint64_t handledSerial = 0;
    for (;;) {
      Request current;
      {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait_for(lock, std::chrono::milliseconds(10), [&]() {
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

  bool update(
    const std::string& path,
    const std::string& playbackKey,
    double sourceTime,
    bool playing,
    double playbackRate,
    void* nativeWindow,
    int windowWidth,
    int windowHeight,
    bool stretch)
  {
    if (path.empty() || playbackKey.empty() || !nativeWindow) return false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      request.path = path;
      request.playbackKey = playbackKey;
      request.sourceTime = std::max(0.0, sourceTime);
      request.playing = playing;
      request.playbackRate = playbackRate;
      request.nativeWindow = nativeWindow;
      request.windowWidth = std::max(1, windowWidth);
      request.windowHeight = std::max(1, windowHeight);
      request.stretch = stretch;
      ++request.serial;
      hasRequest = true;
    }
    changed.notify_one();
    return statusCode.load(std::memory_order_acquire) >= 0;
  }

  void reset()
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      request = Request{};
      ++request.serial;
      hasRequest = true;
    }
    changed.notify_one();
  }
};

NativeWindowPlayer::NativeWindowPlayer()
  : impl_(std::make_unique<Impl>()) {}

NativeWindowPlayer::~NativeWindowPlayer() = default;

bool NativeWindowPlayer::update(
  const std::string& utf8Path,
  const std::string& playbackKey,
  double sourceTime,
  bool playing,
  double playbackRate,
  void* nativeWindow,
  int windowWidth,
  int windowHeight,
  bool stretch)
{
  return impl_ && impl_->update(
    utf8Path, playbackKey, sourceTime, playing, playbackRate,
    nativeWindow, windowWidth, windowHeight, stretch);
}

void NativeWindowPlayer::reset()
{
  if (impl_) impl_->reset();
}

int NativeWindowPlayer::status() const
{
  return impl_
    ? impl_->statusCode.load(std::memory_order_acquire)
    : -299;
}

} // namespace vshook_video

#endif

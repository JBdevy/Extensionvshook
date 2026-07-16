#ifndef SWELL_PROVIDED_BY_APP
#define SWELL_PROVIDED_BY_APP
#endif
#include "reaper_plugin.h"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

#ifndef _WIN32
// Igual ao wrapper do SWS: em SWELL, CF_TEXT precisa usar o formato registrado.
#undef CF_TEXT
#define CF_TEXT RegisterClipboardFormat("SWELL__CF_TEXT")
#endif

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using plugin_getapi_t = void* (*)(const char*);
using ShowMessageBox_t = int (*)(const char*, const char*, int);
using GetMainHwnd_t = HWND (*)();
using GetResourcePath_t = const char* (*)();
using GetAppVersion_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using GetToggleCommandState_t = int (*)(int);
using GetToggleCommandStateEx_t = int (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);
using NamedCommandLookup_t = int (*)(const char*);
using ReverseNamedCommandLookup_t = const char* (*)(int);
using GetExtState_t = const char* (*)(const char*, const char*);
using SetExtState_t = void (*)(const char*, const char*, const char*, bool);
using EnumProjects_t = ReaProject* (*)(int, char*, int);
using GetProjExtState_t = int (*)(ReaProject*, const char*, const char*, char*, int);
using SetProjExtState_t = int (*)(ReaProject*, const char*, const char*, const char*);
using MarkProjectDirty_t = void (*)(ReaProject*);
using CountProjectMarkers_t = int (*)(ReaProject*, int*, int*);
using EnumProjectMarkers3_t = int (*)(ReaProject*, int, bool*, double*, double*, const char**, int*, int*);
using GetRegionOrMarker_t = void* (*)(ReaProject*, int, const char*);
using GetRegionOrMarkerInfo_Value_t = double (*)(ReaProject*, void*, const char*);
using GetProjectStateChangeCount_t = int (*)(ReaProject*);
using GetPlayStateEx_t = int (*)(ReaProject*);
using GetPlayPositionEx_t = double (*)(ReaProject*);
using GetCursorPositionEx_t = double (*)(ReaProject*);
using GetSet_LoopTimeRange2_t = void (*)(ReaProject*, bool, bool, double*, double*, bool);
using GetSetRepeatEx_t = int (*)(ReaProject*, int);
using DockIsChildOfDock_t = int (*)(HWND, bool*);
using DockWindowActivate_t = void (*)(HWND);
using DockWindowAdd_t = void (*)(HWND, const char*, int, bool);
using DockWindowRefreshForHWND_t = void (*)(HWND);
using DockWindowRemove_t = void (*)(HWND);

using CountTracks_t = int (*)(ReaProject*);
using GetTrack_t = MediaTrack* (*)(ReaProject*, int);
using GetMasterTrack_t = MediaTrack* (*)(ReaProject*);
using GetTrackName_t = bool (*)(MediaTrack*, char*, int);
using GetTrackNumMediaItems_t = int (*)(MediaTrack*);
using GetTrackMediaItem_t = MediaItem* (*)(MediaTrack*, int);
using GetMediaItemInfo_Value_t = double (*)(MediaItem*, const char*);
using SetMediaItemInfo_Value_t = bool (*)(MediaItem*, const char*, double);
using GetSetMediaItemInfo_String_t = bool (*)(MediaItem*, const char*, char*, bool);
using GetActiveTake_t = MediaItem_Take* (*)(MediaItem*);
using GetTakeName_t = const char* (*)(MediaItem_Take*);
using GetSetMediaItemTakeInfo_String_t = bool (*)(MediaItem_Take*, const char*, char*, bool);
using GetMediaItemTakeInfo_Value_t = double (*)(MediaItem_Take*, const char*);
using GetMediaItemTake_Source_t = PCM_source* (*)(MediaItem_Take*);
using GetMediaSourceFileName_t = void (*)(PCM_source*, char*, int);
using TakeFX_GetCount_t = int (*)(MediaItem_Take*);
using TakeFX_GetEnabled_t = bool (*)(MediaItem_Take*, int);
using TakeFX_SetEnabled_t = void (*)(MediaItem_Take*, int, bool);
using GetMediaTrackInfo_Value_t = double (*)(MediaTrack*, const char*);
using SetMediaTrackInfo_Value_t = bool (*)(MediaTrack*, const char*, double);
using Track_GetPeakInfo_t = double (*)(MediaTrack*, int);
using SelectProjectInstance_t = void (*)(ReaProject*);
using TrackList_AdjustWindows_t = void (*)(bool);
using UpdateArrange_t = void (*)();
using SetEditCurPos_t = void (*)(double, bool, bool);
using SetEditCurPos2_t = void (*)(ReaProject*, double, bool, bool);
using GetTrackGUID_t = GUID* (*)(MediaTrack*);
using guidToString_t = void (*)(const GUID*, char*);
using TrackFX_GetCount_t = int (*)(MediaTrack*);
using TrackFX_GetFXName_t = bool (*)(MediaTrack*, int, char*, int);
using TrackFX_GetNumParams_t = int (*)(MediaTrack*, int);
using TrackFX_GetParamName_t = bool (*)(MediaTrack*, int, int, char*, int);
using TrackFX_FormatParamValueNormalized_t = bool (*)(MediaTrack*, int, int, double, char*, int);
using TrackFX_GetParamNormalized_t = double (*)(MediaTrack*, int, int);
using TrackFX_SetParamNormalized_t = bool (*)(MediaTrack*, int, int, double);
using TrackFX_AddByName_t = int (*)(MediaTrack*, const char*, bool, int);
using TrackFX_CopyToTrack_t = void (*)(MediaTrack*, int, MediaTrack*, int, bool);

static plugin_register_t plugin_register_ptr = nullptr;
static plugin_getapi_t plugin_getapi_ptr = nullptr;
static ShowMessageBox_t ShowMessageBox_ptr = nullptr;
static GetMainHwnd_t GetMainHwnd_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static GetAppVersion_t GetAppVersion_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static GetToggleCommandState_t GetToggleCommandState_ptr = nullptr;
static GetToggleCommandStateEx_t GetToggleCommandStateEx_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;
static NamedCommandLookup_t NamedCommandLookup_ptr = nullptr;
static ReverseNamedCommandLookup_t ReverseNamedCommandLookup_ptr = nullptr;
static GetExtState_t GetExtState_ptr = nullptr;
static SetExtState_t SetExtState_ptr = nullptr;
static EnumProjects_t EnumProjects_ptr = nullptr;
static GetProjExtState_t GetProjExtState_ptr = nullptr;
static SetProjExtState_t SetProjExtState_ptr = nullptr;
static MarkProjectDirty_t MarkProjectDirty_ptr = nullptr;
static CountProjectMarkers_t CountProjectMarkers_ptr = nullptr;
static EnumProjectMarkers3_t EnumProjectMarkers3_ptr = nullptr;
static GetRegionOrMarker_t GetRegionOrMarker_ptr = nullptr;
static GetRegionOrMarkerInfo_Value_t GetRegionOrMarkerInfo_Value_ptr = nullptr;
static GetProjectStateChangeCount_t GetProjectStateChangeCount_ptr = nullptr;
static GetPlayStateEx_t GetPlayStateEx_ptr = nullptr;
static GetPlayPositionEx_t GetPlayPositionEx_ptr = nullptr;
static GetCursorPositionEx_t GetCursorPositionEx_ptr = nullptr;
static GetSet_LoopTimeRange2_t GetSet_LoopTimeRange2_ptr = nullptr;
static GetSetRepeatEx_t GetSetRepeatEx_ptr = nullptr;
static DockIsChildOfDock_t DockIsChildOfDock_ptr = nullptr;
static DockWindowActivate_t DockWindowActivate_ptr = nullptr;
static DockWindowAdd_t DockWindowAdd_ptr = nullptr;
static DockWindowRefreshForHWND_t DockWindowRefreshForHWND_ptr = nullptr;
static DockWindowRemove_t DockWindowRemove_ptr = nullptr;

static CountTracks_t CountTracks_ptr = nullptr;
static GetTrack_t GetTrack_ptr = nullptr;
static GetMasterTrack_t GetMasterTrack_ptr = nullptr;
static GetTrackName_t GetTrackName_ptr = nullptr;
static GetTrackNumMediaItems_t GetTrackNumMediaItems_ptr = nullptr;
static GetTrackMediaItem_t GetTrackMediaItem_ptr = nullptr;
static GetMediaItemInfo_Value_t GetMediaItemInfo_Value_ptr = nullptr;
static SetMediaItemInfo_Value_t SetMediaItemInfo_Value_ptr = nullptr;
static GetSetMediaItemInfo_String_t GetSetMediaItemInfo_String_ptr = nullptr;
static GetActiveTake_t GetActiveTake_ptr = nullptr;
static GetTakeName_t GetTakeName_ptr = nullptr;
static GetSetMediaItemTakeInfo_String_t GetSetMediaItemTakeInfo_String_ptr = nullptr;
static GetMediaItemTakeInfo_Value_t GetMediaItemTakeInfo_Value_ptr = nullptr;
static GetMediaItemTake_Source_t GetMediaItemTake_Source_ptr = nullptr;
static GetMediaSourceFileName_t GetMediaSourceFileName_ptr = nullptr;
static TakeFX_GetCount_t TakeFX_GetCount_ptr = nullptr;
static TakeFX_GetEnabled_t TakeFX_GetEnabled_ptr = nullptr;
static TakeFX_SetEnabled_t TakeFX_SetEnabled_ptr = nullptr;
static GetMediaTrackInfo_Value_t GetMediaTrackInfo_Value_ptr = nullptr;
static SetMediaTrackInfo_Value_t SetMediaTrackInfo_Value_ptr = nullptr;
static Track_GetPeakInfo_t Track_GetPeakInfo_ptr = nullptr;
static SelectProjectInstance_t SelectProjectInstance_ptr = nullptr;
static TrackList_AdjustWindows_t TrackList_AdjustWindows_ptr = nullptr;
static UpdateArrange_t UpdateArrange_ptr = nullptr;
static SetEditCurPos_t SetEditCurPos_ptr = nullptr;
static SetEditCurPos2_t SetEditCurPos2_ptr = nullptr;
static GetTrackGUID_t GetTrackGUID_ptr = nullptr;
static guidToString_t guidToString_ptr = nullptr;
static TrackFX_GetCount_t TrackFX_GetCount_ptr = nullptr;
static TrackFX_GetFXName_t TrackFX_GetFXName_ptr = nullptr;
static TrackFX_GetNumParams_t TrackFX_GetNumParams_ptr = nullptr;
static TrackFX_GetParamName_t TrackFX_GetParamName_ptr = nullptr;
static TrackFX_FormatParamValueNormalized_t TrackFX_FormatParamValueNormalized_ptr = nullptr;
static TrackFX_GetParamNormalized_t TrackFX_GetParamNormalized_ptr = nullptr;
static TrackFX_SetParamNormalized_t TrackFX_SetParamNormalized_ptr = nullptr;
static TrackFX_AddByName_t TrackFX_AddByName_ptr = nullptr;
static TrackFX_CopyToTrack_t TrackFX_CopyToTrack_ptr = nullptr;

static ReaProject* getCurrentProject(char* pathOut, int pathOutSize);

static const char* kExtStateSection = "VS_HOOK_LOADER";
static const char* kAutoOpenModeKey = "AUTO_OPEN_VSHOOK_MODE";
static const char* kLegacyAutoOpenKey = "AUTO_OPEN_VSHOOK";
static const char* kProjectAutoOpenModeKey = "PROJECT_AUTO_OPEN_VSHOOK_MODE";
static const char* kScriptControlSection = "VS_HOOK_SCRIPT_CONTROL";
static const char* kActiveScriptModeKey = "ACTIVE_MODE_V1";

static bool g_transportCommandBypass = false;
static const int kReaperTransportPlayStopCommandId = 40044;
static const int kReaperTransportStopCommandId = 1016;

// FIX101: qualquer comando manual de transporte deve cancelar a protecao de handoff da fila.
// Sem isso, se a fila manual acabou de entrar e o usuario aperta Stop, a extensao
// entende a parada como falha de transicao e religa o Play, reiniciando a musica.
static void nativeCancelQueueHandoffProtection();
static bool nativeIsLuaControlActive();
static bool nativeIsDirectorControlActive();
static bool nativeIsRuntimeControlActive();
static void nativeClearExplicitStopQueueState();
static bool nativePrepareStopSelectionFromQueueOrCurrent(ReaProject* project, bool moveCursorAfterStop);
static bool nativeStopTransportAndPrepareExplicitSelection(ReaProject* project, const std::string& explicitSelectionId, const std::string& explicitSelectionTab, double explicitStartPos, double explicitEndPos, bool moveCursorAfterStop);
static bool nativeStopTransportAndPrepareSelectionFromQueueOrCurrent(ReaProject* project, bool moveCursorAfterStop);

static bool handleTransportQueueStopCommand(int command)
{
  if (command != kReaperTransportPlayStopCommandId && command != kReaperTransportStopCommandId) return false;
  if (!nativeIsRuntimeControlActive()) return false;
  const bool luaOwnsQueueLogic = nativeIsLuaControlActive();

  if (!luaOwnsQueueLogic) nativeCancelQueueHandoffProtection();

  // Stop/PlayStop precisa consumir a fila ANTES do REAPER limpar o transporte.
  // Esta interceptacao nao e PlayProtection: ela apenas preserva a regra da fila,
  // deixando a musica aguardando selecionada e pronta para o proximo Play.
  if (!luaOwnsQueueLogic && !g_transportCommandBypass && Main_OnCommand_ptr) {
    char pathBuf[2048] = "";
    ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
    const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
    const bool isPlaying = ((playState & 1) == 1) || ((playState & 4) == 4);

    if (command == kReaperTransportStopCommandId || (command == kReaperTransportPlayStopCommandId && isPlaying)) {
      if (isPlaying) {
        g_transportCommandBypass = true;
        nativeStopTransportAndPrepareSelectionFromQueueOrCurrent(project, true);
        g_transportCommandBypass = false;
        return true;
      }

      // Parado nao tem fila de espera. Stop parado limpa apenas residuos nativos.
      nativeClearExplicitStopQueueState();
    }
  }

  return false;
}

struct ScriptEntry {
  custom_action_register_t action;
  const char* displayName;
  const char* fileName;
  const char* autoOpenMode;
  bool showInExtensionsMenu = false;
  std::string scriptPath;
  int scriptCommandId = 0;
  int commandId = 0;
};

struct AutoOpenEntry {
  custom_action_register_t action;
  const char* displayName;
  const char* autoOpenMode;
  int commandId = 0;
};

static ScriptEntry g_scripts[] = {
  {
    { 0, "VSHOOKRUNBETA", "VS Hook Beta", nullptr },
    "VS Hook Beta",
    "VS Hook Beta.lua",
    "beta",
    true
  },
  {
    { 0, "VSHOOKRUNESTABLE", "VS Hook Estable", nullptr },
    "VS Hook Estable",
    "VS Hook Estable.lua",
    "estable",
    true
  },
  {
    { 0, "VSHOOKAPPACTIVE", "APP ATIVO", nullptr },
    "APP ATIVO",
    "APP ATIVO.lua",
    "app_active",
    false
  }
};

struct State {
  bool initialized = false;
  bool commandHookRegistered = false;
  bool commandHook2Registered = false;
  bool toggleActionRegistered = false;
  bool menuHookRegistered = false;
  bool acceleratorRegistered = false;
  bool timerRegistered = false;
  bool apiRegistered = false;
  bool cfSetClipboardAliasRegistered = false;
  int startupTimerTicks = 0;
  int projectStableTicks = 0;
  bool didGlobalStartupAutoOpen = false;
  std::string activeProjectSignature;
  std::string autoOpenedProjectSignature;
  std::string lastLaunchedMode;
  std::string modeBeforeDirectorApp;
  std::string pendingScriptMode;
  int pendingScriptWaitTicks = 0;
  bool appActiveScreenOpen = false;
  bool pcAccessOverride = false;
} g_state;

// APP ATIVO: nunca executar Main_OnCommand/AddRemoveReaScript a partir da thread HTTP.
// O app apenas pede a troca; o timer da extensao aplica na thread principal do REAPER.
static std::atomic<bool> g_appActiveOpenRequested{false};
static std::atomic<bool> g_pcResumeRequested{false};
static std::chrono::steady_clock::time_point g_lastAppActiveTransition;
static REAPER_PLUGIN_HINSTANCE g_pluginInstance = nullptr;

struct NativeAppActivePanelModel {
  struct Row {
    std::string id;
    std::string name;
    std::string parentId;
    std::string blockColorHex;
    std::string inheritedColorHex;
    double start = 0.0;
    double end = 0.0;
    int order = 0;
    bool block = false;
    bool child = false;
  };

  bool playing = false;
  bool queueActive = false;
  bool loopActive = false;
  bool partArmed = false;
  bool regionsPage = false;
  bool mixerPage = false;
  bool autoplayEnabled = false;
  bool autoBlocoEnabled = false;
  bool autoStopEnabled = true;
  bool timerRunning = false;
  bool timerVisible = false;
  bool timerExpired = false;
  bool liveEnabled = false;
  bool numberRegionMode = false;
  bool drawerOutlineEnabled = true;
  bool drawerSymbolEnabled = true;
  std::string drawerOutlineColor = "yellow";
  std::string drawerSymbolColor = "yellow";
  int previewMode = 0;
  double progress = 0.0;
  double listScrollRatio = 0.0;
  uint64_t listScrollRevision = 0;
  double playingStart = 0.0;
  double playingEnd = 0.0;
  double queuedStart = 0.0;
  double queuedEnd = 0.0;
  double selectedStart = 0.0;
  double selectedEnd = 0.0;
  std::string playlistName = "Músicas";
  std::string activePage = "playlist";
  std::string playlistTotalText = "00:00:00";
  std::string timerMode = "progressive";
  std::string timerDisplayText = "00:00:00";
  std::string playingId;
  std::string queuedId;
  std::string selectedId;
  std::string playingName = "NENHUMA MUSICA EM REPRODUCAO";
  std::string queuedName = "FILA DE ESPERA VAZIA";
  std::string armedPartName;
  std::string loopPartName;
  std::vector<Row> rows;
};

static HWND g_nativeAppActivePanelHwnd = nullptr;
static bool g_nativeAppActivePanelClosing = false;
static bool g_nativeAppActiveResumeConfirmOpen = false;
static RECT g_nativeAppActiveResumeButtonRect{0, 0, 0, 0};
static RECT g_nativeAppActiveResumeConfirmYesRect{0, 0, 0, 0};
static RECT g_nativeAppActiveResumeConfirmNoRect{0, 0, 0, 0};
static NativeAppActivePanelModel g_nativeAppActivePanelModel;
static int g_nativeAppActiveListFirstRow = 0;
static int g_nativeAppActiveListScrollPixels = 0;
static std::string g_nativeAppActiveListFocusSignature;
static uint64_t g_nativeAppActiveLastAppliedScrollRevision = 0;
static bool g_nativeAppActiveEnterWasDown = false;

static bool resumePcAccessFromAppActiveScreen();
static bool nativeAppActivePanelIsOpen();
static bool nativeOpenAppActivePanel();
static void nativeCloseAppActivePanel();
static void nativeRefreshAppActivePanelModel();
static bool nativeGetLoopTimeRange(ReaProject* project, double& startOut, double& endOut);
static std::string nativeFindLoopBoundaryLabel(ReaProject* project, double boundaryPos);
static std::string nativeFindLoopRegionLabel(ReaProject* project, double loopStart, double loopEnd);

static AutoOpenEntry g_autoOpenEntries[] = {
  {
    { 0, "VSHOOKAUTOBETA", "Iniciar VS Hook Beta junto com o REAPER", nullptr },
    "Iniciar VS Hook Beta junto com o REAPER",
    "beta"
  },
  {
    { 0, "VSHOOKAUTOESTABLE", "Iniciar VS Hook Estable junto com o REAPER", nullptr },
    "Iniciar VS Hook Estable junto com o REAPER",
    "estable"
  }
};

static AutoOpenEntry g_projectAutoOpenEntries[] = {
  {
    { 0, "VSHOOKPROJECTAUTOBETA", "Iniciar VS Hook Beta junto com ESTE projeto", nullptr },
    "Iniciar VS Hook Beta junto com ESTE projeto",
    "beta"
  },
  {
    { 0, "VSHOOKPROJECTAUTOESTABLE", "Iniciar VS Hook Estable junto com ESTE projeto", nullptr },
    "Iniciar VS Hook Estable junto com ESTE projeto",
    "estable"
  }
};

#ifdef _WIN32
static std::wstring utf8ToWide(const std::string& text)
{
  if (text.empty()) return std::wstring();

  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                 text.c_str(), -1, nullptr, 0);

  // Fallback: if the incoming text is not valid UTF-8, use the current
  // Windows codepage instead of failing. This keeps the loader tolerant of
  // paths returned by older Windows/REAPER setups.
  UINT codepage = CP_UTF8;
  DWORD flags = MB_ERR_INVALID_CHARS;
  if (size <= 0) {
    codepage = CP_ACP;
    flags = 0;
    size = MultiByteToWideChar(codepage, flags, text.c_str(), -1, nullptr, 0);
  }

  if (size <= 0) return std::wstring();

  std::wstring wide(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(codepage, flags, text.c_str(), -1, wide.data(), size);

  if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
  return wide;
}

#endif

#ifndef _WIN32
static std::string wideToUtf8Fallback(const void* data)
{
  (void)data;
  return std::string();
}
#endif

static void VS_Hook_SetClipboard(const char* text)
{
  const char* buf = text ? text : "";

#ifdef _WIN32
  // Mesmo caminho do SWS CF_SetClipboard: entrada UTF-8 -> CF_UNICODETEXT.
  const int length = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
  if (length <= 0) return;
  const size_t size = static_cast<size_t>(length) * sizeof(wchar_t);
#else
  // No SWELL/macOS o SWS usa CF_TEXT redefinido como SWELL__CF_TEXT.
  const size_t size = std::strlen(buf) + 1;
#endif

  HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, size);
  if (!mem) return;

#ifdef _WIN32
  MultiByteToWideChar(CP_UTF8, 0, buf, -1,
    static_cast<wchar_t*>(GlobalLock(mem)), length);
#else
  std::memcpy(GlobalLock(mem), buf, size);
#endif
  GlobalUnlock(mem);

  OpenClipboard(GetMainHwnd_ptr ? GetMainHwnd_ptr() : nullptr);
  EmptyClipboard();
#ifdef _WIN32
  SetClipboardData(CF_UNICODETEXT, mem);
#else
  SetClipboardData(CF_TEXT, mem);
#endif
  CloseClipboard();
}

// REAPER só expõe corretamente para Lua quando a API também registra APIvararg_*.
// Foi isso que faltava: o SWS registra API_CF_SetClipboard + APIvararg_CF_SetClipboard.
static void* VS_Hook_SetClipboard_vararg(void** arglist, int numparms)
{
  const char* value = "";
  if (arglist && numparms > 0 && arglist[0]) value = static_cast<const char*>(arglist[0]);
  VS_Hook_SetClipboard(value);
  return nullptr;
}

static bool VS_Hook_GetClipboard(char* buf, int bufSize)
{
  if (!buf || bufSize <= 0) return false;
  buf[0] = '\0';

  if (!OpenClipboard(nullptr)) return false;

#ifdef _WIN32
  HANDLE mem = GetClipboardData(CF_UNICODETEXT);
  if (!mem) {
    CloseClipboard();
    return false;
  }

  const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(mem));
  if (!data) {
    CloseClipboard();
    return false;
  }

  const int needed = WideCharToMultiByte(CP_UTF8, 0, data, -1, nullptr, 0, nullptr, nullptr);
  if (needed > 0) {
    WideCharToMultiByte(CP_UTF8, 0, data, -1, buf, bufSize, nullptr, nullptr);
    buf[bufSize - 1] = '\0';
  }

  GlobalUnlock(mem);
  CloseClipboard();
  return needed > 0;
#else
  HANDLE mem = GetClipboardData(CF_TEXT);
  if (!mem) {
    CloseClipboard();
    return false;
  }

  const char* data = static_cast<const char*>(GlobalLock(mem));
  if (!data) {
    CloseClipboard();
    return false;
  }

  std::strncpy(buf, data, static_cast<size_t>(bufSize - 1));
  buf[bufSize - 1] = '\0';
  GlobalUnlock(mem);
  CloseClipboard();
  return true;
#endif
}

static char g_apiDefSetClipboard[] =
  "void\0const char*\0text\0Write text into the system clipboard using the VS Hook extension.";

static char g_apiDefGetClipboard[] =
  "bool\0char*,int\0textOutNeedBig,textOutNeedBig_sz\0Read text from the system clipboard using the VS Hook extension.";

static char g_apiDefCFSetClipboard[] =
  "void\0const char*\0str\0Write the given string into the system clipboard.";

static bool registerClipboardApi()
{
  if (!plugin_register_ptr) return false;

  bool ok = true;
  ok = (plugin_register_ptr("API_VS_Hook_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard)) != 0) && ok;
  ok = (plugin_register_ptr("APIvararg_VS_Hook_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard_vararg)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_SetClipboard", reinterpret_cast<void*>(g_apiDefSetClipboard)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_GetClipboard", reinterpret_cast<void*>(&VS_Hook_GetClipboard)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_GetClipboard", reinterpret_cast<void*>(g_apiDefGetClipboard)) != 0) && ok;

  // Alias compatível com SWS. Se o SWS já existir, não derruba o registro principal do VS Hook.
  // Isso permite que Lua antigo que chama reaper.CF_SetClipboard use a nossa implementação.
  bool cfAliasOk = false;
  if (!plugin_getapi_ptr || plugin_getapi_ptr("CF_SetClipboard") == nullptr) {
    cfAliasOk = (plugin_register_ptr("API_CF_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard)) != 0);
    if (cfAliasOk) {
      plugin_register_ptr("APIvararg_CF_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard_vararg));
      plugin_register_ptr("APIdef_CF_SetClipboard", reinterpret_cast<void*>(g_apiDefCFSetClipboard));
    }
  }
  g_state.cfSetClipboardAliasRegistered = cfAliasOk;

  g_state.apiRegistered = ok;
  return ok;
}

static void unregisterClipboardApi()
{
  if (!plugin_register_ptr || !g_state.apiRegistered) return;

  if (g_state.cfSetClipboardAliasRegistered) {
    plugin_register_ptr("-APIdef_CF_SetClipboard", reinterpret_cast<void*>(g_apiDefCFSetClipboard));
    plugin_register_ptr("-APIvararg_CF_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard_vararg));
    plugin_register_ptr("-API_CF_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard));
    g_state.cfSetClipboardAliasRegistered = false;
  }
  plugin_register_ptr("-APIdef_VS_Hook_GetClipboard", reinterpret_cast<void*>(g_apiDefGetClipboard));
  plugin_register_ptr("-API_VS_Hook_GetClipboard", reinterpret_cast<void*>(&VS_Hook_GetClipboard));
  plugin_register_ptr("-APIdef_VS_Hook_SetClipboard", reinterpret_cast<void*>(g_apiDefSetClipboard));
  plugin_register_ptr("-APIvararg_VS_Hook_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard_vararg));
  plugin_register_ptr("-API_VS_Hook_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard));
  g_state.apiRegistered = false;
}

static std::string joinPath(const std::string& base, const std::string& child)
{
  if (base.empty()) return child;
  const char last = base.back();
  if (last == '/' || last == '\\') return base + child;
  return base + "/" + child;
}

static void showDiagnostic(const std::string& text)
{
  if (ShowMessageBox_ptr) {
    ShowMessageBox_ptr(text.c_str(), "VS Hook Loader", 0);
  }
}

// As ruler lanes usadas pelo VS Hook existem a partir do REAPER 7.62.
// O produto recomenda 7.65+ para manter o mesmo comportamento em todas as telas.
static bool nativeReaperVersionIsSupported()
{
  const char* raw = GetAppVersion_ptr ? GetAppVersion_ptr() : nullptr;
  const std::string version = raw ? raw : "";
  if (version.empty()) return false;

  size_t pos = 0;
  while (pos < version.size() && !std::isdigit(static_cast<unsigned char>(version[pos]))) ++pos;
  if (pos >= version.size()) return false;

  const int major = std::atoi(version.c_str() + pos);
  while (pos < version.size() && std::isdigit(static_cast<unsigned char>(version[pos]))) ++pos;
  int minor = 0;
  if (pos < version.size() && version[pos] == '.') {
    ++pos;
    minor = std::atoi(version.c_str() + pos);
  }
  return major > 7 || (major == 7 && minor >= 65);
}

static bool nativeEnsureSupportedReaperVersion()
{
  if (nativeReaperVersionIsSupported()) return true;
  const char* raw = GetAppVersion_ptr ? GetAppVersion_ptr() : nullptr;
  const std::string installed = (raw && *raw) ? raw : "versao nao identificada";
  showDiagnostic(
    "O VS Hook requer o REAPER 7.65 ou superior.\n\n"
    "Versao instalada: " + installed + "\n\n"
    "Atualize o REAPER para abrir o Hook."
  );
  // A extensao permanece carregada; apenas a abertura dos Luas e recusada.
  return false;
}

static bool fileExists(const std::string& path)
{
#ifdef _WIN32
  const std::wstring widePath = utf8ToWide(path);
  if (widePath.empty()) return false;

  const DWORD attrs = GetFileAttributesW(widePath.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static std::string normalizeSlashes(std::string path)
{
  for (char& c : path) {
    if (c == '\\') c = '/';
  }
  return path;
}

static std::vector<std::string> buildScriptCandidates(const char* fileName)
{
  std::vector<std::string> candidates;

#ifdef _WIN32
  candidates.push_back(std::string("C:/Users/Public/VS Hook APP/") + fileName);

  char* publicDir = nullptr;
  size_t publicLen = 0;
  if (_dupenv_s(&publicDir, &publicLen, "PUBLIC") == 0 && publicDir && publicDir[0]) {
    candidates.push_back(joinPath(normalizeSlashes(publicDir), std::string("VS Hook APP/") + fileName));
  }
  if (publicDir) free(publicDir);
#else
  const char* resource = GetResourcePath_ptr ? GetResourcePath_ptr() : "";
  const std::string base = normalizeSlashes(resource ? resource : "");
  candidates.push_back(base + "/Scripts/VS Hook APP/" + fileName);
  candidates.push_back(base + "/Scripts/" + fileName);
  candidates.push_back(base + "/VS Hook APP/" + fileName);
#endif

  return candidates;
}

static bool resolveScriptPath(ScriptEntry& script)
{
  const auto candidates = buildScriptCandidates(script.fileName);

  for (const auto& candidate : candidates) {
    if (fileExists(candidate)) {
      script.scriptPath = candidate;
      return true;
    }
  }

  script.scriptPath.clear();
  return false;
}

static std::string normalizeActionPathForMatch(std::string value)
{
  std::replace(value.begin(), value.end(), '\\', '/');
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return value;
}

static std::string readTextFile(const std::string& path)
{
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file) return std::string();
  std::ostringstream oss;
  oss << file.rdbuf();
  return oss.str();
}

static int lookupNamedCommandToken(const std::string& token)
{
  if (!NamedCommandLookup_ptr || token.empty()) return 0;
  const std::string withUnderscore = token[0] == '_' ? token : ("_" + token);
  int commandId = NamedCommandLookup_ptr(withUnderscore.c_str());
  if (commandId == 0 && token[0] != '_') commandId = NamedCommandLookup_ptr(token.c_str());
  return commandId;
}

static std::string extractReaScriptTokenFromLine(const std::string& line)
{
  size_t pos = line.find("RS");
  while (pos != std::string::npos) {
    const bool startsToken = pos == 0 || !(std::isalnum(static_cast<unsigned char>(line[pos - 1])) || line[pos - 1] == '_');
    if (startsToken) {
      size_t end = pos + 2;
      while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_')) ++end;
      if (end > pos + 2) return line.substr(pos, end - pos);
    }
    pos = line.find("RS", pos + 2);
  }
  return std::string();
}

static int findExistingReaScriptCommandInKeyboardIni(const ScriptEntry& script)
{
  if (!NamedCommandLookup_ptr || !GetResourcePath_ptr) return 0;
  const char* resourcePath = GetResourcePath_ptr();
  if (!resourcePath || !*resourcePath) return 0;

  const std::string kbPath = joinPath(resourcePath, "reaper-kb.ini");
  const std::string text = readTextFile(kbPath);
  if (text.empty()) return 0;

  const std::string wantedFile = normalizeActionPathForMatch(script.fileName ? script.fileName : "");
  const std::string wantedPath = normalizeActionPathForMatch(script.scriptPath);
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const std::string normalizedLine = normalizeActionPathForMatch(line);
    const bool matchesFile = !wantedFile.empty() && normalizedLine.find(wantedFile) != std::string::npos;
    const bool matchesPath = !wantedPath.empty() && normalizedLine.find(wantedPath) != std::string::npos;
    if (!matchesFile && !matchesPath) continue;

    const std::string token = extractReaScriptTokenFromLine(line);
    const int commandId = lookupNamedCommandToken(token);
    if (commandId != 0) return commandId;
  }

  return 0;
}

static void rememberScriptNamedCommand(const ScriptEntry& script)
{
  if (!SetExtState_ptr || !ReverseNamedCommandLookup_ptr || script.scriptCommandId == 0 || !script.fileName) return;
  const char* named = ReverseNamedCommandLookup_ptr(script.scriptCommandId);
  if (!named || !*named) return;
  SetExtState_ptr("VSHookLoader", (std::string("ReaScriptNamedCommand.") + script.fileName).c_str(), named, true);
}

static int findRememberedScriptCommand(const ScriptEntry& script)
{
  if (!GetExtState_ptr || !NamedCommandLookup_ptr || !script.fileName) return 0;
  const char* named = GetExtState_ptr("VSHookLoader", (std::string("ReaScriptNamedCommand.") + script.fileName).c_str());
  if (!named || !*named) return 0;
  return lookupNamedCommandToken(named);
}

static int findExistingReaScriptCommand(ScriptEntry& script)
{
  int commandId = findRememberedScriptCommand(script);
  if (commandId != 0) return commandId;
  if (script.scriptPath.empty()) resolveScriptPath(script);
  commandId = findExistingReaScriptCommandInKeyboardIni(script);
  if (commandId != 0) return commandId;
  return 0;
}

static bool ensureScriptRegistered(ScriptEntry& script)
{
  if (script.scriptCommandId != 0) {
    return true;
  }

  const int existingCommandId = findExistingReaScriptCommand(script);
  if (existingCommandId != 0) {
    script.scriptCommandId = existingCommandId;
    rememberScriptNamedCommand(script);
    return true;
  }

  if (!AddRemoveReaScript_ptr) {
    showDiagnostic("REAPER nao entregou a API AddRemoveReaScript para o VS Hook Loader.");
    return false;
  }

  if (!resolveScriptPath(script)) {
    std::ostringstream oss;
    oss << "Nao encontrei o script: " << script.fileName << "\n\nCaminhos verificados:";
    for (const auto& candidate : buildScriptCandidates(script.fileName)) {
      oss << "\n- " << candidate;
    }
    showDiagnostic(oss.str());
    return false;
  }

  // Nao use commit=true aqui: regravar reaper-kb.ini ao "garantir" registro
  // pode apagar atalhos do usuario para ReaScripts ja existentes.
  const int commandId = AddRemoveReaScript_ptr(true, 0, script.scriptPath.c_str(), false);
  if (commandId == 0) {
    showDiagnostic(std::string("REAPER nao conseguiu registrar o script:\n") + script.scriptPath);
    return false;
  }

  script.scriptCommandId = commandId;
  rememberScriptNamedCommand(script);
  return true;
}


static void registerScriptInActionListIfPresent(ScriptEntry& script)
{
  if (script.scriptCommandId != 0) return;

  const int existingCommandId = findExistingReaScriptCommand(script);
  if (existingCommandId != 0) {
    script.scriptCommandId = existingCommandId;
    rememberScriptNamedCommand(script);
    return;
  }

  if (!AddRemoveReaScript_ptr) return;
  if (!resolveScriptPath(script)) return;

  const int commandId = AddRemoveReaScript_ptr(true, 0, script.scriptPath.c_str(), false);
  if (commandId != 0) {
    script.scriptCommandId = commandId;
    rememberScriptNamedCommand(script);
  }
}

static void unregisterLegacyAppActiveLuaScript()
{
  if (!AddRemoveReaScript_ptr) return;
  for (ScriptEntry& script : g_scripts) {
    const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
    if (mode != "app_active") continue;
    const std::vector<std::string> candidates = buildScriptCandidates(script.fileName);
    for (size_t i = 0; i < candidates.size(); ++i) {
      // Remove inclusive registros deixados por versoes antigas. A janela atual
      // e 100% nativa e nao executa mais APP ATIVO.lua.
      AddRemoveReaScript_ptr(false, 0, candidates[i].c_str(), true);
    }
    script.scriptCommandId = 0;
    script.scriptPath.clear();
    if (SetExtState_ptr && script.fileName) {
      SetExtState_ptr("VSHookLoader",
        (std::string("ReaScriptNamedCommand.") + script.fileName).c_str(), "", true);
    }
  }
}

static int getScriptToggleState(const ScriptEntry& script)
{
  if (script.scriptCommandId == 0) return 0;

  if (GetToggleCommandStateEx_ptr) {
    const int state = GetToggleCommandStateEx_ptr(0, script.scriptCommandId);
    if (state >= 0) return state;
  }

  if (GetToggleCommandState_ptr) {
    const int state = GetToggleCommandState_ptr(script.scriptCommandId);
    if (state >= 0) return state;
  }

  return 0;
}

static bool nativeWindowTreeContainsTitle(HWND parent, const char* needle, int depth)
{
  if (!needle || !*needle || depth < 0) return false;

  HWND child = nullptr;
  while ((child = FindWindowEx(parent, child, nullptr, nullptr)) != nullptr) {
    char title[512] = "";
    const int length = GetWindowText(child, title, static_cast<int>(sizeof(title)));
    if (length > 0 && std::strstr(title, needle) != nullptr) {
      return true;
    }
    if (depth > 0 && nativeWindowTreeContainsTitle(child, needle, depth - 1)) {
      return true;
    }
  }
  return false;
}

static bool isScriptWindowOpen(const ScriptEntry& script)
{
  const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
  if (mode == "app_active") return nativeAppActivePanelIsOpen();
  const char* needle = nullptr;
  if (mode == "beta") needle = "VS Hook Beta";
  else if (mode == "estable") needle = "VS Hook Est";
  if (!needle) return false;

  // Procura tanto janelas soltas quanto filhos do docker do REAPER/SWELL.
  return nativeWindowTreeContainsTitle(nullptr, needle, 8);
}

static bool isVsHookModeText(const std::string& mode)
{
  return mode == "beta" || mode == "estable";
}

static void rememberActiveScriptMode(const std::string& mode)
{
  g_state.lastLaunchedMode = mode;
  if (SetExtState_ptr) {
    SetExtState_ptr(kScriptControlSection, kActiveScriptModeKey, mode.c_str(), false);
  }
}

static std::string readRememberedActiveScriptMode()
{
  if (!g_state.lastLaunchedMode.empty()) return g_state.lastLaunchedMode;
  if (!GetExtState_ptr) return std::string();

  const char* raw = GetExtState_ptr(kScriptControlSection, kActiveScriptModeKey);
  const std::string mode = raw ? raw : "";
  if (mode == "beta" || mode == "estable" || mode == "app_active") {
    g_state.lastLaunchedMode = mode;
    return mode;
  }
  return std::string();
}

static std::string getExclusiveActiveScriptMode()
{
  if (!g_state.pendingScriptMode.empty()) return g_state.pendingScriptMode;

  std::vector<std::string> openModes;
  for (ScriptEntry& script : g_scripts) {
    if (!script.autoOpenMode) continue;
    if (isScriptWindowOpen(script)) openModes.emplace_back(script.autoOpenMode);
  }

  if (openModes.size() == 1) {
    rememberActiveScriptMode(openModes.front());
    return openModes.front();
  }

  const std::string remembered = readRememberedActiveScriptMode();
  if (!remembered.empty()) {
    for (const std::string& mode : openModes) {
      if (mode == remembered) return remembered;
    }
    // A janela pode estar dockada/oculta e nao ter sido localizada pelo SWELL.
    if (openModes.empty()) return remembered;
  }

  // Se uma instalacao antiga deixou Beta e Estable marcados ao mesmo tempo,
  // nunca devolve dois estados. Na ausencia de memoria, prioriza Beta apenas
  // para permitir que o proximo clique faça a troca exclusiva corretamente.
  if (openModes.size() > 1) {
    for (const std::string& mode : openModes) {
      if (mode == "beta") return mode;
    }
    return openModes.front();
  }

  return std::string();
}


// FIX 20260711: ESC, R, L e Enter continuam chegando ao Lua mesmo quando
// o usuario clicou em uma track ou em outra area do REAPER.
static std::atomic<unsigned long long> g_globalHotkeySequence{0};
static std::atomic<bool> g_globalStopBreakRequested{false};
static std::atomic<bool> g_globalPauseRequested{false};
static std::atomic<bool> g_globalStopPauseRequested{false};
static std::atomic<bool> g_stopPauseModeEnabled{false};
// -1 = nenhum pedido, 0 = desligar, 1 = ligar, 2 = alternar.
static std::atomic<int> g_nativeMultiLoopBypassRequest{-1};
static int g_globalHotkeyLastVirtualKey = 0;
static std::chrono::steady_clock::time_point g_globalHotkeyLastAt;

static bool nativeWindowOrAncestorTitleContains(HWND hwnd, const char* needle)
{
  if (!needle || !*needle) return false;
  HWND current = hwnd;
  for (int depth = 0; current && depth < 12; ++depth) {
    char title[512] = "";
    const int length = GetWindowText(current, title, static_cast<int>(sizeof(title)));
    if (length > 0 && std::strstr(title, needle) != nullptr) return true;
    current = GetParent(current);
  }
  return false;
}

static bool nativeMessageTargetsVsHookWindow(const MSG* msg)
{
  HWND target = msg ? msg->hwnd : nullptr;
  if (!target) target = GetFocus();
  return nativeWindowOrAncestorTitleContains(target, "VS Hook Beta") ||
         nativeWindowOrAncestorTitleContains(target, "VS Hook Est") ||
         nativeWindowOrAncestorTitleContains(target, "APP ATIVO");
}

// Nao rouba teclas enquanto o usuario estiver escrevendo em qualquer campo do REAPER.
static bool nativeMessageTargetsTextInput(const MSG* msg)
{
  HWND current = msg ? msg->hwnd : nullptr;
  if (!current) current = GetFocus();

  for (int depth = 0; current && depth < 12; ++depth) {
    char className[256] = "";
    const int length = GetClassName(current, className, static_cast<int>(sizeof(className)));
    std::string kind = length > 0 ? className : "";
    std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (kind.find("edit") != std::string::npos ||
        kind.find("textbox") != std::string::npos ||
        kind.find("combobox") != std::string::npos ||
        kind.find("textinput") != std::string::npos) {
      return true;
    }
#ifdef _WIN32
    const LRESULT dialogCode = SendMessage(current, WM_GETDLGCODE, 0, 0);
    if ((dialogCode & DLGC_WANTCHARS) != 0) return true;
#endif
    current = GetParent(current);
  }
  return false;
}

static bool nativeAnyVsHookScriptIsOpen()
{
  for (ScriptEntry& script : g_scripts) {
    if (isScriptWindowOpen(script) || getScriptToggleState(script) > 0) return true;
  }
  return false;
}

static bool nativeHotkeyHasCommandModifier()
{
  const bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  const bool system = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
  return control || alt || system;
}

static bool nativeStopBreakModifierDown()
{
  // No SWELL/macOS, VK_CONTROL representa a tecla Command. No Windows,
  // representa Ctrl. Assim a mesma leitura cobre Ctrl+Espaco e Command+Espaco.
  return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

static bool nativePauseModifierDown()
{
  // VK_MENU representa Alt no Windows e Option no SWELL/macOS.
  return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}

static void nativePublishForwardedGlobalHotkey(const char* key)
{
  if (!SetExtState_ptr || !key || !*key) return;
  const unsigned long long seq = ++g_globalHotkeySequence;
  const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  const std::string seqText = std::to_string(seq);
  const std::string atText = std::to_string(static_cast<long long>(nowMs));
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "GLOBAL_HOTKEY_KEY_V1", key, false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "GLOBAL_HOTKEY_SEQ_V1", seqText.c_str(), false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "GLOBAL_HOTKEY_AT_MS_V1", atText.c_str(), false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "GLOBAL_HOTKEY_OUT_OF_FOCUS_V1", "1", false);
}

static int nativeGlobalHotkeyTranslate(MSG* msg, accelerator_register_t* ctx)
{
  (void)ctx;
  if (!msg) return 0;

  const bool keyDown = msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN;
  const int vk = static_cast<int>(msg->wParam);
  // No painel nativo, o Enter nao depende do foco nem do encaminhamento do
  // docker. Consome a tecla aqui e chama diretamente o mesmo WndProc usado pela
  // janela. O latch impede que o autorepeat abra e confirme o modal no mesmo
  // toque; e liberado no key-up ou pela leitura fisica feita no timer.
  if (vk == VK_RETURN && nativeAppActivePanelIsOpen()) {
    if (!keyDown) {
      if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) {
        g_nativeAppActiveEnterWasDown = false;
        return 1;
      }
      return 0;
    }
    if (!g_nativeAppActiveEnterWasDown) {
      g_nativeAppActiveEnterWasDown = true;
      SendMessage(g_nativeAppActivePanelHwnd, WM_KEYDOWN, VK_RETURN, 0);
    }
    return 1;
  }
  if (!keyDown) return 0;
  if (nativeMessageTargetsTextInput(msg)) return 0;
  // R sozinho pertence ao VS Hook. Shift+R fica inteiramente livre para o
  // mapa de atalhos do REAPER, inclusive quando a janela do Lua está em foco.
  const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  if ((vk == 'R' || vk == 'r') && shiftDown) return 0;
  const bool stopBreakShortcut = vk == VK_SPACE && nativeStopBreakModifierDown();
  const bool pauseShortcut = vk == VK_SPACE && !stopBreakShortcut && nativePauseModifierDown();
  const bool stopPauseModeShortcut = vk == VK_SPACE && !stopBreakShortcut && !pauseShortcut && g_stopPauseModeEnabled.load();
  if (!stopBreakShortcut && !pauseShortcut && nativeHotkeyHasCommandModifier()) return 0;
  const char* key = nullptr;
  if (vk == VK_ESCAPE) key = "escape";
  else if (vk == VK_RETURN) key = "enter";
  else if (vk == VK_SPACE) key = "space";
  else if (vk == 'R' || vk == 'r') key = "r";
  else if (vk == 'L' || vk == 'l') key = "l";
  else if (vk == 'B' || vk == 'b') key = "b";
  else return 0;

  // A validade do atalho precisa pertencer ao mesmo dono do modo lembrado.
  // Antes, um heartbeat do Diretor ainda ativo podia ser combinado com o ultimo
  // modo Lua salvo (Beta/Estable) mesmo depois do Lua fechar. Nesse estado o
  // Espaco continuava sendo consumido e o transporte normal do REAPER parava de
  // responder. Nao basta existir "algum" runtime: cada modo autoriza apenas o
  // heartbeat correspondente.
  const bool luaControlActive = nativeIsLuaControlActive();
  const bool directorControlActive = nativeIsDirectorControlActive();
  if (!luaControlActive && !directorControlActive) return 0;

  const std::string activeMode = getExclusiveActiveScriptMode();
  if (activeMode != "beta" && activeMode != "estable" && activeMode != "app_active") return 0;
  if (isVsHookModeText(activeMode) && !luaControlActive) return 0;
  if (activeMode == "app_active" &&
      (!directorControlActive || !nativeAppActivePanelIsOpen())) return 0;

  // Stop/Pause pertence sempre a extensao, inclusive com foco no GFX do Lua.
  // Os demais atalhos continuam no Lua quando a propria janela esta em foco.
  const bool extensionOwnsFocusedShortcut = stopPauseModeShortcut ||
    ((stopBreakShortcut || pauseShortcut) && activeMode == "app_active");
  if (nativeMessageTargetsVsHookWindow(msg) && !extensionOwnsFocusedShortcut) return -1;

  const auto now = std::chrono::steady_clock::now();
  if (g_globalHotkeyLastAt.time_since_epoch().count() != 0 &&
      g_globalHotkeyLastVirtualKey == vk &&
      std::chrono::duration_cast<std::chrono::milliseconds>(now - g_globalHotkeyLastAt).count() < 180) {
    // ESC nunca deve bloquear o comportamento nativo do REAPER. O primeiro
    // toque ainda e encaminhado ao Lua; repeticoes apenas seguem para o host.
    return vk == VK_ESCAPE ? 0 : 1;
  }
  g_globalHotkeyLastVirtualKey = vk;
  g_globalHotkeyLastAt = now;

  if (stopBreakShortcut) {
    g_globalStopBreakRequested.store(true);
    return 1;
  }

  if (pauseShortcut) {
    g_globalPauseRequested.store(true);
    return 1;
  }

  if (stopPauseModeShortcut) {
    g_globalStopPauseRequested.store(true);
    return 1;
  }

  // O bypass de Multiloops pertence a extensao. Quando o Beta/Estavel nao
  // esta com o foco no proprio GFX, B nao volta mais para o Lua.
  if (vk == 'B' || vk == 'b') {
    g_nativeMultiLoopBypassRequest.store(2);
    return 1;
  }

  // APP ATIVO: Enter fora do foco tambem retoma o acesso no PC.
  if (activeMode == "app_active") {
    if (vk == VK_RETURN) {
      if (nativeAppActivePanelIsOpen()) PostMessage(g_nativeAppActivePanelHwnd, WM_KEYDOWN, VK_RETURN, 0);
      return 1;
    }
    return 0;
  }

  nativePublishForwardedGlobalHotkey(key);
  // O Lua recebe o ESC para cancelar fila/Parts/loop, mas a tecla continua
  // chegando ao REAPER para limpar uma selecao comum da timeline.
  return vk == VK_ESCAPE ? 0 : 1;
}

static accelerator_register_t g_vshookGlobalHotkeyAccelerator = {
  &nativeGlobalHotkeyTranslate,
  true,
  nullptr
};

static bool isScriptRunning(ScriptEntry& script)
{
  const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
  if (mode != "app_active" && !ensureScriptRegistered(script)) return false;
  const std::string activeMode = getExclusiveActiveScriptMode();
  return script.autoOpenMode && activeMode == script.autoOpenMode;
}

static void requestScriptInstanceClose(const ScriptEntry& script)
{
  if (!SetExtState_ptr || !script.autoOpenMode) return;
  const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  const std::string token = std::string("close_") + script.autoOpenMode + "_" + std::to_string(static_cast<long long>(nowMs));
  SetExtState_ptr(kScriptControlSection, "CLOSE_MODE_V1", script.autoOpenMode, false);
  SetExtState_ptr(kScriptControlSection, "CLOSE_TOKEN_V1", token.c_str(), false);
}

static void terminateKnownScript(ScriptEntry& script)
{
  const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
  if (mode == "app_active") {
    nativeCloseAppActivePanel();
    g_state.appActiveScreenOpen = false;
    return;
  }
  if (!ensureScriptRegistered(script)) return;
  requestScriptInstanceClose(script);
  if (Main_OnCommand_ptr && script.scriptCommandId != 0) {
    // Beta, Estable e APP ATIVO usam set_action_options(5): relancar a action
    // encerra a instancia atual sem abrir outra.
    Main_OnCommand_ptr(script.scriptCommandId, 0);
  }
}

static bool closeOtherOpenScripts(ScriptEntry& target)
{
  bool requestedClose = false;
  const std::string targetMode = target.autoOpenMode ? target.autoOpenMode : "";
  const std::string remembered = readRememberedActiveScriptMode();

  for (ScriptEntry& other : g_scripts) {
    if (&other == &target || !other.autoOpenMode) continue;

    const std::string otherMode = other.autoOpenMode;
    const bool visiblyOpen = isScriptWindowOpen(other);
    const bool rememberedOpen = !remembered.empty() && remembered == otherMode;
    const bool appKnownOpen = otherMode == "app_active" && g_state.appActiveScreenOpen;

    if (visiblyOpen || rememberedOpen || appKnownOpen) {
      terminateKnownScript(other);
      requestedClose = true;
    }
  }

  if (requestedClose) {
    g_state.pendingScriptMode = targetMode;
    g_state.pendingScriptWaitTicks = 0;
    rememberActiveScriptMode(targetMode);
  }
  return requestedClose;
}

static void launchScriptNow(ScriptEntry& script)
{
  const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
  if (mode == "app_active") {
    if (!nativeAppActivePanelIsOpen() && !nativeOpenAppActivePanel()) return;
    rememberActiveScriptMode(mode);
    g_state.appActiveScreenOpen = true;
    return;
  }
  if (!ensureScriptRegistered(script)) return;
  if (!Main_OnCommand_ptr || script.scriptCommandId == 0) return;

  if (!isScriptWindowOpen(script)) {
    Main_OnCommand_ptr(script.scriptCommandId, 0);
  }

  rememberActiveScriptMode(mode);
  g_state.appActiveScreenOpen = (mode == "app_active");
}

static void runScript(ScriptEntry& script, bool toggleIfAlreadyOpen = true)
{
  if (!nativeEnsureSupportedReaperVersion()) return;

  const std::string targetMode = script.autoOpenMode ? script.autoOpenMode : "";
  if (targetMode != "app_active" && !ensureScriptRegistered(script)) return;
  const bool targetOpen = isScriptWindowOpen(script);
  const std::string activeMode = getExclusiveActiveScriptMode();
  bool anotherScriptWindowOpen = false;
  for (ScriptEntry& other : g_scripts) {
    if (&other != &script && isScriptWindowOpen(other)) {
      anotherScriptWindowOpen = true;
      break;
    }
  }

  // Clique no mesmo modo funciona como liga/desliga somente quando ele e o unico
  // script aberto. Se uma versao antiga deixou Beta e Estable juntos, o clique
  // preserva o alvo e encerra primeiro a outra versao.
  if (targetOpen && activeMode == targetMode && !anotherScriptWindowOpen) {
    if (!toggleIfAlreadyOpen) {
      rememberActiveScriptMode(targetMode);
      return;
    }
    terminateKnownScript(script);
    g_state.pendingScriptMode.clear();
    g_state.pendingScriptWaitTicks = 0;
    rememberActiveScriptMode("");
    if (targetMode == "app_active") g_state.appActiveScreenOpen = false;
    return;
  }

  // Encerra primeiro qualquer outro modo. O alvo so sera aberto pelo timer
  // quando a janela anterior realmente desaparecer, evitando Beta e Estable
  // coexistirem ate mesmo por um frame.
  if (closeOtherOpenScripts(script)) {
    return;
  }

  launchScriptNow(script);
}

static void processPendingScriptSwitchOnMainThread()
{
  if (g_state.pendingScriptMode.empty()) return;

  ScriptEntry* target = nullptr;
  for (ScriptEntry& script : g_scripts) {
    if (script.autoOpenMode && g_state.pendingScriptMode == script.autoOpenMode) {
      target = &script;
      break;
    }
  }
  if (!target) {
    g_state.pendingScriptMode.clear();
    g_state.pendingScriptWaitTicks = 0;
    return;
  }

  bool anotherWindowStillOpen = false;
  for (ScriptEntry& other : g_scripts) {
    if (&other == target) continue;
    if (isScriptWindowOpen(other)) {
      anotherWindowStillOpen = true;
      break;
    }
  }

  if (anotherWindowStillOpen) {
    ++g_state.pendingScriptWaitTicks;
    // Reforca o encerramento apenas uma vez se a janela demorou a responder.
    if (g_state.pendingScriptWaitTicks == 8) {
      for (ScriptEntry& other : g_scripts) {
        if (&other != target && isScriptWindowOpen(other)) terminateKnownScript(other);
      }
    }
    return;
  }

  launchScriptNow(*target);
  g_state.pendingScriptMode.clear();
  g_state.pendingScriptWaitTicks = 0;
}


static bool isVsHookRunMode(const std::string& mode)
{
  return mode == "beta" || mode == "estable";
}

static ScriptEntry* findScriptByAutoOpenModeAny(const char* mode)
{
  if (!mode || !*mode) return nullptr;
  for (ScriptEntry& script : g_scripts) {
    if (script.autoOpenMode && std::strcmp(script.autoOpenMode, mode) == 0) {
      return &script;
    }
  }
  return nullptr;
}

static std::string detectRunningVsHookMode()
{
  const std::string mode = getExclusiveActiveScriptMode();
  return isVsHookRunMode(mode) ? mode : std::string();
}

static void openAppActiveScreenForDirector()
{
  // Esta funcao so pode rodar na thread principal do REAPER.
  // O handler HTTP deve chamar requestAppActiveScreenForDirector(), nao esta funcao.
  if (g_state.pcAccessOverride) return;

  const auto now = std::chrono::steady_clock::now();
  if (g_lastAppActiveTransition.time_since_epoch().count() != 0 &&
      std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastAppActiveTransition).count() < 2500) {
    return;
  }

  ScriptEntry* appActiveScript = findScriptByAutoOpenModeAny("app_active");
  if (!appActiveScript) return;

  // Nao confia apenas em GetToggleCommandStateEx para script defer/gfx:
  // em alguns setups ele volta 0 mesmo com o APP ATIVO rodando.
  // Se chamarmos Main_OnCommand de novo, o REAPER abre o popup
  // "ReaScript task control". Portanto, depois que a extensao abriu
  // o APP ATIVO, tratamos como aberto ate haver retomada/logout.
  if (g_state.appActiveScreenOpen ||
      (!g_state.lastLaunchedMode.empty() && g_state.lastLaunchedMode == "app_active")) {
    return;
  }

  const std::string runningMode = detectRunningVsHookMode();
  if (!runningMode.empty()) {
    g_state.modeBeforeDirectorApp = runningMode;
  } else if (g_state.modeBeforeDirectorApp.empty() && isVsHookRunMode(g_state.lastLaunchedMode)) {
    g_state.modeBeforeDirectorApp = g_state.lastLaunchedMode;
  }

  // runScript fecha Beta/Estable antes de abrir APP ATIVO porque todos os scripts
  // fazem parte de g_scripts. Isso evita duas janelas GFX pesadas ao mesmo tempo.
  runScript(*appActiveScript, false);
  g_state.appActiveScreenOpen = true;
  g_lastAppActiveTransition = now;
}

static void requestAppActiveScreenForDirector()
{
  // Thread-safe: pode ser chamado pelo servidor HTTP. Nao chama nenhuma API do REAPER.
  g_appActiveOpenRequested.store(true);
}

static void processAppActiveScreenRequestsOnMainThread()
{
  if (!g_appActiveOpenRequested.exchange(false)) return;
  openAppActiveScreenForDirector();
}

static bool resumePcAccessFromAppActiveScreen();

static void processPcResumeRequestFromAppActiveOnMainThread()
{
  // A API chamada pelo APP ATIVO apenas agenda a retomada. O encerramento do
  // APP ATIVO e a reabertura do Beta/Estable acontecem aqui, no timer da extensão,
  // depois que o callback Lua já devolveu o controle ao REAPER.
  if (GetExtState_ptr) {
    const char* raw = GetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "PC_RESUME_REQUEST_V1");
    const std::string token = raw ? raw : "";
    if (!token.empty()) {
      static std::string s_lastPcResumeToken;
      if (token != s_lastPcResumeToken) {
        s_lastPcResumeToken = token;
        g_pcResumeRequested.store(true);
      }
      if (SetExtState_ptr) {
        SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "PC_RESUME_REQUEST_V1", "", false);
      }
    }
  }

  if (!g_pcResumeRequested.exchange(false)) return;
  resumePcAccessFromAppActiveScreen();
}

static void requestDirectorLogoutFromPcResume()
{
  if (!SetExtState_ptr) return;

  const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  const std::string token = std::string("pc_resume_") + std::to_string(static_cast<long long>(nowMs));

  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "DIRECTOR_LOGOUT_REQUEST_V1", token.c_str(), false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "DIRECTOR_LOGOUT_REQUEST_AT_V1", std::to_string(static_cast<long long>(nowMs)).c_str(), false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "DIRECTOR_LOGOUT_COMMAND_V1", "director_force_logout", false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "DIRECTOR_LOGOUT_TARGET_V1", "director", false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "DIRECTOR_ACTIVE_V1", "0", false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "DIRECTOR_ACTIVE_AT_SEC_V1", "0", false);
}

// O logout global precisa permanecer tempo suficiente para todos os apps Diretor
// conectados receberem o mesmo token, mas nunca pode travar uma nova tela de
// senha indefinidamente. O timestamp usa epoch em ms (PC resume e senha).
static bool nativeDirectorLogoutRequestIsFresh(const std::string& token, const std::string& timestamp)
{
  if (token.empty() || timestamp.empty()) return false;
  const long long raw = std::atoll(timestamp.c_str());
  if (raw <= 0) return false;
  const long long atMs = raw < 100000000000LL ? raw * 1000LL : raw;
  const long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  // Todos os clientes ativos consultam o bridge em menos de um segundo. Oito
  // segundos cobre atrasos de rede sem deixar um token velho bloquear login.
  return atMs <= nowMs + 2000LL && nowMs - atMs <= 8000LL;
}

static bool resumePcAccessFromAppActiveScreen()
{
  g_state.pcAccessOverride = true;
  g_state.appActiveScreenOpen = false;
  requestDirectorLogoutFromPcResume();
  nativeCloseAppActivePanel();

  std::string mode = g_state.modeBeforeDirectorApp;
  if (!isVsHookRunMode(mode)) {
    mode = isVsHookRunMode(g_state.lastLaunchedMode) ? g_state.lastLaunchedMode : std::string("estable");
  }

  ScriptEntry* target = findScriptByAutoOpenModeAny(mode.c_str());
  if (!target) return false;

  // Reabre exatamente o modo que estava aberto quando o Diretor assumiu
  // (Beta ou Estable), usando a mesma geometria/dock que a janela nativa salvou.
  runScript(*target, false);
  g_state.modeBeforeDirectorApp = mode;
  g_lastAppActiveTransition = std::chrono::steady_clock::now();
  return true;
}

static ReaProject* getCurrentProject(char* pathOut, int pathOutSize);
static std::string getCurrentProjectSignature();

static std::string getAutoOpenMode()
{
  if (!GetExtState_ptr) return std::string();

  const char* value = GetExtState_ptr(kExtStateSection, kAutoOpenModeKey);
  if (value && value[0]) {
    return value;
  }

  // Compatibilidade com instalacoes antigas: se o usuario ja tinha
  // "Abrir VS Hook junto com o REAPER" ativo, a extensao nova assume Estable.
  const char* legacy = GetExtState_ptr(kExtStateSection, kLegacyAutoOpenKey);
  if (legacy && std::strcmp(legacy, "1") == 0) {
    return "estable";
  }

  return std::string();
}

static void setAutoOpenModeRaw(const char* mode, bool showError)
{
  if (!SetExtState_ptr) {
    if (showError) {
      showDiagnostic("REAPER nao entregou a API SetExtState para salvar essa configuracao.");
    }
    return;
  }

  SetExtState_ptr(kExtStateSection, kAutoOpenModeKey, mode ? mode : "", true);

  // Desliga a chave antiga para ela nao religar um modo antigo depois que o usuario
  // escolher Beta, Estable ou desativar o auto-inicio.
  SetExtState_ptr(kExtStateSection, kLegacyAutoOpenKey, "0", true);
}

static void setAutoOpenMode(const char* mode)
{
  setAutoOpenModeRaw(mode, true);
}

static bool isAutoOpenModeValue(const std::string& mode)
{
  return mode == "beta" || mode == "estable";
}

static ReaProject* getCurrentProject(char* pathOut, int pathOutSize)
{
  if (pathOut && pathOutSize > 0) pathOut[0] = '\0';
  if (!EnumProjects_ptr) return nullptr;
  return EnumProjects_ptr(-1, pathOut, pathOutSize);
}

static std::string getCurrentProjectSignature()
{
  char path[2048] = "";
  ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));

  std::ostringstream oss;
  oss << reinterpret_cast<std::uintptr_t>(project) << "|" << normalizeSlashes(path);
  return oss.str();
}

static std::string getProjectAutoOpenMode()
{
  if (!GetProjExtState_ptr) return std::string();

  char value[64] = "";
  char path[2048] = "";
  ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));

  // Igual a logica de startup action por projeto da SWS: a configuracao fica no .RPP.
  GetProjExtState_ptr(project, kExtStateSection, kProjectAutoOpenModeKey, value, static_cast<int>(sizeof(value)));

  const std::string mode = value;
  return isAutoOpenModeValue(mode) ? mode : std::string();
}

static void setProjectAutoOpenModeRaw(const char* mode, bool showError)
{
  if (!SetProjExtState_ptr) {
    if (showError) {
      showDiagnostic("REAPER nao entregou a API SetProjExtState para salvar configuracao deste projeto.");
    }
    return;
  }

  const std::string safeMode = (mode && isAutoOpenModeValue(mode)) ? mode : "";
  char path[2048] = "";
  ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));

  // Igual a SWS: grava no ProjExtState do projeto. Para ficar permanente, salve o .RPP.
  SetProjExtState_ptr(project, kExtStateSection, kProjectAutoOpenModeKey, safeMode.c_str());

  // Evita que o timer abra imediatamente ao marcar a opcao; ela passa a valer
  // quando este projeto for aberto/carregado de novo.
  if (!safeMode.empty()) {
    g_state.autoOpenedProjectSignature = getCurrentProjectSignature() + "|" + safeMode;
  } else {
    g_state.autoOpenedProjectSignature.clear();
  }
}

static void setProjectAutoOpenMode(const char* mode)
{
  setProjectAutoOpenModeRaw(mode, true);
}

static void normalizeAutoOpenConflict()
{
  // Se uma configuracao antiga deixou REAPER e projeto ativos ao mesmo tempo,
  // o projeto ganha por ser mais especifico. Isso evita duplo disparo ao abrir.
  if (!getAutoOpenMode().empty() && !getProjectAutoOpenMode().empty()) {
    setAutoOpenModeRaw("", false);
  }
}

static const char* displayNameForAutoOpenMode(const char* mode)
{
  for (const ScriptEntry& script : g_scripts) {
    if (mode && std::strcmp(script.autoOpenMode, mode) == 0) {
      return script.displayName;
    }
  }
  return "VS Hook";
}

static void toggleAutoOpenMode(const char* mode)
{
  const std::string currentMode = getAutoOpenMode();

  if (mode && currentMode == mode) {
    setAutoOpenMode("");
    showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " nao vai mais iniciar junto com o REAPER.");
    return;
  }

  // Regra de exclusividade: escolher iniciar junto com o REAPER desmarca
  // qualquer configuracao deste projeto, para nao existir duplo disparo.
  setProjectAutoOpenModeRaw("", false);
  setAutoOpenMode(mode);
  showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " configurado para iniciar junto com o REAPER. A opcao deste projeto foi desmarcada.");
}

static void toggleProjectAutoOpenMode(const char* mode)
{
  const std::string currentMode = getProjectAutoOpenMode();

  if (mode && currentMode == mode) {
    setProjectAutoOpenMode("");
    showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " nao vai mais iniciar junto com ESTE projeto.");
    return;
  }

  // Regra de exclusividade: escolher iniciar junto com ESTE projeto desmarca
  // o auto-inicio global com o REAPER, para nao existir conflito.
  setAutoOpenModeRaw("", false);
  setProjectAutoOpenMode(mode);
  showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " configurado para iniciar junto com ESTE projeto. O auto-inicio com o REAPER foi desmarcado. Salve o projeto para gravar essa configuracao no RPP.");
}

static void runScriptByAutoOpenMode(const std::string& mode)
{
  if (!isAutoOpenModeValue(mode)) return;

  for (ScriptEntry& script : g_scripts) {
    if (mode == script.autoOpenMode) {
      runScript(script, false);
      return;
    }
  }
}

static ScriptEntry* findScriptByCommand(int command)
{
  for (ScriptEntry& script : g_scripts) {
    if (script.commandId != 0 && command == script.commandId) {
      return &script;
    }
  }
  return nullptr;
}

// Necessario para cliques vindos do menu Extensions e tambem chamadas normais da action list.
static bool hookCommand(int command, int flag)
{
  (void)flag;

  if (handleTransportQueueStopCommand(command)) return true;
  for (AutoOpenEntry& entry : g_autoOpenEntries) {
    if (entry.commandId != 0 && command == entry.commandId) {
      toggleAutoOpenMode(entry.autoOpenMode);
      return true;
    }
  }

  for (AutoOpenEntry& entry : g_projectAutoOpenEntries) {
    if (entry.commandId != 0 && command == entry.commandId) {
      toggleProjectAutoOpenMode(entry.autoOpenMode);
      return true;
    }
  }

  ScriptEntry* script = findScriptByCommand(command);
  if (script) {
    runScript(*script);
    return true;
  }

  return false;
}

// Necessario para atalhos, MIDI e alguns disparos internos do REAPER.
static bool hookCommand2(KbdSectionInfo* sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  (void)sec;
  (void)val;
  (void)val2;
  (void)relmode;
  (void)hwnd;

  if (handleTransportQueueStopCommand(command)) return true;
  for (AutoOpenEntry& entry : g_autoOpenEntries) {
    if (entry.commandId != 0 && command == entry.commandId) {
      toggleAutoOpenMode(entry.autoOpenMode);
      return true;
    }
  }

  for (AutoOpenEntry& entry : g_projectAutoOpenEntries) {
    if (entry.commandId != 0 && command == entry.commandId) {
      toggleProjectAutoOpenMode(entry.autoOpenMode);
      return true;
    }
  }

  ScriptEntry* script = findScriptByCommand(command);
  if (script) {
    runScript(*script);
    return true;
  }

  return false;
}

static int toggleActionState(int commandId)
{
  normalizeAutoOpenConflict();

  for (AutoOpenEntry& entry : g_autoOpenEntries) {
    if (entry.commandId != 0 && commandId == entry.commandId) {
      return getAutoOpenMode() == entry.autoOpenMode ? 1 : 0;
    }
  }

  for (AutoOpenEntry& entry : g_projectAutoOpenEntries) {
    if (entry.commandId != 0 && commandId == entry.commandId) {
      return getProjectAutoOpenMode() == entry.autoOpenMode ? 1 : 0;
    }
  }

  const std::string activeScriptMode = getExclusiveActiveScriptMode();
  for (ScriptEntry& script : g_scripts) {
    if (script.commandId != 0 && commandId == script.commandId) {
      return script.autoOpenMode && activeScriptMode == script.autoOpenMode ? 1 : 0;
    }
  }

  return -1;
}

static void insertMenuString(HMENU hMenu, const char* text, int commandId, bool checked)
{
  MENUITEMINFO mi = { sizeof(MENUITEMINFO), };
  mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
  mi.fType = MFT_STRING;
  mi.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
  mi.dwTypeData = (char*)text;
  mi.wID = commandId;
  InsertMenuItem(hMenu, 0, true, &mi);
}

static void insertMenuSeparator(HMENU hMenu)
{
  MENUITEMINFO mi = { sizeof(MENUITEMINFO), };
  mi.fMask = MIIM_TYPE;
  mi.fType = MFT_SEPARATOR;
  InsertMenuItem(hMenu, 0, true, &mi);
}

static void setMenuCommandChecked(HMENU hMenu, int commandId, bool checked)
{
  if (!hMenu || commandId == 0) return;
#ifdef _WIN32
  CheckMenuItem(hMenu, static_cast<UINT>(commandId), MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
#else
  CheckMenuItem(hMenu, commandId, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
#endif
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  if (!menustr || !hMenu) return;
  if (std::strcmp(menustr, "Main extensions") != 0) return;

  normalizeAutoOpenConflict();

  const std::string autoMode = getAutoOpenMode();
  const std::string projectAutoMode = getProjectAutoOpenMode();

  // flag 1 acontece quando o menu vai ser exibido. Atualiza a marcacao
  // imediatamente, sem precisar reiniciar o REAPER.
  if (flag == 1) {
    for (AutoOpenEntry& entry : g_autoOpenEntries) {
      setMenuCommandChecked(hMenu, entry.commandId, autoMode == entry.autoOpenMode);
    }
    for (AutoOpenEntry& entry : g_projectAutoOpenEntries) {
      setMenuCommandChecked(hMenu, entry.commandId, projectAutoMode == entry.autoOpenMode);
    }
    const std::string activeScriptMode = getExclusiveActiveScriptMode();
    for (ScriptEntry& script : g_scripts) {
      if (!script.showInExtensionsMenu) continue;
      const bool checked = script.commandId != 0 && script.autoOpenMode && activeScriptMode == script.autoOpenMode;
      setMenuCommandChecked(hMenu, script.commandId, checked);
    }
    return;
  }

  if (flag != 0) return;

  // Insere de tras para frente porque cada item entra na posicao 0.
  // Ordem final no menu:
  // VS Hook Beta / VS Hook Estable
  // separador / auto-inicio com REAPER / separador / auto-inicio deste projeto.
  for (int i = static_cast<int>(sizeof(g_projectAutoOpenEntries) / sizeof(g_projectAutoOpenEntries[0])) - 1; i >= 0; --i) {
    if (g_projectAutoOpenEntries[i].commandId == 0) continue;
    const bool checked = projectAutoMode == g_projectAutoOpenEntries[i].autoOpenMode;
    insertMenuString(
      hMenu,
      g_projectAutoOpenEntries[i].displayName,
      g_projectAutoOpenEntries[i].commandId,
      checked
    );
  }

  insertMenuSeparator(hMenu);

  for (int i = static_cast<int>(sizeof(g_autoOpenEntries) / sizeof(g_autoOpenEntries[0])) - 1; i >= 0; --i) {
    if (g_autoOpenEntries[i].commandId == 0) continue;
    const bool checked = autoMode == g_autoOpenEntries[i].autoOpenMode;
    insertMenuString(
      hMenu,
      g_autoOpenEntries[i].displayName,
      g_autoOpenEntries[i].commandId,
      checked
    );
  }

  insertMenuSeparator(hMenu);

  const std::string activeScriptMode = getExclusiveActiveScriptMode();
  for (int i = static_cast<int>(sizeof(g_scripts) / sizeof(g_scripts[0])) - 1; i >= 0; --i) {
    if (!g_scripts[i].showInExtensionsMenu) continue;
    if (g_scripts[i].commandId == 0) continue;

    const bool checked = g_scripts[i].autoOpenMode && activeScriptMode == g_scripts[i].autoOpenMode;
    insertMenuString(hMenu, g_scripts[i].displayName, g_scripts[i].commandId, checked);
  }
}



// ==========================================================
// VS Hook Native Bridge
// ----------------------------------------------------------
// Responsavel por manter em memoria o snapshot pesado que antes era
// escrito pelo Lua em JSON: projetos, repertorios, musicas, filhos,
// markers e sync leve. A Hook Center consulta esta extensao por HTTP
// local; o Lua apenas recebe comandos via API nativa e envia estados
// realmente exclusivos dele, como cronometro/fila.
// ==========================================================

static const int kNativeBridgePort = 47830;
static const int kNativeBridgeLiveIntervalMs = 220;
static const char* kPlaylistExtSection = "CHATGPT_REGION_PLAYLIST";
static const char* kPlaylistExtKey = "PLAYLISTS_DB_V3";
static const char* kNativeExtStateSection = "VS_HOOK_NATIVE_BRIDGE";
static const char* kEnsureReaPitchRequestKey = "ENSURE_REAPITCH_REQUEST_V1";
static const char* kEnsureReaPitchResultKey = "ENSURE_REAPITCH_RESULT_V1";
static const char* kDirectorAuthRevisionKey = "DIRECTOR_AUTH_REVISION_V1";
static const char* kNativeCommandsExtKey = "COMMANDS_JSON_V1";
static const char* kNativeAutoplayExtKey = "AUTOPLAY_ENABLED_V1";
static const char* kNativeAutoStopExtKey = "AUTO_STOP_ENABLED_V1";
static const char* kNativeAutoBlocoExtKey = "AUTO_BLOCO_ENABLED_V1";
static const char* kLuaControlHeartbeatKey = "LUA_CONTROL_HEARTBEAT_V1";
static const char* kLuaQueueSyncSeqKey = "LUA_QUEUE_SYNC_SEQ_V1";
static const char* kLuaQueueActiveKey = "LUA_QUEUE_ACTIVE_V1";
static const char* kLuaQueueIdKey = "LUA_QUEUE_ID_V1";
static const char* kLuaQueueIndexKey = "LUA_QUEUE_INDEX_V1";
static const char* kLuaQueueStartKey = "LUA_QUEUE_START_V1";
static const char* kLuaQueueEndKey = "LUA_QUEUE_END_V1";
static const char* kLuaQueueNameKey = "LUA_QUEUE_NAME_V1";
static const char* kLuaQueueKindKey = "LUA_QUEUE_KIND_V1";
static const char* kLuaQueueManualKey = "LUA_QUEUE_MANUAL_V1";
static const char* kLuaStateSyncSeqKey = "LUA_STATE_SYNC_SEQ_V1";
static const char* kLuaAutoplayStateKey = "LUA_AUTOPLAY_STATE_V1";
static const char* kLuaAutoBlocoStateKey = "LUA_AUTOBLOCO_STATE_V1";
static const char* kLuaAutoStopStateKey = "LUA_AUTOSTOP_STATE_V1";
static const char* kLuaStopPauseStateKey = "LUA_STOP_PAUSE_STATE_V1";
static const char* kLuaFamilyDrawersIdsKey = "LUA_FAMILY_DRAWERS_OPEN_IDS_V1";
static const char* kLuaDrawerOutlineEnabledKey = "LUA_DRAWER_OUTLINE_ENABLED_V1";
static const char* kLuaDrawerOutlineColorKey = "LUA_DRAWER_OUTLINE_COLOR_V1";
static const char* kLuaDrawerSymbolEnabledKey = "LUA_DRAWER_SYMBOL_ENABLED_V1";
static const char* kLuaDrawerSymbolColorKey = "LUA_DRAWER_SYMBOL_COLOR_V1";
static const char* kSharedControlOwnerKey = "CONTROL_OWNER_V1";
static const char* kSharedResumeRevisionKey = "RESUME_REVISION_V1";
static const char* kSharedQueueActiveKey = "SHARED_QUEUE_ACTIVE_V1";
static const char* kSharedQueueIdKey = "SHARED_QUEUE_ID_V1";
static const char* kSharedQueueIndexKey = "SHARED_QUEUE_INDEX_V1";
static const char* kSharedQueueStartKey = "SHARED_QUEUE_START_V1";
static const char* kSharedQueueEndKey = "SHARED_QUEUE_END_V1";
static const char* kSharedQueueManualKey = "SHARED_QUEUE_MANUAL_V1";
static const char* kSharedQueueKindKey = "SHARED_QUEUE_KIND_V1";
static const char* kSharedAutoStopKey = "SHARED_AUTOSTOP_ENABLED_V1";
static const char* kSharedStopPauseKey = "SHARED_STOP_PAUSE_ENABLED_V1";
static const char* kSharedFamilyDrawersIdsKey = "SHARED_FAMILY_DRAWERS_OPEN_IDS_V1";
static const char* kSharedFamilyDrawersOwnerKey = "SHARED_FAMILY_DRAWERS_OWNER_V1";
static const char* kSharedFamilyDrawersRevisionKey = "SHARED_FAMILY_DRAWERS_REVISION_V1";
static const char* kLuaWindowExtStateSection = "JBKEYS_VSLIVE_WINDOW";
static const char* kLuaWindowAutoplayKey = "AUTOPLAY_ENABLED_V1";
static const char* kLuaWindowAutoBlocoKey = "AUTOBLOCO_V1";
static const char* kLuaWindowPreviewKey = "TP_PREVIEW_ACTIVE_V1";
static const char* kLuaWindowMultiLoopKey = "MULTILOOPS_NEW_STATE_V1";
static const char* kNativeMultiLoopProjectSection = "VS_HOOK_MULTILOOPS";
static const char* kNativeMultiLoopProjectStateKey = "STATE_V2";
static const char* kNativeMultiLoopProjectMsStateKey = "MS_STATE_V1";
static const char* kNativeMultiLoopKeyPrefix = "MULTILOOPS_STATE_V2_";
static const char* kNativeMultiLoopMsKeyPrefix = "MULTILOOPS_MS_STATE_V1_";
static const char* kNativeMultiLoopLuaPushKey = "MULTILOOPS_LUA_PUSH_V2";
static const char* kNativeMultiLoopLuaPushProjectKey = "MULTILOOPS_LUA_PUSH_PROJECT_V2";
static const char* kNativeMultiLoopLuaPushSequenceKey = "MULTILOOPS_LUA_PUSH_SEQUENCE_V2";
static const char* kNativeMultiLoopLegacyMigratedKey = "MULTILOOPS_LEGACY_MIGRATED_V2";
static const char* kNativeLuaLiveExtKey = "LUA_LIVE_JSON_V1";
static const char* kManualStopFadeoutEnabledKey = "MANUAL_STOP_FADEOUT_ENABLED_V1";
static const char* kManualStopFadeoutDurationKey = "MANUAL_STOP_FADEOUT_DURATION_V1";
static const char* kManualStopFadeoutTracksKey = "MANUAL_STOP_FADEOUT_TRACKS_V1";
static const char* kManualStopFadeoutRevisionKey = "MANUAL_STOP_FADEOUT_REVISION_V1";
static const char* kManualStopFadeoutProjectSection = "VS_HOOK_FADEROUT";
static const char* kStopPauseModeEnabledKey = "STOP_PAUSE_MODE_ENABLED_V1";
static const char* kStopPauseModeRevisionKey = "STOP_PAUSE_MODE_REVISION_V1";

#ifdef _WIN32
using native_socket_t = SOCKET;
static const native_socket_t kInvalidNativeSocket = INVALID_SOCKET;
static void nativeCloseSocket(native_socket_t s) { if (s != INVALID_SOCKET) closesocket(s); }
#else
using native_socket_t = int;
static const native_socket_t kInvalidNativeSocket = -1;
static void nativeCloseSocket(native_socket_t s) { if (s >= 0) close(s); }
#endif

static std::mutex g_nativeMutex;
static std::thread g_nativeThread;
static std::atomic<bool> g_nativeRunning{false};
static native_socket_t g_nativeServerSocket = kInvalidNativeSocket;
static std::string g_nativeStateJson;
static std::string g_nativeSnapshotSignature;
static std::string g_nativeLuaLiveFragment;
static std::deque<std::string> g_nativeCommandQueue;
// A thread HTTP nunca pode chamar a API do REAPER. Ela apenas deposita os
// comandos aqui; o startupTimer consome a fila na thread principal.
static std::deque<std::string> g_nativeHttpCommandQueue;
static std::deque<std::string> g_nativeTunerCommandQueue;
static std::vector<std::string> g_nativeCommandHistory;
static uint64_t g_nativeCommandSequence = 0;
static std::string g_nativePremixSelectedSongId;
static double g_nativePremixSelectedSongStart = 0.0;
static double g_nativePremixSelectedSongEnd = 0.0;
static std::chrono::steady_clock::time_point g_nativeLastDirectorHeartbeat;
static std::chrono::steady_clock::time_point g_nativeLastLuaControlHeartbeat;
static std::string g_nativeLastLuaControlHeartbeatToken;
static std::string g_nativeLastLuaQueueSyncSeq;
static std::string g_nativeLastLuaStateSyncSeq;
static uint64_t g_nativeResumeRevision = 1;
static bool g_nativeDirectorWasActive = false;
static std::string g_nativeControlOwner = "lua";
static std::string g_nativePublishedCommandsSignature;
static std::chrono::steady_clock::time_point g_nativeLastLiveBuild;
static bool g_nativeWinsockStarted = false;
static std::atomic<bool> g_nativeForceStateBuild{false};
// Atualizacao viva e releitura estrutural sao coisas diferentes. Um scroll ou
// uma selecao do app nao pode obrigar a extensao a reenumerar o projeto inteiro.
static std::atomic<bool> g_nativeForceSnapshotBuild{false};
static std::atomic<uint64_t> g_lastCursorMoveSeq{0};
// Os meters ficam fora do snapshot principal. A thread HTTP apenas renova esta
// assinatura; a leitura dos peaks acontece no timer principal do REAPER e para
// automaticamente quando Mixer/Premix deixam de solicitar dados.
static std::atomic<int64_t> g_nativeMetersLastRequestMs{0};
static std::string g_nativeMetersJson = "{\"ok\":true,\"active\":false,\"tracks\":[],\"master\":null}";
static std::chrono::steady_clock::time_point g_nativeMetersLastBuild;
static bool g_nativeMetersPublishedActive = false;

static int64_t nativeSteadyNowMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct NativeManualStopFadeoutTrack {
  MediaTrack* track = nullptr;
  double before = 1.0;
};

struct NativeManualStopFadeoutRuntime {
  bool active = false;
  bool restorePending = false;
  ReaProject* project = nullptr;
  std::chrono::steady_clock::time_point startedAt;
  std::chrono::steady_clock::time_point restoreAt;
  double durationSec = 3.0;
  double progress = 0.0;
  std::string pendingStopCommand;
  std::vector<NativeManualStopFadeoutTrack> tracks;
};

static std::mutex g_nativeManualStopFadeoutMutex;
static NativeManualStopFadeoutRuntime g_nativeManualStopFadeoutRuntime;
static bool g_nativeManualStopFadeoutBypass = false;

struct NativePendingSelectionCommand {
  bool pending = false;
  std::string type;
  std::string id;
  double startPos = 0.0;
  double endPos = 0.0;
  double minPos = 0.0;
  double maxPos = 0.0;
  bool hasBounds = false;
  bool hasExplicitStart = false;
  bool exactPosition = false;
  bool seekToMarker = false;
  bool noSeek = false;
  int markerNumber = 0;
  int markerEnumIndex = -1;
};

static NativePendingSelectionCommand g_nativePendingSelection;
static bool g_nativeTimerRunning = false;
static std::string g_nativeTimerMode = "progressive";
static double g_nativeTimerBaseSec = 0.0;
static double g_nativeTimerTargetSec = 0.0;
static bool g_nativeTimerResetToZero = false;
static std::chrono::steady_clock::time_point g_nativeTimerStartedAtSteady;
static std::chrono::system_clock::time_point g_nativeTimerStartedAtSystem;


static std::string nativeJsonEscape(const std::string& value)
{
  std::string out;
  out.reserve(value.size() + 16);
  for (char c : value) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          std::ostringstream oss;
          oss << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
          out += oss.str();
        } else {
          out += c;
        }
    }
  }
  return out;
}

static std::string nativeJsonString(const std::string& value)
{
  return std::string("\"") + nativeJsonEscape(value) + "\"";
}

static std::string nativeTrim(std::string value)
{
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) value.erase(value.begin());
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) value.pop_back();
  return value;
}

// Mesma funcao simple_hash usada pelo VS Hook.lua e pelo App Diretor.
// A extensao publica somente o hash; a senha nunca sai do ExtState local do REAPER.
static std::string nativeDirectorPasswordHash(const std::string& rawPassword)
{
  const std::string password = nativeTrim(rawPassword);
  if (password.empty()) return std::string();

  uint32_t h1 = 0x45D9u;
  uint32_t h2 = 0x2710u;
  for (size_t i = 0; i < password.size(); ++i) {
    const uint32_t b = static_cast<unsigned char>(password[i]);
    const uint32_t pos = static_cast<uint32_t>(i + 1u);
    h1 = (h1 ^ (b * pos + 17u)) & 0xFFFFFFu;
    h2 = (h2 + ((b + static_cast<uint32_t>(i)) * 131u)) & 0xFFFFFFu;
    h1 = (h1 * 33u + h2) & 0xFFFFFFu;
    h2 = (h2 * 17u + h1) & 0xFFFFFFu;
  }

  const uint32_t value = static_cast<uint32_t>((h1 << 12u) + h2);
  std::ostringstream out;
  out << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
  return out.str();
}

static std::string nativeReadDirectorPassword()
{
  if (!GetExtState_ptr) return std::string();
  const char* direct = GetExtState_ptr("JBKEYS_VSHOOK_ACCESS", "DIRECTOR_PASS_V1");
  std::string password = direct ? nativeTrim(direct) : std::string();
  if (!password.empty()) return password;
  const char* legacy = GetExtState_ptr("JBKEYS_VSHOOK_ACCESS", "PASS_V1");
  return legacy ? nativeTrim(legacy) : std::string();
}

static std::string nativeUnescapeExtField(std::string value)
{
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\' && i + 1 < value.size()) {
      const char n = value[i + 1];
      if (n == 'p') { out += '|'; ++i; continue; }
      if (n == 't') { out += '\t'; ++i; continue; }
      if (n == 'n') { out += '\n'; ++i; continue; }
      if (n == '\\') { out += '\\'; ++i; continue; }
    }
    out += value[i];
  }
  return out;
}

static std::vector<std::string> nativeSplit(const std::string& text, char delim)
{
  std::vector<std::string> out;
  std::string cur;
  for (char c : text) {
    if (c == delim) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  out.push_back(cur);
  return out;
}

static bool nativeStartsWith(const std::string& value, const std::string& prefix)
{
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

static std::string nativeCleanSongName(std::string name)
{
  name = nativeTrim(name);
  if (nativeStartsWith(name, "--")) name = nativeTrim(name.substr(2));
  // Remove prefixo numerico simples usado na interface, sem destruir nomes reais.
  size_t i = 0;
  while (i < name.size() && name[i] >= '0' && name[i] <= '9') ++i;
  if (i > 0 && i < name.size() && (name[i] == '-' || name[i] == '.' || name[i] == ')' || name[i] == ' ')) {
    while (i < name.size() && (name[i] == '-' || name[i] == '.' || name[i] == ')' || name[i] == ' ')) ++i;
    name = nativeTrim(name.substr(i));
  }
  return name;
}

static std::string nativeBasenameNoRpp(const std::string& rawPath)
{
  std::string p = normalizeSlashes(rawPath);
  const size_t slash = p.find_last_of('/');
  std::string base = slash == std::string::npos ? p : p.substr(slash + 1);
  const std::string lower = base.size() >= 4 ? base.substr(base.size() - 4) : std::string();
  if (lower == ".RPP" || lower == ".rpp") base = base.substr(0, base.size() - 4);
  return base;
}

static std::string nativeIsoNow()
{
  std::time_t t = std::time(nullptr);
  std::tm tmv{};
#ifdef _WIN32
  gmtime_s(&tmv, &t);
#else
  gmtime_r(&t, &tmv);
#endif
  char buf[32] = "";
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
  return buf;
}

static std::string nativeNumber(double v, int precision = 6)
{
  if (!std::isfinite(v)) v = 0.0;
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(precision) << v;
  return oss.str();
}


static bool nativeParseDouble(const std::string& raw, double& out)
{
  const std::string value = nativeTrim(raw);
  if (value.empty() || value == "null") return false;
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (end == value.c_str()) return false;
  while (end && *end) {
    if (*end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') return false;
    ++end;
  }
  if (!std::isfinite(parsed)) return false;
  out = parsed;
  return true;
}

static std::string nativeNumberFromJsonText(const std::string& raw, double fallback = 0.0, int precision = 6)
{
  double value = fallback;
  nativeParseDouble(raw, value);
  return nativeNumber(value, precision);
}

static std::string nativeNormalizeTimerMode(std::string mode)
{
  mode = nativeTrim(mode);
  std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  if (mode == "local_time" || mode == "localtime" || mode == "local" ||
      mode == "horario_local" || mode == "hora_local" || mode == "clock" || mode == "relogio") {
    return "local_time";
  }
  if (mode == "countdown" || mode == "regressive" || mode == "regressivo" || mode == "down") {
    return "countdown";
  }
  return "progressive";
}

static std::string nativeFormatTimerTextFromSeconds(double seconds, bool forceNegative = false)
{
  if (!std::isfinite(seconds)) seconds = 0.0;
  const bool negative = forceNegative || seconds < 0.0;
  int total = static_cast<int>(std::floor(std::fabs(seconds) + 0.0001));
  const int maxTotal = (99 * 3600) + (59 * 60) + 59;
  if (total > maxTotal) total = maxTotal;
  const int h = total / 3600;
  const int m = (total % 3600) / 60;
  const int s = total % 60;
  char buf[20] = "";
  std::snprintf(buf, sizeof(buf), negative ? "- %02d:%02d:%02d" : "%02d:%02d:%02d", h, m, s);
  return std::string(buf);
}

static double nativeTimerEpochMs(std::chrono::system_clock::time_point tp)
{
  if (tp.time_since_epoch().count() == 0) return 0.0;
  return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
}

static double nativeTimerElapsedSinceStartSecLocked()
{
  if (!g_nativeTimerRunning || g_nativeTimerStartedAtSteady.time_since_epoch().count() == 0) return 0.0;
  const auto elapsed = std::chrono::steady_clock::now() - g_nativeTimerStartedAtSteady;
  const double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0;
  return std::max(0.0, seconds);
}

static double nativeTimerDisplaySecLocked()
{
  if (g_nativeTimerMode == "local_time") return 0.0;
  const double elapsed = nativeTimerElapsedSinceStartSecLocked();
  if (g_nativeTimerMode == "countdown") {
    if (!g_nativeTimerRunning && g_nativeTimerResetToZero) return 0.0;
    return g_nativeTimerBaseSec - elapsed;
  }
  return std::max(0.0, g_nativeTimerBaseSec + elapsed);
}

static bool nativeTimerCountdownExpiredLocked()
{
  return g_nativeTimerMode == "countdown" && g_nativeTimerRunning && nativeTimerDisplaySecLocked() <= 0.0;
}

// Compatibilidade com o contrato historico do Teleprompt/Lua:
// - timerAccumulatedSec representa quanto tempo ja foi consumido antes do ciclo atual;
// - timerElapsedSec representa o tempo total consumido;
// - timerDisplaySec representa o valor visual (restante no regressivo).
// A implementacao antiga publicava o valor restante em timerAccumulatedSec no modo
// countdown. O Teleprompt subtraia esse valor do alvo e mostrava 00:00:00 ao iniciar.
static double nativeTimerAccumulatedSecLocked()
{
  if (g_nativeTimerMode == "local_time") return 0.0;
  if (g_nativeTimerMode == "countdown") {
    if (!g_nativeTimerRunning && g_nativeTimerResetToZero) return 0.0;
    return std::max(0.0, g_nativeTimerTargetSec - g_nativeTimerBaseSec);
  }
  return std::max(0.0, g_nativeTimerBaseSec);
}

static double nativeTimerElapsedValueSecLocked()
{
  if (g_nativeTimerMode == "local_time") return 0.0;
  if (g_nativeTimerMode == "countdown") {
    if (!g_nativeTimerRunning && g_nativeTimerResetToZero) return 0.0;
    return std::max(0.0, g_nativeTimerTargetSec - nativeTimerDisplaySecLocked());
  }
  return nativeTimerDisplaySecLocked();
}

static std::string nativeLocalTimeText()
{
  std::time_t t = std::time(nullptr);
  std::tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  char buf[16] = "";
  std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmv);
  return std::string(buf);
}

static std::string nativeBuildTimerStateJsonLocked()
{
  const double displaySec = nativeTimerDisplaySecLocked();
  const double accumulatedSec = nativeTimerAccumulatedSecLocked();
  const double elapsedSec = nativeTimerElapsedValueSecLocked();
  const double startedAtMs = g_nativeTimerRunning ? nativeTimerEpochMs(g_nativeTimerStartedAtSystem) : 0.0;
  const std::string localTimeText = nativeLocalTimeText();
  const bool countdownExpired = nativeTimerCountdownExpiredLocked();
  const double overrunSec = countdownExpired ? std::max(0.0, -displaySec) : 0.0;
  const std::string displayText = g_nativeTimerMode == "local_time" ? localTimeText : nativeFormatTimerTextFromSeconds(displaySec, countdownExpired);

  std::ostringstream json;
  json << "{";
  json << "\"running\":" << (g_nativeTimerRunning ? "true" : "false") << ",";
  json << "\"active\":" << (g_nativeTimerRunning ? "true" : "false") << ",";
  json << "\"mode\":" << nativeJsonString(g_nativeTimerMode) << ",";
  json << "\"type\":" << nativeJsonString(g_nativeTimerMode) << ",";
  json << "\"startedAt\":" << nativeNumber(startedAtMs) << ",";
  json << "\"startedAtMs\":" << nativeNumber(startedAtMs) << ",";
  json << "\"accumulatedSec\":" << nativeNumber(accumulatedSec) << ",";
  json << "\"targetSec\":" << nativeNumber(g_nativeTimerTargetSec) << ",";
  json << "\"elapsedSec\":" << nativeNumber(elapsedSec) << ",";
  json << "\"displaySec\":" << nativeNumber(displaySec) << ",";
  json << "\"expired\":" << (countdownExpired ? "true" : "false") << ",";
  json << "\"overrun\":" << (countdownExpired ? "true" : "false") << ",";
  json << "\"negative\":" << (countdownExpired ? "true" : "false") << ",";
  json << "\"overrunSec\":" << nativeNumber(overrunSec) << ",";
  json << "\"displayText\":" << nativeJsonString(displayText) << ",";
  json << "\"localTimeText\":" << nativeJsonString(localTimeText);
  json << "}";
  return json.str();
}

static void nativePublishTimerStateLocked()
{
  if (!SetExtState_ptr) return;
  const std::string timerJson = nativeBuildTimerStateJsonLocked();
  SetExtState_ptr(kNativeExtStateSection, "TIMER_STATE_V1", timerJson.c_str(), false);
}


static std::string nativeHexByte(double value)
{
  int n = static_cast<int>(std::round(std::max(0.0, std::min(1.0, value)) * 255.0));
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << n;
  return oss.str();
}

static std::string nativeRgbHex(double r, double g, double b)
{
  return std::string("#") + nativeHexByte(r) + nativeHexByte(g) + nativeHexByte(b);
}

static std::string nativeBlockColorHexFromKey(const std::string& rawKey, int blockNumber)
{
  const std::string key = nativeTrim(rawKey);
  if (key == "none") return std::string();
  if (key == "yellow") return nativeRgbHex(1.00, 0.74, 0.12);
  if (key == "green")  return nativeRgbHex(0.18, 0.74, 0.36);
  if (key == "blue")   return nativeRgbHex(0.20, 0.52, 1.00);
  if (key == "purple") return nativeRgbHex(0.56, 0.34, 0.94);
  if (key == "red")    return nativeRgbHex(1.00, 0.38, 0.28);
  if (key == "cyan")   return nativeRgbHex(0.10, 0.70, 0.78);
  if (key == "pink")   return nativeRgbHex(0.95, 0.38, 0.72);
  if (key == "olive")  return nativeRgbHex(0.58, 0.70, 0.18);
  if (key == "orange") return nativeRgbHex(0.96, 0.56, 0.16);
  if (key == "white")  return nativeRgbHex(0.62, 0.62, 0.62);

  static const double palette[][3] = {
    {1.00, 0.74, 0.12}, {0.18, 0.74, 0.36}, {0.20, 0.52, 1.00}, {0.56, 0.34, 0.94},
    {1.00, 0.38, 0.28}, {0.10, 0.70, 0.78}, {0.95, 0.38, 0.72}, {0.58, 0.70, 0.18}
  };
  const int safeBlock = std::max(1, blockNumber);
  const int idx = (safeBlock - 1) % 8;
  return nativeRgbHex(palette[idx][0], palette[idx][1], palette[idx][2]);
}

static double nativeVolumeToRatio(double volume)
{
  if (!std::isfinite(volume) || volume <= 0.000001) return 0.0;
  const double db = 20.0 * std::log10(volume);
  return std::max(0.0, std::min(1.0, (db + 60.0) / 72.0));
}

static double nativeVolumeToDb(double volume)
{
  if (!std::isfinite(volume) || volume <= 0.000001) return -150.0;
  return 20.0 * std::log10(volume);
}

static std::string nativeTrackName(MediaTrack* track, int index)
{
  char buf[512] = "";
  if (track && GetTrackName_ptr && GetTrackName_ptr(track, buf, static_cast<int>(sizeof(buf))) && buf[0]) return std::string(buf);
  return std::string("Pista ") + std::to_string(index + 1);
}

static std::string nativeTrackGuid(MediaTrack* track, int index)
{
  if (track && GetTrackGUID_ptr && guidToString_ptr) {
    GUID* guid = GetTrackGUID_ptr(track);
    if (guid) {
      char buf[80] = "";
      guidToString_ptr(guid, buf);
      if (buf[0]) return std::string(buf);
    }
  }
  return std::string("track_") + std::to_string(index + 1);
}

static double nativeSafeTrackPeak(MediaTrack* track, int channel)
{
  if (!track || !Track_GetPeakInfo_ptr) return 0.0;
  const double peak = Track_GetPeakInfo_ptr(track, channel);
  if (!std::isfinite(peak) || peak <= 0.0) return 0.0;
  return std::min(8.0, peak);
}

static void nativeUpdateTrackMetersOnMainThread()
{
  const int64_t nowMs = nativeSteadyNowMs();
  const int64_t requestedAtMs = g_nativeMetersLastRequestMs.load(std::memory_order_relaxed);
  const bool requested = requestedAtMs > 0 && (nowMs - requestedAtMs) <= 900;
  if (!requested) {
    if (g_nativeMetersPublishedActive) {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      g_nativeMetersJson = "{\"ok\":true,\"active\":false,\"tracks\":[],\"master\":null}";
      g_nativeMetersPublishedActive = false;
    }
    return;
  }

  const std::chrono::steady_clock::time_point tickNow = std::chrono::steady_clock::now();
  if (g_nativeMetersLastBuild.time_since_epoch().count() != 0 &&
      std::chrono::duration_cast<std::chrono::milliseconds>(tickNow - g_nativeMetersLastBuild).count() < 66) {
    return;
  }
  g_nativeMetersLastBuild = tickNow;

  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  std::ostringstream tracks;
  tracks << "[";
  bool first = true;
  if (project && CountTracks_ptr && GetTrack_ptr && Track_GetPeakInfo_ptr) {
    const int count = CountTracks_ptr(project);
    for (int index = 0; index < count; ++index) {
      MediaTrack* track = GetTrack_ptr(project, index);
      if (!track) continue;
      const double left = nativeSafeTrackPeak(track, 0);
      const double right = nativeSafeTrackPeak(track, 1);
      const double volume = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "D_VOL") : 1.0;
      const bool mute = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "B_MUTE") > 0.5 : false;
      const bool solo = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "I_SOLO") > 0.5 : false;
      if (!first) tracks << ",";
      first = false;
      const std::string guid = nativeTrackGuid(track, index);
      tracks << "{";
      tracks << "\"id\":" << nativeJsonString(guid) << ",";
      tracks << "\"guid\":" << nativeJsonString(guid) << ",";
      tracks << "\"index\":" << (index + 1) << ",";
      tracks << "\"peakL\":" << nativeNumber(left, 7) << ",";
      tracks << "\"peakR\":" << nativeNumber(right, 7) << ",";
      tracks << "\"peak\":" << nativeNumber(std::max(left, right), 7) << ",";
      tracks << "\"volume\":" << nativeNumber(volume, 7) << ",";
      tracks << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(volume), 7) << ",";
      tracks << "\"db\":" << nativeNumber(nativeVolumeToDb(volume), 3) << ",";
      tracks << "\"mute\":" << (mute ? "true" : "false") << ",";
      tracks << "\"solo\":" << (solo ? "true" : "false");
      tracks << "}";
    }
  }
  tracks << "]";

  std::string masterJson = "null";
  if (project && GetMasterTrack_ptr && Track_GetPeakInfo_ptr) {
    MediaTrack* master = GetMasterTrack_ptr(project);
    if (master) {
      const double left = nativeSafeTrackPeak(master, 0);
      const double right = nativeSafeTrackPeak(master, 1);
      const double volume = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(master, "D_VOL") : 1.0;
      const bool mute = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(master, "B_MUTE") > 0.5 : false;
      const bool solo = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(master, "I_SOLO") > 0.5 : false;
      std::ostringstream masterStream;
      masterStream << "{";
      masterStream << "\"id\":\"MASTER_TRACK\",";
      masterStream << "\"index\":0,";
      masterStream << "\"peakL\":" << nativeNumber(left, 7) << ",";
      masterStream << "\"peakR\":" << nativeNumber(right, 7) << ",";
      masterStream << "\"peak\":" << nativeNumber(std::max(left, right), 7) << ",";
      masterStream << "\"volume\":" << nativeNumber(volume, 7) << ",";
      masterStream << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(volume), 7) << ",";
      masterStream << "\"db\":" << nativeNumber(nativeVolumeToDb(volume), 3) << ",";
      masterStream << "\"mute\":" << (mute ? "true" : "false") << ",";
      masterStream << "\"solo\":" << (solo ? "true" : "false");
      masterStream << "}";
      masterJson = masterStream.str();
    }
  }

  std::ostringstream payload;
  payload << "{\"ok\":true,\"active\":true,\"updatedAtMs\":" << nowMs;
  payload << ",\"tracks\":" << tracks.str();
  payload << ",\"master\":" << masterJson << "}";
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeMetersJson = payload.str();
  }
  g_nativeMetersPublishedActive = true;
}

static std::string nativeMediaItemGuid(MediaItem* item, const std::string& fallback)
{
  if (item && GetSetMediaItemInfo_String_ptr) {
    char buf[128] = "";
    if (GetSetMediaItemInfo_String_ptr(item, "GUID", buf, false) && buf[0]) return std::string(buf);
  }
  return fallback;
}

static std::string nativeTakeName(MediaItem* item, const std::string& fallback)
{
  if (item && GetActiveTake_ptr && GetTakeName_ptr) {
    MediaItem_Take* take = GetActiveTake_ptr(item);
    const char* name = take ? GetTakeName_ptr(take) : nullptr;
    if (name && *name) return std::string(name);
  }
  return fallback;
}

static int nativeTakeFxCount(MediaItem* item)
{
  if (!item || !GetActiveTake_ptr || !TakeFX_GetCount_ptr) return 0;
  MediaItem_Take* take = GetActiveTake_ptr(item);
  return take ? std::max(0, TakeFX_GetCount_ptr(take)) : 0;
}

static bool nativeTakeAnyFxEnabled(MediaItem* item)
{
  if (!item || !GetActiveTake_ptr) return false;
  MediaItem_Take* take = GetActiveTake_ptr(item);
  if (!take) return false;
  const int count = nativeTakeFxCount(item);
  if (count <= 0) return false;
  if (!TakeFX_GetEnabled_ptr) return true;
  for (int i = 0; i < count; ++i) {
    if (TakeFX_GetEnabled_ptr(take, i)) return true;
  }
  return false;
}

static std::string nativeLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return value;
}

static std::string nativeJsonExtractString(const std::string& json, const std::string& key)
{
  const std::string pat = std::string("\"") + key + "\"";
  size_t p = json.find(pat);
  if (p == std::string::npos) return std::string();
  p = json.find(':', p + pat.size());
  if (p == std::string::npos) return std::string();
  ++p;
  while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) ++p;
  if (p >= json.size()) return std::string();
  if (json[p] == '"') {
    ++p;
    std::string out;
    while (p < json.size()) {
      char c = json[p++];
      if (c == '"') break;
      if (c == '\\' && p < json.size()) {
        char n = json[p++];
        if (n == 'n') out += '\n';
        else if (n == 'r') out += '\r';
        else if (n == 't') out += '\t';
        else out += n;
      } else out += c;
    }
    return out;
  }
  size_t e = p;
  while (e < json.size() && json[e] != ',' && json[e] != '}' && json[e] != ']' && json[e] != '\r' && json[e] != '\n') ++e;
  return nativeTrim(json.substr(p, e - p));
}


static std::string nativeMediaItemNotes(MediaItem* item)
{
  if (!item || !GetSetMediaItemInfo_String_ptr) return std::string();
  std::string buf(65536, '\0');
  if (!GetSetMediaItemInfo_String_ptr(item, "P_NOTES", buf.data(), false)) return std::string();
  const size_t n = buf.find('\0');
  if (n != std::string::npos) buf.resize(n);
  return nativeTrim(buf);
}

static std::string nativeCollectLyricsForRange(ReaProject* project, double start, double end)
{
  if (!project || !CountTracks_ptr || !GetTrack_ptr || !GetTrackNumMediaItems_ptr || !GetTrackMediaItem_ptr || !GetMediaItemInfo_Value_ptr) return std::string();
  std::string best;
  const int trackCount = CountTracks_ptr(project);
  for (int ti = 0; ti < trackCount; ++ti) {
    MediaTrack* track = GetTrack_ptr(project, ti);
    if (!track) continue;
    const int itemCount = GetTrackNumMediaItems_ptr(track);
    for (int ii = 0; ii < itemCount; ++ii) {
      MediaItem* item = GetTrackMediaItem_ptr(track, ii);
      if (!item) continue;
      const double pos = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
      const double len = std::max(0.0, GetMediaItemInfo_Value_ptr(item, "D_LENGTH"));
      const double itemEnd = pos + len;
      const bool startsInside = pos >= start - 0.0005 && pos < end - 0.0005;
      const bool overlaps = itemEnd > start + 0.0005 && pos < end - 0.0005;
      if (!startsInside && !overlaps) continue;
      const std::string notes = nativeMediaItemNotes(item);
      if (!notes.empty()) {
        // Prioriza texto que começa dentro da música; se só atravessa, guarda como fallback.
        if (startsInside) return notes;
        if (best.empty()) best = notes;
      }
    }
  }
  return best;
}

static bool nativeItemStartsInRange(MediaItem* item, double start, double end)
{
  if (!item || !GetMediaItemInfo_Value_ptr) return false;
  const double pos = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
  return pos >= start - 0.0005 && pos < end - 0.0005;
}

static bool nativeItemOverlapsRange(MediaItem* item, double start, double end)
{
  if (!item || !GetMediaItemInfo_Value_ptr) return false;
  const double pos = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
  const double len = std::max(0.0, GetMediaItemInfo_Value_ptr(item, "D_LENGTH"));
  const double itemEnd = pos + len;
  return itemEnd > start + 0.0005 && pos < end - 0.0005;
}

struct NativeSongWindow {
  std::string id;
  std::string name;
  std::string type;
  double start = 0.0;
  double end = 0.0;
  int sourceNumber = 0;
  bool isBlock = false;
  bool isHashParent = false;
  bool isHashChild = false;
  bool isRegionChild = false;
  std::string parentId;
  std::string parentName;
  std::string blockColorKey;
  std::string blockColorHex;
  std::string inheritedBlockColorHex;
  std::string lyricsText;
  bool liveExecuted = false;
  int playlistOrder = 0;
  std::string playlistEntryId;
  int enumIndex = -1;
  int parentEnumIndex = -1;
  int premixNumber = 0;
  int tunerValue = 0;
};

static std::vector<NativeSongWindow> g_nativeSongWindows;
static bool g_nativeCurrentTransportPlaying = false;
static std::string g_nativeCurrentPlayingId;
static double g_nativeCurrentSongStart = 0.0;
static double g_nativeCurrentSongEnd = 0.0;
static double g_nativeCurrentPlayPosition = 0.0;
static std::string g_nativeSelectedId;
static std::string g_nativeSelectedTab;
static std::string g_nativeDirectorActivePage = "playlist";
static double g_nativeDirectorPlaylistScrollRatio = 0.0;
static double g_nativeDirectorRegionsScrollRatio = 0.0;
static std::map<std::string, bool> g_nativeDirectorOpenFamilyDrawers;
static std::string g_nativeFamilyDrawersOwner = "lua";
static uint64_t g_nativeFamilyDrawersRevision = 1;
static bool g_nativeDrawerOutlineEnabled = true;
static bool g_nativeDrawerSymbolEnabled = true;
static std::string g_nativeDrawerOutlineColor = "yellow";
static std::string g_nativeDrawerSymbolColor = "yellow";
static uint64_t g_nativeDirectorListScrollRevision = 0;
static double g_nativeSelectedStart = 0.0;
static double g_nativeSelectedEnd = 0.0;
static uint64_t g_nativeStopReadySequence = 0;
static std::chrono::steady_clock::time_point g_nativeSuppressStoppedTransitionPrepareUntil;
static std::string g_nativeSelectedMarkerId;
static std::string g_nativeArmedMarkerId;
static std::chrono::steady_clock::time_point g_nativeArmedMarkerSetAt;
static double g_nativeSelectedMarkerPos = 0.0;
static double g_nativeArmedMarkerStartPlayPos = 0.0;

static std::vector<NativeSongWindow> g_nativeActivePlaylistItems;
static bool g_nativeAutoplayEnabled = false;
static bool g_nativeAutoBlocoEnabled = false;
static bool g_nativeAutoStopEnabled = true;
static bool g_nativeAutomationSettingsLoaded = false;
static std::string g_nativeQueuedSongId;
static std::string g_nativeQueuedPlaylistSongId;
static int g_nativeQueuedRegionNumber = 0;
static double g_nativeQueuedStart = 0.0;
static double g_nativeQueuedEnd = 0.0;
static int g_nativeQueuedPlaylistIndex = 0;
static bool g_nativeQueuedManual = false;
static std::string g_nativeQueuedKind = "playlist";
static std::string g_nativeAutoBlocoTargetSongId;
static std::string g_nativeAutoBlocoTargetPlaylistSongId;
static int g_nativeAutoBlocoTargetRegionNumber = 0;
static double g_nativeAutoBlocoTargetStart = 0.0;
static double g_nativeAutoBlocoTargetEnd = 0.0;
static int g_nativeAutoBlocoTargetPlaylistIndex = 0;
static int g_nativePreviewMode = 0;
static bool g_nativeLiveMarkEnabled = false;
static std::map<std::string, bool> g_nativeLiveExecutedItems;
static std::string g_nativeLiveTrackerId;
static double g_nativeLiveTrackerElapsed = 0.0;
static std::chrono::steady_clock::time_point g_nativeLiveTrackerClock;
struct NativeMultiLoopTrackPreset {
  bool autoFader = false;
  bool mute = false;
  bool solo = false;
  bool fadeLimitSet = false;
  double fadeLimitDb = 0.0;
};
struct NativeMultiLoopSongState {
  bool loop[2] = {false, false};
  bool ms[2] = {false, false};
  int fadeSec[2] = {3, 3};
  std::map<std::string, NativeMultiLoopTrackPreset> tracks[2];
};
struct NativeMultiLoopPair {
  bool valid = false;
  std::string key;
  std::string songKey;
  int slot = 0;
  double start = 0.0;
  double end = 0.0;
};
struct NativeMultiLoopTrackRestore {
  MediaTrack* track = nullptr;
  std::string guid;
  double volume = 1.0;
  double targetVolume = 0.0;
  double mute = 0.0;
  double solo = 0.0;
};
static std::map<std::string, NativeMultiLoopSongState> g_nativeMultiLoopStates;
static std::string g_nativeMultiLoopRaw;
static std::string g_nativeMultiLoopStorageKey;
static std::string g_nativeMultiLoopMsStorageKey;
static ReaProject* g_nativeMultiLoopProject = nullptr;
static std::string g_nativeMultiLoopLuaPushSequence;
static std::string g_nativeMultiLoopFocusId;
static std::string g_nativeMultiLoopFocusKey;
static std::string g_nativeMultiLoopFocusName;
static double g_nativeMultiLoopFocusStart = 0.0;
static double g_nativeMultiLoopFocusEnd = 0.0;
static uint64_t g_nativeMultiLoopPairCacheRevision = 1;
static NativeMultiLoopPair g_nativeMultiLoopActivePair;
static std::string g_nativeMultiLoopDisarmedPairKey;
static std::vector<NativeMultiLoopTrackRestore> g_nativeMultiLoopRestore;
static std::vector<NativeMultiLoopTrackRestore> g_nativeMultiLoopFadeTracks;
static bool g_nativeMultiLoopFadeOutActive = false;
static bool g_nativeMultiLoopFadeInActive = false;
static double g_nativeMultiLoopFadeStart = 0.0;
static double g_nativeMultiLoopFadeEnd = 0.0;
static bool g_nativeMultiLoopWasPaused = false;
static bool g_nativeMultiLoopPauseSeeked = false;
static double g_nativeMultiLoopPausedPlayPos = 0.0;
static double g_nativeMultiLoopPausedEditPos = 0.0;
static bool g_nativeMultiLoopBypassActive = false;
static std::string g_nativeMultiLoopBypassSongKey;
static double g_nativeMultiLoopBypassLastPos = 0.0;
static double g_nativeMultiLoopBypassMaxEnd = 0.0;
static std::vector<NativeMultiLoopTrackRestore> g_nativeMultiLoopBypassVolumeBaseline;
static std::string g_nativeQueuedSeekSignature;
static std::string g_nativeLastAutoQueueForPlayingId;
// FIX74: quando a fila nativa faz seek 1s antes do fim, o Lua/autostop antigo pode
// tentar parar logo depois. Mantemos uma pequena janela de proteção para garantir
// que a música da fila continue tocando após o seek.
static std::string g_nativeQueueHandoffPlayId;
static double g_nativeQueueHandoffStart = 0.0;
static double g_nativeQueueHandoffEnd = 0.0;
static std::chrono::steady_clock::time_point g_nativeQueueHandoffProtectUntil;
static std::string g_nativeAutoStopLastStoppedSignature;
static std::string g_nativeAutoStopLastPlayingId;
static double g_nativeAutoStopLastPlayingStart = 0.0;
static double g_nativeAutoStopLastPlayingEnd = 0.0;

static bool nativeBoolFromText(std::string value, bool fallback);

static void nativeLoadAutomationSettingsOnceLocked()
{
  g_nativeAutomationSettingsLoaded = true;
  if (GetExtState_ptr) {
    const char* nativeAuto = GetExtState_ptr(kNativeExtStateSection, kNativeAutoplayExtKey);
    const char* luaAuto = (!nativeAuto || !*nativeAuto) ? GetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowAutoplayKey) : nullptr;
    const char* autoValue = (nativeAuto && *nativeAuto) ? nativeAuto : luaAuto;
    if (autoValue && *autoValue) g_nativeAutoplayEnabled = nativeBoolFromText(autoValue, g_nativeAutoplayEnabled);
    const char* autoStop = GetExtState_ptr(kNativeExtStateSection, kNativeAutoStopExtKey);
    if (autoStop && *autoStop) g_nativeAutoStopEnabled = nativeBoolFromText(autoStop, g_nativeAutoStopEnabled);
    const char* nativeAutoBloco = GetExtState_ptr(kNativeExtStateSection, kNativeAutoBlocoExtKey);
    const char* luaAutoBloco = GetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowAutoBlocoKey);
    // AUTOBLOCO_V1 e o estado historico do Lua e ganha na primeira migracao.
    // Depois disso app e extensao gravam os dois campos juntos.
    const char* autoBlocoValue = (luaAutoBloco && *luaAutoBloco) ? luaAutoBloco : nativeAutoBloco;
    if (autoBlocoValue && *autoBlocoValue) g_nativeAutoBlocoEnabled = nativeBoolFromText(autoBlocoValue, g_nativeAutoBlocoEnabled);
    const char* previewMode = GetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowPreviewKey);
    if (previewMode && *previewMode) {
      const int value = std::atoi(previewMode);
      g_nativePreviewMode = (value >= 1 && value <= 3) ? value : 0;
    }
  }
}

static void nativeSaveAutoplayEnabledLocked()
{
  if (SetExtState_ptr) {
    SetExtState_ptr(kNativeExtStateSection, kNativeAutoplayExtKey, g_nativeAutoplayEnabled ? "1" : "0", true);
    SetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowAutoplayKey, g_nativeAutoplayEnabled ? "1" : "0", true);
  }
}

static void nativeSaveAutoStopEnabledLocked()
{
  if (!SetExtState_ptr) return;
  const char* value = g_nativeAutoStopEnabled ? "1" : "0";
  SetExtState_ptr(kNativeExtStateSection, kNativeAutoStopExtKey, value, true);
  SetExtState_ptr(kNativeExtStateSection, "AUTOSTOP_ENABLED_V1", value, true);
  SetExtState_ptr(kNativeExtStateSection, kSharedAutoStopKey, value, false);
  // Mantem o estado persistente usado pelo botao Stop/AutoStop do Lua alinhado
  // com o comando vindo do App Diretor.
  SetExtState_ptr(kLuaWindowExtStateSection, "AUTOSTOP_ENABLED_V1", value, true);
}

static void nativeSaveAutoBlocoEnabledLocked()
{
  if (!SetExtState_ptr) return;
  const char* value = g_nativeAutoBlocoEnabled ? "1" : "0";
  SetExtState_ptr(kNativeExtStateSection, kNativeAutoBlocoExtKey, value, true);
  SetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowAutoBlocoKey, value, true);
}

static void nativeSavePreviewModeLocked()
{
  if (!SetExtState_ptr) return;
  SetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowPreviewKey, std::to_string(g_nativePreviewMode).c_str(), true);
}


static void nativePublishSharedControlStateLocked()
{
  if (!SetExtState_ptr) return;
  SetExtState_ptr(kNativeExtStateSection, kSharedControlOwnerKey, g_nativeControlOwner.c_str(), false);
  const std::string revision = std::to_string(g_nativeResumeRevision);
  SetExtState_ptr(kNativeExtStateSection, kSharedResumeRevisionKey, revision.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, kNativeAutoplayExtKey, g_nativeAutoplayEnabled ? "1" : "0", true);
  SetExtState_ptr(kNativeExtStateSection, kNativeAutoBlocoExtKey, g_nativeAutoBlocoEnabled ? "1" : "0", true);
  SetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowAutoBlocoKey, g_nativeAutoBlocoEnabled ? "1" : "0", true);
  SetExtState_ptr(kNativeExtStateSection, kNativeAutoStopExtKey, g_nativeAutoStopEnabled ? "1" : "0", true);
  SetExtState_ptr(kNativeExtStateSection, "AUTOSTOP_ENABLED_V1", g_nativeAutoStopEnabled ? "1" : "0", true);
  SetExtState_ptr(kNativeExtStateSection, kSharedAutoStopKey, g_nativeAutoStopEnabled ? "1" : "0", false);
  SetExtState_ptr(kNativeExtStateSection, kSharedStopPauseKey, g_stopPauseModeEnabled.load() ? "1" : "0", false);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueActiveKey, g_nativeQueuedSongId.empty() ? "0" : "1", false);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueIdKey, g_nativeQueuedSongId.c_str(), false);
  const std::string qindex = std::to_string(g_nativeQueuedPlaylistIndex);
  const std::string qstart = std::to_string(g_nativeQueuedStart);
  const std::string qend = std::to_string(g_nativeQueuedEnd);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueIndexKey, qindex.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueStartKey, qstart.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueEndKey, qend.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueManualKey, g_nativeQueuedManual ? "1" : "0", false);
  SetExtState_ptr(kNativeExtStateSection, kSharedQueueKindKey, g_nativeQueuedKind.c_str(), false);
  std::ostringstream drawerIds;
  bool firstDrawerId = true;
  for (const auto& entry : g_nativeDirectorOpenFamilyDrawers) {
    if (!entry.second || entry.first.empty()) continue;
    if (!firstDrawerId) drawerIds << '|';
    drawerIds << entry.first;
    firstDrawerId = false;
  }
  const std::string drawerRevision = std::to_string(g_nativeFamilyDrawersRevision);
  SetExtState_ptr(kNativeExtStateSection, kSharedFamilyDrawersIdsKey, drawerIds.str().c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, kSharedFamilyDrawersOwnerKey, g_nativeFamilyDrawersOwner.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, kSharedFamilyDrawersRevisionKey, drawerRevision.c_str(), false);
}

static void nativeBumpSharedRevisionLocked(const char* owner = nullptr)
{
  if (owner && *owner) g_nativeControlOwner = owner;
  ++g_nativeResumeRevision;
  nativePublishSharedControlStateLocked();
}

static void nativeCancelQueueHandoffProtectionLocked()
{
  g_nativeQueueHandoffPlayId.clear();
  g_nativeQueueHandoffStart = 0.0;
  g_nativeQueueHandoffEnd = 0.0;
  g_nativeQueueHandoffProtectUntil = std::chrono::steady_clock::time_point();
}

static void nativeCancelQueueHandoffProtection()
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  nativeCancelQueueHandoffProtectionLocked();
}

static bool nativeIsLuaControlActive()
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (g_nativeLastLuaControlHeartbeat.time_since_epoch().count() == 0) return false;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - g_nativeLastLuaControlHeartbeat).count();
  return elapsed >= 0 && elapsed < 1800;
}

static bool nativeIsDirectorControlActive()
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (g_nativeLastDirectorHeartbeat.time_since_epoch().count() == 0) return false;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - g_nativeLastDirectorHeartbeat).count();
  return elapsed >= 0 && elapsed < 12000;
}

static bool nativeIsRuntimeControlActive()
{
  return nativeIsLuaControlActive() || nativeIsDirectorControlActive();
}

static std::string nativePremixSongKey(const NativeSongWindow& item)
{
  if (item.isBlock || item.isHashParent) return std::string();
  const bool legacyMarkerChild = item.isHashChild && !item.isRegionChild;
  const std::string kind = legacyMarkerChild ? "hash_region_marker_song" : "region";
  const int number = item.premixNumber > 0 ? item.premixNumber : item.sourceNumber;
  const std::string childId = legacyMarkerChild
    ? std::to_string(item.enumIndex >= 0 ? item.enumIndex : item.sourceNumber)
    : std::string();
  const std::string parentId = legacyMarkerChild
    ? std::to_string(item.parentEnumIndex >= 0 ? item.parentEnumIndex : std::atoi(item.parentId.c_str()))
    : std::string();
  std::ostringstream oss;
  oss << kind << "|" << number << "|" << childId << "|" << parentId << "|"
      << std::fixed << std::setprecision(6) << item.start << "|" << item.end;
  return oss.str();
}

static std::string nativeMultiLoopSongKey(const NativeSongWindow& item)
{
  if (item.isBlock || item.isHashParent) return std::string();
  const int number = item.sourceNumber > 0 ? item.sourceNumber : item.premixNumber;
  return number > 0 ? std::to_string(number) : std::string();
}

static std::string nativeGetProjExtStateString(ReaProject* project, const char* section, const char* key);

static const char* kTunerProjectSection = "CHATGPT_REGION_PLAYLIST";
static const char* kTunerOffsetsKey = "TUNER_OFFSETS_V1";

static std::map<std::string, int> nativeLoadTunerOffsets(ReaProject* project)
{
  std::map<std::string, int> out;
  const std::string raw = nativeGetProjExtStateString(project, kTunerProjectSection, kTunerOffsetsKey);
  size_t pos = 0;
  while (pos <= raw.size()) {
    const size_t end = raw.find('|', pos);
    const std::string entry = raw.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    const size_t eq = entry.find('=');
    if (eq != std::string::npos) {
      const std::string key = nativeTrim(entry.substr(0, eq));
      const int value = std::max(-12, std::min(12, std::atoi(entry.substr(eq + 1).c_str())));
      if (!key.empty() && value != 0) out[key] = value;
    }
    if (end == std::string::npos) break;
    pos = end + 1;
  }
  return out;
}

static void nativeSaveTunerOffsets(ReaProject* project, const std::map<std::string, int>& offsets)
{
  if (!SetProjExtState_ptr || !project) return;
  std::ostringstream oss;
  bool first = true;
  for (const auto& kv : offsets) {
    if (kv.first.empty() || kv.second == 0) continue;
    if (!first) oss << '|';
    first = false;
    oss << kv.first << '=' << std::max(-12, std::min(12, kv.second));
  }
  SetProjExtState_ptr(project, kTunerProjectSection, kTunerOffsetsKey, oss.str().c_str());
}

static std::string nativeTunerKey(const NativeSongWindow& song)
{
  return song.sourceNumber > 0 ? std::to_string(song.sourceNumber) : song.id;
}

static int nativeTunerValueForSong(ReaProject* project, const NativeSongWindow& song)
{
  const auto offsets = nativeLoadTunerOffsets(project);
  const auto it = offsets.find(nativeTunerKey(song));
  return it == offsets.end() ? 0 : it->second;
}

static double nativeParseFxDisplayNumber(std::string text)
{
  std::replace(text.begin(), text.end(), ',', '.');
  std::string clean;
  for (char c : text) if (!std::isspace(static_cast<unsigned char>(c))) clean.push_back(c);
  const char* begin = clean.c_str();
  char* end = nullptr;
  double value = std::strtod(begin, &end);
  if (end != begin) return value;
  for (size_t i = 0; i < clean.size(); ++i) {
    if (clean[i] == '+' || clean[i] == '-' || std::isdigit(static_cast<unsigned char>(clean[i]))) {
      value = std::strtod(clean.c_str() + i, &end);
      if (end != clean.c_str() + i) return value;
    }
  }
  // Texto sem numero nao equivale a zero. Retornar zero fazia a extensao
  // considerar parametros nao numericos como se fossem centrados em 0.
  return std::numeric_limits<double>::quiet_NaN();
}

static int nativeFindReaPitchFx(MediaTrack* track)
{
  if (!track || !TrackFX_GetCount_ptr || !TrackFX_GetFXName_ptr) return -1;
  const int count = TrackFX_GetCount_ptr(track);
  char name[512];
  for (int fx = 0; fx < count; ++fx) {
    name[0] = 0;
    if (TrackFX_GetFXName_ptr(track, fx, name, sizeof(name))) {
      if (nativeLower(name).find("reapitch") != std::string::npos) return fx;
    }
  }
  return -1;
}

// Executado somente quando o Lua solicita Converter/Atualizar. Nao existe
// varredura continua: pistas com # no inicio recebem um unico ReaPitch no
// primeiro slot da cadeia, sem abrir a janela de FX.
static int nativeEnsureReaPitchFirstOnHashTracks(ReaProject* project)
{
  if (!project || !CountTracks_ptr || !GetTrack_ptr || !GetTrackName_ptr ||
      !TrackFX_GetCount_ptr || !TrackFX_GetFXName_ptr || !TrackFX_AddByName_ptr) return 0;

  int changed = 0;
  const int trackCount = CountTracks_ptr(project);
  char trackName[512];
  for (int i = 0; i < trackCount; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;

    trackName[0] = 0;
    GetTrackName_ptr(track, trackName, static_cast<int>(sizeof(trackName)));
    const std::string trimmedName = nativeTrim(trackName);
    if (trimmedName.empty() || trimmedName[0] != '#') continue;

    int reapitchFx = nativeFindReaPitchFx(track);
    if (reapitchFx < 0) {
      reapitchFx = TrackFX_AddByName_ptr(track, "ReaPitch (Cockos)", false, -1000);
      if (reapitchFx < 0) {
        reapitchFx = TrackFX_AddByName_ptr(track, "ReaPitch", false, -1000);
      }
      if (reapitchFx >= 0) ++changed;
      continue;
    }

    if (reapitchFx > 0 && TrackFX_CopyToTrack_ptr) {
      TrackFX_CopyToTrack_ptr(track, reapitchFx, track, 0, true);
      if (nativeFindReaPitchFx(track) == 0) ++changed;
    }
  }

  if (changed > 0) {
    if (MarkProjectDirty_ptr) MarkProjectDirty_ptr(project);
    if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
    if (UpdateArrange_ptr) UpdateArrange_ptr();
  }
  return changed;
}

static void nativeProcessEnsureReaPitchRequestOnMainThread()
{
  if (!GetExtState_ptr || !SetExtState_ptr) return;
  const char* raw = GetExtState_ptr(kNativeExtStateSection, kEnsureReaPitchRequestKey);
  const std::string request = raw ? raw : "";
  if (request.empty()) return;

  // Consome antes de executar para cada clique produzir no maximo uma passagem.
  SetExtState_ptr(kNativeExtStateSection, kEnsureReaPitchRequestKey, "", false);
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  const int changed = nativeEnsureReaPitchFirstOnHashTracks(project);
  SetExtState_ptr(kNativeExtStateSection, kEnsureReaPitchResultKey, std::to_string(changed).c_str(), false);
}

static int nativeFindReaPitchSemitoneParam(MediaTrack* track, int fx, double& minOut, double& maxOut)
{
  minOut = -18.0; maxOut = 18.0;
  if (!track || fx < 0 || !TrackFX_GetNumParams_ptr || !TrackFX_GetParamName_ptr || !TrackFX_FormatParamValueNormalized_ptr) return -1;
  const int count = TrackFX_GetNumParams_ptr(track, fx);
  int named = -1;
  char pname[256], bmin[128], bmid[128], bmax[128];
  for (int param = 0; param < count; ++param) {
    pname[0]=bmin[0]=bmid[0]=bmax[0]=0;
    TrackFX_GetParamName_ptr(track, fx, param, pname, sizeof(pname));
    TrackFX_FormatParamValueNormalized_ptr(track, fx, param, 0.0, bmin, sizeof(bmin));
    TrackFX_FormatParamValueNormalized_ptr(track, fx, param, 0.5, bmid, sizeof(bmid));
    TrackFX_FormatParamValueNormalized_ptr(track, fx, param, 1.0, bmax, sizeof(bmax));
    const std::string lower = nativeLower(pname);
    const double mn=nativeParseFxDisplayNumber(bmin), md=nativeParseFxDisplayNumber(bmid), mx=nativeParseFxDisplayNumber(bmax);
    const bool parsed = std::isfinite(mn) && std::isfinite(md) && std::isfinite(mx);
    const bool centered = parsed && std::abs(md) <= 0.001;
    const bool extremes = parsed && mn < 0 && mx > 0 &&
      ((std::abs(mn) >= 12 && std::abs(mx) >= 12) ||
       lower.find("shift") != std::string::npos ||
       lower.find("semi") != std::string::npos);
    if (centered && extremes) { minOut=mn; maxOut=mx; return param; }
    if (named < 0 && lower.find("shift") != std::string::npos && lower.find("semi") != std::string::npos) {
      named=param;
      if (std::isfinite(mn) && std::isfinite(mx) && mx > mn) { minOut=mn; maxOut=mx; }
    }
  }
  return named;
}

static bool nativeApplyTunerValue(ReaProject* project, int semitones)
{
  if (!project || !CountTracks_ptr || !GetTrack_ptr || !GetTrackName_ptr || !TrackFX_SetParamNormalized_ptr) return false;
  semitones = std::max(-12, std::min(12, semitones));
  bool found = false;
  bool changed = false;
  const int count = CountTracks_ptr(project);
  char name[512];
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;
    name[0]=0; GetTrackName_ptr(track, name, sizeof(name));
    const std::string trimmed = nativeTrim(name);
    if (trimmed.empty() || trimmed[0] != '#') continue;
    const int fx = nativeFindReaPitchFx(track);
    if (fx < 0) continue;
    double mn=-18.0, mx=18.0;
    const int param = nativeFindReaPitchSemitoneParam(track, fx, mn, mx);
    if (param < 0 || std::abs(mx-mn) < 0.000001) continue;
    const double target = std::max(mn, std::min(mx, static_cast<double>(semitones)));
    const double normalized = std::max(0.0, std::min(1.0, (target-mn)/(mx-mn)));
    found = true;
    const double current = TrackFX_GetParamNormalized_ptr
      ? TrackFX_GetParamNormalized_ptr(track, fx, param)
      : -1000.0;
    if (!std::isfinite(current) || std::fabs(current - normalized) > 0.000001) {
      if (TrackFX_SetParamNormalized_ptr(track, fx, param, normalized)) changed = true;
    }
  }
  if (changed && UpdateArrange_ptr) UpdateArrange_ptr();
  return found;
}

static const NativeSongWindow* nativeResolveTunerSong(const std::vector<NativeSongWindow>& songs, const std::string& body)
{
  std::string id = nativeJsonExtractString(body, "songId");
  if (id.empty()) id = nativeJsonExtractString(body, "targetId");
  if (id.empty()) id = nativeJsonExtractString(body, "regionId");
  std::string number = nativeJsonExtractString(body, "sourceNumber");
  if (number.empty()) number = nativeJsonExtractString(body, "regionNumber");
  const bool hasExplicitTarget = !id.empty() || !number.empty();
  for (const auto& song : songs) {
    if (song.isBlock || song.isHashParent) continue;
    if ((!id.empty() && song.id == id) || (!number.empty() && std::to_string(song.sourceNumber) == number)) return &song;
  }
  // Um comando apontando para uma regiao-pai nao pode cair por engano na
  // musica que estiver tocando. Pai nunca e alvo ajustavel do Tuner.
  if (hasExplicitTarget) return nullptr;
  if (!g_nativeCurrentPlayingId.empty()) {
    for (const auto& song : songs) {
      if (!song.isBlock && !song.isHashParent && song.id == g_nativeCurrentPlayingId) return &song;
    }
  }
  return nullptr;
}

static void nativeProcessTunerCommands(ReaProject* project, const std::vector<NativeSongWindow>& songs)
{
  std::deque<std::string> queue;
  { std::lock_guard<std::mutex> lock(g_nativeMutex); queue.swap(g_nativeTunerCommandQueue); }
  for (const std::string& body : queue) {
    const std::string type = nativeLower(nativeJsonExtractString(body, "type"));
    auto offsets = nativeLoadTunerOffsets(project);
    if (type == "tuner_reset") {
      offsets.clear(); nativeSaveTunerOffsets(project, offsets);
      { std::lock_guard<std::mutex> lock(g_nativeMutex); nativeBumpSharedRevisionLocked("director"); }
      nativeApplyTunerValue(project, 0); continue;
    }
    const NativeSongWindow* song = nativeResolveTunerSong(songs, body);
    if (!song) continue;
    const std::string key = nativeTunerKey(*song);
    int current = offsets.count(key) ? offsets[key] : 0;
    if (type == "tuner_adjust") {
      std::string d = nativeJsonExtractString(body, "delta");
      if (d.empty()) d = nativeJsonExtractString(body, "direction");
      current += std::atoi(d.c_str());
    } else if (type == "tuner_set") {
      std::string v = nativeJsonExtractString(body, "value");
      if (v.empty()) v = nativeJsonExtractString(body, "semitones");
      current = std::atoi(v.c_str());
    } else continue;
    current = std::max(-12, std::min(12, current));
    if (current == 0) offsets.erase(key); else offsets[key] = current;
    nativeSaveTunerOffsets(project, offsets);
    { std::lock_guard<std::mutex> lock(g_nativeMutex); nativeBumpSharedRevisionLocked("director"); }
    if (song->id == g_nativeCurrentPlayingId) nativeApplyTunerValue(project, current);
  }
}

static bool nativeApplyTunerCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (type != "tuner_adjust" && type != "tuner_set" && type != "tuner_reset" && type != "tuner_focus" && type != "set_tuner_visibility") return false;
  if (type == "tuner_focus" || type == "set_tuner_visibility") return true;
  { std::lock_guard<std::mutex> lock(g_nativeMutex); if (g_nativeTunerCommandQueue.size() > 100) g_nativeTunerCommandQueue.pop_front(); g_nativeTunerCommandQueue.push_back(commandBody); }
  g_nativeForceSnapshotBuild.store(true);
  g_nativeForceStateBuild.store(true);
  return true;
}

static void nativeResetLiveTrackerLocked()
{
  g_nativeLiveTrackerId.clear();
  g_nativeLiveTrackerElapsed = 0.0;
  g_nativeLiveTrackerClock = std::chrono::steady_clock::time_point();
}

static void nativeFinalizeLiveTrackerLocked()
{
  if (!g_nativeLiveTrackerId.empty() && g_nativeLiveTrackerElapsed >= 60.0) {
    const bool wasMarked = g_nativeLiveExecutedItems.find(g_nativeLiveTrackerId) != g_nativeLiveExecutedItems.end();
    g_nativeLiveExecutedItems[g_nativeLiveTrackerId] = true;
    if (!wasMarked) g_nativeForceStateBuild.store(true);
  }
  nativeResetLiveTrackerLocked();
}

static void nativeUpdateLiveMarkTracker(bool advancing, bool paused, const std::string& playingId)
{
  const auto clockNow = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (!g_nativeLiveMarkEnabled) {
    nativeResetLiveTrackerLocked();
    return;
  }
  if (paused) {
    if (!g_nativeLiveTrackerId.empty()) g_nativeLiveTrackerClock = clockNow;
    return;
  }
  if (!advancing || playingId.empty()) {
    nativeFinalizeLiveTrackerLocked();
    return;
  }
  if (g_nativeLiveTrackerId != playingId) {
    nativeFinalizeLiveTrackerLocked();
    g_nativeLiveTrackerId = playingId;
    g_nativeLiveTrackerClock = clockNow;
    return;
  }
  if (g_nativeLiveTrackerClock.time_since_epoch().count() != 0) {
    const double delta = std::chrono::duration<double>(clockNow - g_nativeLiveTrackerClock).count();
    if (delta > 0.0 && delta < 30.0) g_nativeLiveTrackerElapsed += delta;
  }
  g_nativeLiveTrackerClock = clockNow;
}

static void nativeApplyLiveExecutedFlags(std::vector<NativeSongWindow>& songs)
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  for (auto& song : songs) {
    song.liveExecuted = g_nativeLiveMarkEnabled && !song.isBlock &&
      g_nativeLiveExecutedItems.find(song.id) != g_nativeLiveExecutedItems.end();
  }
}

static std::string nativeSongToJson(const NativeSongWindow& item, int index)
{
  const double duration = std::max(0.0, item.end - item.start);
  double elapsed = 0.0;
  double remaining = duration;
  bool playingThis = false;
  if (!item.isBlock && g_nativeCurrentTransportPlaying && !g_nativeCurrentPlayingId.empty() && g_nativeCurrentPlayingId == item.id) {
    elapsed = std::max(0.0, std::min(duration, g_nativeCurrentPlayPosition - item.start));
    remaining = std::max(0.0, duration - elapsed);
    playingThis = true;
  }
  const double progress = duration > 0.0 ? std::max(0.0, std::min(1.0, elapsed / duration)) : 0.0;
  std::ostringstream oss;
  oss << "{";
  oss << "\"id\":" << nativeJsonString(item.id) << ",";
  oss << "\"uid\":" << nativeJsonString(item.id + "|" + nativeNumber(item.start, 3) + "|" + nativeNumber(item.end, 3)) << ",";
  oss << "\"index\":" << index << ",";
  oss << "\"order\":" << (item.playlistOrder > 0 ? item.playlistOrder : index) << ",";
  oss << "\"playlistOrder\":" << (item.playlistOrder > 0 ? item.playlistOrder : index) << ",";
  oss << "\"playlistEntryId\":" << nativeJsonString(item.playlistEntryId.empty() ? item.id : item.playlistEntryId) << ",";
  oss << "\"name\":" << nativeJsonString(item.name) << ",";
  oss << "\"label\":" << nativeJsonString(item.name) << ",";
  oss << "\"type\":" << nativeJsonString(item.type.empty() ? (item.isBlock ? "block" : "song") : item.type) << ",";
  oss << "\"itemType\":" << nativeJsonString(item.type.empty() ? (item.isBlock ? "block" : "song") : item.type) << ",";
  oss << "\"isBlock\":" << (item.isBlock ? "true" : "false") << ",";
  oss << "\"isPlayable\":" << (!item.isBlock ? "true" : "false") << ",";
  oss << "\"source_number\":" << item.sourceNumber << ",";
  oss << "\"sourceNumber\":" << item.sourceNumber << ",";
  oss << "\"startPos\":" << nativeNumber(item.start) << ",";
  oss << "\"endPos\":" << nativeNumber(item.end) << ",";
  oss << "\"durationSec\":" << (int)std::floor(duration + 0.5) << ",";
  oss << "\"remainingSec\":" << nativeNumber(remaining) << ",";
  oss << "\"elapsedSec\":" << nativeNumber(elapsed) << ",";
  oss << "\"progress\":" << nativeNumber(progress, 6) << ",";
  oss << "\"playbackProgress\":" << nativeNumber(progress, 6) << ",";
  oss << "\"playing\":" << (playingThis ? "true" : "false") << ",";
  oss << "\"liveExecuted\":" << (item.liveExecuted ? "true" : "false") << ",";
  oss << "\"liveMarked\":" << (item.liveExecuted ? "true" : "false") << ",";
  oss << "\"isHashParent\":" << (item.isHashParent ? "true" : "false") << ",";
  oss << "\"isHashChild\":" << (item.isHashChild ? "true" : "false") << ",";
  oss << "\"isRegionChild\":" << (item.isRegionChild ? "true" : "false") << ",";
  oss << "\"sourceKind\":" << nativeJsonString(item.isRegionChild ? "region" : (item.isHashChild ? "hash_region_marker_song" : "region")) << ",";
  oss << "\"isFamilyItem\":" << ((item.isHashParent || item.isHashChild) ? "true" : "false") << ",";
  oss << "\"familyRole\":" << nativeJsonString(item.isHashParent ? "parent" : (item.isHashChild ? "child" : "none")) << ",";
  oss << "\"parentId\":" << nativeJsonString(item.parentId) << ",";
  oss << "\"parentName\":" << nativeJsonString(item.parentName) << ",";
  oss << "\"premixKey\":" << nativeJsonString(nativePremixSongKey(item)) << ",";
  oss << "\"premixRegionKey\":" << nativeJsonString(nativePremixSongKey(item)) << ",";
  oss << "\"tunerKey\":" << nativeJsonString(nativeTunerKey(item)) << ",";
  oss << "\"tunerValue\":" << item.tunerValue << ",";
  oss << "\"tunerAvailable\":" << (!item.isBlock && !item.isHashParent ? "true" : "false") << ",";
  oss << "\"tunerControllable\":" << (!item.isBlock && !item.isHashParent ? "true" : "false") << ",";
  oss << "\"blockColorKey\":" << nativeJsonString(item.blockColorKey) << ",";
  oss << "\"blockColorHex\":" << nativeJsonString(item.blockColorHex) << ",";
  oss << "\"bridgeBlockColorHex\":" << nativeJsonString(item.blockColorHex) << ",";
  oss << "\"inheritedBlockColorHex\":" << nativeJsonString(item.inheritedBlockColorHex) << ",";
  oss << "\"textColorHex\":" << nativeJsonString(item.inheritedBlockColorHex) << ",";
  oss << "\"finalTextColorHex\":" << nativeJsonString(item.inheritedBlockColorHex) << ",";
  oss << "\"lyricsText\":" << nativeJsonString(item.lyricsText) << ",";
  oss << "\"lyrics\":" << nativeJsonString(item.lyricsText) << ",";
  oss << "\"tp1LyricsText\":" << nativeJsonString(item.lyricsText) << ",";
  oss << "\"depth\":" << (item.isHashChild ? 1 : 0);
  oss << "}";
  return oss.str();
}

static std::string nativeGetProjExtStateString(ReaProject* project, const char* section, const char* key)
{
  if (!GetProjExtState_ptr) return std::string();
  std::string buffer(1024 * 1024, '\0');
  const int rv = GetProjExtState_ptr(project, section, key, buffer.data(), static_cast<int>(buffer.size()));
  if (rv == 0) return std::string();
  const size_t n = buffer.find('\0');
  if (n != std::string::npos) buffer.resize(n);
  return buffer;
}

static std::string nativeBuildProjectsJson(ReaProject* activeProject, std::string& activeProjectNameOut, std::string& activeProjectPathOut, int& activeIndexOut)
{
  std::ostringstream oss;
  oss << "[";
  bool first = true;
  activeIndexOut = 0;
  if (!EnumProjects_ptr) {
    oss << "]";
    return oss.str();
  }

  for (int i = 0; i < 64; ++i) {
    char pathBuf[2048] = "";
    ReaProject* project = EnumProjects_ptr(i, pathBuf, static_cast<int>(sizeof(pathBuf)));
    if (!project) break;
    const std::string projectPath = normalizeSlashes(pathBuf);
    std::string projectName = nativeBasenameNoRpp(projectPath);
    if (projectName.empty()) projectName = std::string("Projeto ") + std::to_string(i + 1);
    const bool active = project == activeProject;
    if (active) {
      activeIndexOut = i;
      activeProjectNameOut = projectName;
      activeProjectPathOut = projectPath;
    }
    if (!first) oss << ",";
    first = false;
    const std::string id = std::to_string(reinterpret_cast<std::uintptr_t>(project));
    oss << "{";
    oss << "\"id\":" << nativeJsonString(id) << ",";
    oss << "\"index\":" << i << ",";
    oss << "\"name\":" << nativeJsonString(projectName) << ",";
    oss << "\"projectName\":" << nativeJsonString(projectName) << ",";
    oss << "\"projectPath\":" << nativeJsonString(projectPath) << ",";
    oss << "\"active\":" << (active ? "true" : "false") << ",";
    oss << "\"isCurrent\":" << (active ? "true" : "false");
    oss << "}";
  }
  oss << "]";
  return oss.str();
}

// Mantem a descoberta do App disponivel sem acordar o motor completo.
// Este snapshot nao percorre tracks, markers, regioes, Parts, Premix ou Mixer;
// apenas enumera as abas de projeto e publica a autenticacao necessaria para a
// tela de entrada do Diretor. O heartbeat do app ativa o snapshot completo.
static void nativePublishStandbyDiscoveryState()
{
  if (!EnumProjects_ptr) return;

  char activePathBuf[2048] = "";
  ReaProject* activeProject = getCurrentProject(activePathBuf, static_cast<int>(sizeof(activePathBuf)));
  std::string activeProjectName;
  std::string activeProjectPath;
  int activeProjectIndex = 0;
  const std::string projectsJson = nativeBuildProjectsJson(
    activeProject, activeProjectName, activeProjectPath, activeProjectIndex);
  if (activeProjectName.empty()) activeProjectName = nativeBasenameNoRpp(activePathBuf);
  if (activeProjectPath.empty()) activeProjectPath = normalizeSlashes(activePathBuf);

  const std::string directorPassword = nativeReadDirectorPassword();
  const std::string directorAuthHash = nativeDirectorPasswordHash(directorPassword);
  const bool directorAuthEnabled = !directorAuthHash.empty();
  std::string directorAuthRevision;
  if (GetExtState_ptr) {
    const char* rawRevision = GetExtState_ptr(kNativeExtStateSection, kDirectorAuthRevisionKey);
    if (rawRevision && *rawRevision) directorAuthRevision = rawRevision;
  }

  const std::string nowIso = nativeIsoNow();
  std::ostringstream json;
  json << "{";
  json << "\"ok\":true,";
  json << "\"nativeBridge\":true,";
  json << "\"bridgeVersion\":2,";
  json << "\"connected\":true,";
  json << "\"standby\":true,";
  json << "\"runtimeControlActive\":false,";
  json << "\"app\":\"VS Hook\",";
  json << "\"appName\":\"VS Hook Diretor\",";
  json << "\"directorAuthEnabled\":" << (directorAuthEnabled ? "true" : "false") << ",";
  json << "\"authEnabled\":" << (directorAuthEnabled ? "true" : "false") << ",";
  json << "\"accessAuthEnabled\":" << (directorAuthEnabled ? "true" : "false") << ",";
  json << "\"directorAuthHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"authHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"accessAuthHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"directorPasswordHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"directorAuthRevision\":" << nativeJsonString(directorAuthRevision) << ",";
  json << "\"authRevision\":" << nativeJsonString(directorAuthRevision) << ",";
  json << "\"accessAuthRevision\":" << nativeJsonString(directorAuthRevision) << ",";
  json << "\"appActive\":false,";
  json << "\"directorAppActive\":false,";
  json << "\"directorActive\":false,";
  json << "\"luaControlActive\":false,";
  json << "\"updatedAt\":" << nativeJsonString(nowIso) << ",";
  json << "\"heartbeatAt\":" << nativeJsonString(nowIso) << ",";
  json << "\"projectName\":" << nativeJsonString(activeProjectName) << ",";
  json << "\"currentProjectName\":" << nativeJsonString(activeProjectName) << ",";
  json << "\"projectPath\":" << nativeJsonString(activeProjectPath) << ",";
  json << "\"projects\":" << projectsJson << ",";
  json << "\"projectTabs\":" << projectsJson << ",";
  json << "\"openProjects\":" << projectsJson << ",";
  json << "\"activeProjectTabIndex\":" << activeProjectIndex << ",";
  json << "\"regions\":[],\"markers\":[],\"playlists\":[]";
  json << "}";

  const std::string payload = json.str();
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeStateJson = payload;
  }
  if (SetExtState_ptr) {
    SetExtState_ptr(kNativeExtStateSection, "NATIVE_STATE_JSON_V1", payload.c_str(), false);
  }
}

static int nativeGetRegionRulerLaneNumber(ReaProject* project, int enumIndex)
{
  if (!project || enumIndex < 0 || !GetRegionOrMarker_ptr || !GetRegionOrMarkerInfo_Value_ptr) return -1;
  void* regionHandle = GetRegionOrMarker_ptr(project, enumIndex, "");
  if (!regionHandle) return -1;
  const double lane = GetRegionOrMarkerInfo_Value_ptr(project, regionHandle, "I_LANENUMBER");
  return std::isfinite(lane) ? static_cast<int>(std::floor(lane)) : -1;
}

static std::vector<NativeSongWindow> nativeCollectProjectSongs(ReaProject* project, std::string& markersJsonOut)
{
  std::vector<NativeSongWindow> songs;
  struct MarkerEntry { bool region=false; double start=0.0; double end=0.0; std::string name; int number=0; int color=0; int enumIndex=-1; int rulerLane=-1; };
  std::vector<MarkerEntry> regions;
  std::vector<MarkerEntry> markers;

  markersJsonOut = "[]";
  if (!CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return songs;

  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;

  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* name = "";
    int number = 0;
    int color = 0;
    const int ok = EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &name, &number, &color);
    if (!ok) continue;
    MarkerEntry entry;
    entry.region = isRegion;
    entry.start = pos;
    entry.end = isRegion ? end : pos;
    entry.name = name ? name : "";
    entry.number = number;
    entry.color = color;
    entry.enumIndex = i;
    entry.rulerLane = isRegion ? nativeGetRegionRulerLaneNumber(project, i) : -1;
    if (entry.name.rfind("!", 0) == 0) continue;
    if (isRegion) regions.push_back(entry); else markers.push_back(entry);
  }

  std::sort(regions.begin(), regions.end(), [](const MarkerEntry& a, const MarkerEntry& b){ return a.start < b.start; });
  std::sort(markers.begin(), markers.end(), [](const MarkerEntry& a, const MarkerEntry& b){ return a.start < b.start; });

  std::ostringstream markersJson;
  markersJson << "[";
  for (size_t i = 0; i < markers.size(); ++i) {
    if (i) markersJson << ",";
    markersJson << "{";
    markersJson << "\"id\":" << nativeJsonString(std::string("m") + std::to_string(markers[i].number)) << ",";
    markersJson << "\"number\":" << markers[i].number << ",";
    markersJson << "\"name\":" << nativeJsonString(nativeCleanSongName(markers[i].name)) << ",";
    markersJson << "\"label\":" << nativeJsonString(nativeCleanSongName(markers[i].name)) << ",";
    markersJson << "\"pos\":" << nativeNumber(markers[i].start) << ",";
    markersJson << "\"position\":" << nativeNumber(markers[i].start);
    markersJson << "}";
  }
  markersJson << "]";
  markersJsonOut = markersJson.str();

  const double familyEpsilon = 0.0005;
  const auto regionIsNestedChildOfParent = [familyEpsilon](const MarkerEntry& child, const MarkerEntry& parent) {
    if (!child.region || !parent.region || child.enumIndex == parent.enumIndex) return false;
    const double childLength = child.end - child.start;
    const double parentLength = parent.end - parent.start;
    if (childLength <= familyEpsilon || parentLength <= familyEpsilon) return false;
    const double overlap = std::min(child.end, parent.end) - std::max(child.start, parent.start);
    if (overlap <= familyEpsilon) return false;
    if (child.rulerLane >= 0 || parent.rulerLane >= 0) {
      if (child.rulerLane != 1 || parent.rulerLane != 0) return false;
      return (overlap / childLength) >= 0.50;
    }
    return (overlap / childLength) >= 0.50;
  };

  const auto regionHasLaneTwoChildren = [&](const MarkerEntry& parent) {
    if (parent.rulerLane != 0) return false;
    for (const auto& child : regions) {
      if (child.rulerLane == 1 && regionIsNestedChildOfParent(child, parent)) return true;
    }
    return false;
  };

  const auto regionIsFamilyChild = [&](const MarkerEntry& child) {
    if (child.rulerLane != 1) return false;
    for (const auto& parent : regions) {
      if (parent.rulerLane == 0 && regionIsNestedChildOfParent(child, parent)) return true;
    }
    return false;
  };

  for (const auto& r : regions) {
    // A faixa Musicas/Filhos aparece apenas dentro da gaveta da regiao-pai.
    if (regionIsFamilyChild(r)) continue;

    const std::string clean = nativeCleanSongName(r.name);
    if (clean.empty()) continue;
    const bool hasLegacyMarkerChildren = nativeTrim(r.name).rfind("--", 0) == 0;
    const bool isHashParent = hasLegacyMarkerChildren || regionHasLaneTwoChildren(r);
    NativeSongWindow parent;
    parent.id = std::to_string(r.number);
    parent.name = clean;
    parent.type = isHashParent ? "hash_parent" : "song";
    parent.start = r.start;
    parent.end = r.end;
    parent.sourceNumber = r.number;
    parent.isHashParent = isHashParent;
    parent.enumIndex = r.enumIndex;
    parent.premixNumber = r.number;
    songs.push_back(parent);

    if (hasLegacyMarkerChildren) {
      std::vector<MarkerEntry> inside;
      for (const auto& m : markers) {
        if (m.start >= r.start - 0.0005 && m.start < r.end - 0.0005) {
          if (!m.name.empty() && m.name.rfind("$", 0) != 0 && m.name.rfind("*", 0) != 0) inside.push_back(m);
        }
      }
      for (size_t i = 0; i < inside.size(); ++i) {
        const auto& m = inside[i];
        const double childEnd = (i + 1 < inside.size()) ? inside[i + 1].start : r.end;
        if (childEnd <= m.start + 0.0005) continue;
        NativeSongWindow child;
        child.id = std::to_string(m.number);
        child.name = nativeCleanSongName(m.name);
        child.type = "hash_child";
        child.start = m.start;
        child.end = childEnd;
        child.sourceNumber = m.number;
        child.isHashChild = true;
        child.parentId = std::to_string(r.number);
        child.parentName = clean;
        child.enumIndex = m.enumIndex;
        child.parentEnumIndex = r.enumIndex;
        const int markerUid = m.enumIndex >= 0 ? m.enumIndex : m.number;
        child.premixNumber = 900000 + std::max(0, markerUid);
        if (!child.name.empty()) songs.push_back(child);
      }
    }

    if (isHashParent) {
      for (const auto& childRegion : regions) {
        if (childRegion.rulerLane != 1 || !regionIsNestedChildOfParent(childRegion, r)) continue;
        NativeSongWindow child;
        child.id = std::to_string(childRegion.number);
        child.name = nativeCleanSongName(childRegion.name);
        child.type = "hash_child";
        child.start = childRegion.start;
        child.end = childRegion.end;
        child.sourceNumber = childRegion.number;
        child.isHashChild = true;
        child.isRegionChild = true;
        child.parentId = std::to_string(r.number);
        child.parentName = clean;
        child.enumIndex = childRegion.enumIndex;
        child.parentEnumIndex = r.enumIndex;
        child.premixNumber = childRegion.number;
        if (!child.name.empty() && child.end > child.start + familyEpsilon) songs.push_back(child);
      }
    }
  }

  std::sort(songs.begin(), songs.end(), [](const NativeSongWindow& a, const NativeSongWindow& b){
    if (a.start == b.start) {
      if (a.isHashParent != b.isHashParent) return a.isHashParent;
      if (a.isHashChild != b.isHashChild) return !a.isHashChild;
      return a.end < b.end;
    }
    return a.start < b.start;
  });

  // A extensão entrega a letra dos Empty Items direto para Diretor/Músicos.
  // O Lua não precisa mais escrever JSON pesado só para teleprompt/letras.
  for (auto& s : songs) {
    if (!s.isBlock) s.lyricsText = nativeCollectLyricsForRange(project, s.start, s.end);
  }
  return songs;
}


static std::string nativeUpperAscii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
  return value;
}

static MediaTrack* nativeFindTrackByExactName(ReaProject* project, const std::string& wantedName, int* trackIndexOut = nullptr)
{
  if (trackIndexOut) *trackIndexOut = -1;
  if (!project || !CountTracks_ptr || !GetTrack_ptr || !GetTrackName_ptr) return nullptr;
  const std::string wanted = nativeUpperAscii(nativeTrim(wantedName));
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* tr = GetTrack_ptr(project, i);
    if (!tr) continue;
    char nameBuf[512] = {0};
    if (!GetTrackName_ptr(tr, nameBuf, static_cast<int>(sizeof(nameBuf)))) continue;
    if (nativeUpperAscii(nativeTrim(nameBuf)) == wanted) {
      if (trackIndexOut) *trackIndexOut = i;
      return tr;
    }
  }
  return nullptr;
}

static std::string nativeReadMediaItemNotes(MediaItem* item)
{
  if (!item || !GetSetMediaItemInfo_String_ptr) return std::string();
  std::string buf(65536, '\0');
  if (!GetSetMediaItemInfo_String_ptr(item, "P_NOTES", buf.data(), false)) return std::string();
  const size_t n = buf.find('\0');
  if (n != std::string::npos) buf.resize(n);
  return nativeTrim(buf);
}

static std::string nativeReadTakeText(MediaItem_Take* take)
{
  if (!take) return std::string();
  if (GetSetMediaItemTakeInfo_String_ptr) {
    std::string buf(65536, '\0');
    if (GetSetMediaItemTakeInfo_String_ptr(take, "P_NAME", buf.data(), false)) {
      const size_t n = buf.find('\0');
      if (n != std::string::npos) buf.resize(n);
      buf = nativeTrim(buf);
      if (!buf.empty()) return buf;
    }
  }
  if (GetTakeName_ptr) {
    const char* name = GetTakeName_ptr(take);
    if (name && *name) return nativeTrim(name);
  }
  return std::string();
}

static std::string nativeReadItemTelepromptText(MediaItem* item)
{
  if (!item) return std::string();
  const std::string notes = nativeReadMediaItemNotes(item);
  if (!notes.empty()) return notes;
  MediaItem_Take* take = GetActiveTake_ptr ? GetActiveTake_ptr(item) : nullptr;
  return nativeReadTakeText(take);
}

static std::string nativeReadTakeSourcePath(MediaItem_Take* take)
{
  if (!take || !GetMediaItemTake_Source_ptr || !GetMediaSourceFileName_ptr) return std::string();
  PCM_source* source = GetMediaItemTake_Source_ptr(take);
  if (!source) return std::string();
  char buf[4096] = {0};
  GetMediaSourceFileName_ptr(source, buf, static_cast<int>(sizeof(buf)));
  return nativeTrim(std::string(buf));
}

static std::string nativeUrlEncode(const std::string& value)
{
  std::ostringstream out;
  out << std::uppercase << std::hex;
  for (unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out << static_cast<char>(c);
    } else {
      out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
  }
  return out.str();
}

static std::string nativeFileExtensionLower(const std::string& path)
{
  const size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= path.size()) return std::string();
  std::string ext = path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return ext;
}

static std::string nativeDetectTelepromptMediaType(const std::string& path)
{
  const std::string ext = nativeFileExtensionLower(path);
  if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif" || ext == "bmp") return "image";
  if (ext == "mp4" || ext == "mov" || ext == "m4v" || ext == "webm" || ext == "mkv" || ext == "avi") return "video";
  return "text";
}

static MediaItem* nativeFindCurrentItemOnTrack(MediaTrack* track, double pos, int* itemIndexOut = nullptr, double* itemStartOut = nullptr, double* itemEndOut = nullptr)
{
  if (itemIndexOut) *itemIndexOut = -1;
  if (itemStartOut) *itemStartOut = 0.0;
  if (itemEndOut) *itemEndOut = 0.0;
  if (!track || !GetTrackNumMediaItems_ptr || !GetTrackMediaItem_ptr || !GetMediaItemInfo_Value_ptr) return nullptr;
  const int count = GetTrackNumMediaItems_ptr(track);
  for (int i = 0; i < count; ++i) {
    MediaItem* item = GetTrackMediaItem_ptr(track, i);
    if (!item) continue;
    const double itemStart = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
    const double itemLen = std::max(0.0, GetMediaItemInfo_Value_ptr(item, "D_LENGTH"));
    const double itemEnd = itemStart + itemLen;
    if (pos >= itemStart - 0.0005 && pos < itemEnd - 0.0005) {
      if (itemIndexOut) *itemIndexOut = i;
      if (itemStartOut) *itemStartOut = itemStart;
      if (itemEndOut) *itemEndOut = itemEnd;
      return item;
    }
  }
  return nullptr;
}

static std::string nativeFindSongNameAtPosition(const std::vector<NativeSongWindow>& songs, double pos, const std::string& fallback)
{
  for (const auto& s : songs) {
    if (s.isBlock) continue;
    if (pos >= s.start - 0.0005 && pos < s.end - 0.0005 && !s.name.empty()) return s.name;
  }
  return fallback;
}

static std::string nativeBuildTelepromptStateJson(ReaProject* project, const std::vector<NativeSongWindow>& songs, int slot, const std::string& trackName, bool transportPlaying, double playPos, const std::string& playingName)
{
  const double pos = (transportPlaying && GetPlayPositionEx_ptr) ? playPos : (GetCursorPositionEx_ptr ? GetCursorPositionEx_ptr(project) : playPos);
  int trackIndex = -1;
  MediaTrack* track = nativeFindTrackByExactName(project, trackName, &trackIndex);

  // FIX105: resolve todos os itens ativos na mesma posição da pista TELEPROMPT.
  // Regra de prioridade na mesma posição:
  // 1) texto/letra continua visível por cima;
  // 2) vídeo ganha de imagem;
  // 3) imagem entra se não houver vídeo.
  bool itemFound = false;
  int chosenItemIndex = -1;
  double chosenItemStart = 0.0;
  double chosenItemEnd = 0.0;

  int chosenMediaPriority = 0; // 0 nenhum, 1 imagem, 2 vídeo
  int chosenMediaIndex = -1;
  double chosenMediaStart = 0.0;
  double chosenMediaEnd = 0.0;
  double chosenMediaOffset = 0.0;
  double chosenMediaPlayrate = 1.0;
  std::string mediaPath;
  std::string mediaType = "text";
  std::string mediaExt;

  std::vector<std::string> textParts;
  int chosenTextIndex = -1;
  double chosenTextStart = 0.0;
  double chosenTextEnd = 0.0;

  auto appendTextPart = [&](const std::string& value) {
    const std::string clean = nativeTrim(value);
    if (clean.empty()) return;
    for (const auto& existing : textParts) {
      if (existing == clean) return;
    }
    textParts.push_back(clean);
  };

  auto labelLooksLikePath = [](const std::string& value) -> bool {
    return value.find('/') != std::string::npos || value.find('\\') != std::string::npos || value.find(':') != std::string::npos;
  };

  if (track && GetTrackNumMediaItems_ptr && GetTrackMediaItem_ptr && GetMediaItemInfo_Value_ptr) {
    const int count = GetTrackNumMediaItems_ptr(track);
    for (int i = 0; i < count; ++i) {
      MediaItem* currentItem = GetTrackMediaItem_ptr(track, i);
      if (!currentItem) continue;
      const double itemStart = GetMediaItemInfo_Value_ptr(currentItem, "D_POSITION");
      const double itemLen = std::max(0.0, GetMediaItemInfo_Value_ptr(currentItem, "D_LENGTH"));
      const double itemEnd = itemStart + itemLen;
      if (pos < itemStart - 0.0005 || pos >= itemEnd - 0.0005) continue;

      itemFound = true;
      if (chosenItemIndex < 0) {
        chosenItemIndex = i;
        chosenItemStart = itemStart;
        chosenItemEnd = itemEnd;
      }

      MediaItem_Take* currentTake = currentItem && GetActiveTake_ptr ? GetActiveTake_ptr(currentItem) : nullptr;
      std::string currentMediaPath = nativeReadTakeSourcePath(currentTake);
      const std::string takeLabel = nativeReadTakeText(currentTake);
      if (currentMediaPath.empty() && !takeLabel.empty() && labelLooksLikePath(takeLabel)) {
        currentMediaPath = takeLabel;
      }

      const std::string currentMediaType = currentMediaPath.empty() ? std::string("text") : nativeDetectTelepromptMediaType(currentMediaPath);
      const int currentPriority = currentMediaType == "video" ? 2 : (currentMediaType == "image" ? 1 : 0);

      std::string currentText = nativeReadItemTelepromptText(currentItem);
      const bool textLooksOnlyLikeMediaFileName = !currentText.empty()
        && currentText.find('/') == std::string::npos
        && currentText.find('\\') == std::string::npos
        && currentText.find(':') == std::string::npos
        && (nativeDetectTelepromptMediaType(currentText) == "image" || nativeDetectTelepromptMediaType(currentText) == "video");
      if (textLooksOnlyLikeMediaFileName) currentText.clear();
      if (!nativeTrim(currentText).empty()) {
        appendTextPart(currentText);
        if (chosenTextIndex < 0) {
          chosenTextIndex = i;
          chosenTextStart = itemStart;
          chosenTextEnd = itemEnd;
        }
      }

      if (currentPriority > 0 && (currentPriority > chosenMediaPriority || (currentPriority == chosenMediaPriority && i >= chosenMediaIndex))) {
        chosenMediaPriority = currentPriority;
        chosenMediaIndex = i;
        chosenMediaStart = itemStart;
        chosenMediaEnd = itemEnd;
        mediaPath = currentMediaPath;
        mediaType = currentMediaType;
        mediaExt = nativeFileExtensionLower(mediaPath);
        chosenMediaOffset = 0.0;
        chosenMediaPlayrate = 1.0;
        if (currentTake && GetMediaItemTakeInfo_Value_ptr) {
          chosenMediaOffset = std::max(0.0, GetMediaItemTakeInfo_Value_ptr(currentTake, "D_STARTOFFS"));
          chosenMediaPlayrate = GetMediaItemTakeInfo_Value_ptr(currentTake, "D_PLAYRATE");
          if (chosenMediaPlayrate <= 0.0) chosenMediaPlayrate = 1.0;
        }
      }
    }
  }

  if (chosenMediaPriority > 0) {
    chosenItemIndex = chosenMediaIndex;
    chosenItemStart = chosenMediaStart;
    chosenItemEnd = chosenMediaEnd;
  } else if (chosenTextIndex >= 0) {
    chosenItemIndex = chosenTextIndex;
    chosenItemStart = chosenTextStart;
    chosenItemEnd = chosenTextEnd;
    mediaType = "text";
    mediaPath.clear();
    mediaExt.clear();
  }

  std::string text;
  for (size_t i = 0; i < textParts.size(); ++i) {
    if (i > 0) text += "\n";
    text += textParts[i];
  }

  const double itemLength = std::max(0.0, chosenItemEnd - chosenItemStart);
  double mediaCurrentTime = 0.0;
  if (chosenMediaPriority > 0) {
    mediaCurrentTime = chosenMediaOffset + (std::max(0.0, pos - chosenMediaStart) * chosenMediaPlayrate);
  }
  const std::string songName = nativeFindSongNameAtPosition(songs, pos, playingName);
  std::ostringstream out;
  out << "{";
  out << "\"source\":\"vs_hook_extension\",";
  out << "\"slot\":" << slot << ",";
  out << "\"updatedAt\":" << nativeJsonString(nativeIsoNow()) << ",";
  out << "\"song\":" << nativeJsonString(songName) << ",";
  out << "\"songName\":" << nativeJsonString(songName) << ",";
  out << "\"currentSongName\":" << nativeJsonString(songName) << ",";
  out << "\"trackName\":" << nativeJsonString(trackName) << ",";
  out << "\"trackFound\":" << (track ? "true" : "false") << ",";
  out << "\"trackIndex\":" << trackIndex << ",";
  out << "\"itemFound\":" << (itemFound ? "true" : "false") << ",";
  out << "\"itemIndex\":" << chosenItemIndex << ",";
  out << "\"itemGuid\":\"\",";
  out << "\"itemStart\":" << nativeNumber(chosenItemStart) << ",";
  out << "\"itemEnd\":" << nativeNumber(chosenItemEnd) << ",";
  out << "\"position\":" << nativeNumber(pos) << ",";
  out << "\"playing\":" << (transportPlaying ? "true" : "false") << ",";
  out << "\"telepromptType\":" << nativeJsonString(mediaType) << ",";
  out << "\"mediaType\":" << nativeJsonString(mediaType) << ",";
  out << "\"mediaPath\":" << nativeJsonString(mediaPath) << ",";
  out << "\"mediaUrl\":" << nativeJsonString(mediaPath.empty() ? std::string() : std::string("/media?path=") + nativeUrlEncode(mediaPath)) << ",";
  out << "\"mediaExt\":" << nativeJsonString(mediaExt) << ",";
  out << "\"mediaCurrentTime\":" << nativeNumber(mediaCurrentTime) << ",";
  out << "\"mediaOffset\":" << nativeNumber(chosenMediaOffset) << ",";
  out << "\"mediaPlayrate\":" << nativeNumber(chosenMediaPlayrate) << ",";
  out << "\"itemLength\":" << nativeNumber(itemLength) << ",";
  out << "\"overlayText\":" << nativeJsonString(text) << ",";
  out << "\"lyricsText\":" << nativeJsonString(text) << ",";
  out << "\"lyrics\":" << nativeJsonString(text);
  out << "}";
  return out.str();
}

static std::string nativeBuildRegionsJson(const std::vector<NativeSongWindow>& songs)
{
  std::ostringstream oss;
  oss << "[";
  bool first = true;
  int index = 1;
  for (const auto& s : songs) {
    if (s.isBlock) continue;
    if (!first) oss << ",";
    first = false;
    NativeSongWindow copy = s; if (copy.playlistOrder <= 0) copy.playlistOrder = index; oss << nativeSongToJson(copy, index++);
  }
  oss << "]";
  return oss.str();
}

static double nativeRawDurationSec(const NativeSongWindow& item)
{
  return std::max(0.0, item.end - item.start);
}

static double nativeComputeRegionsTotalSec(const std::vector<NativeSongWindow>& songs)
{
  double total = 0.0;
  for (const auto& s : songs) {
    if (s.isBlock || s.isHashChild) continue;
    total += nativeRawDurationSec(s);
  }
  return total;
}

static double nativeComputePlaylistTotalSec(const std::vector<NativeSongWindow>& items)
{
  double total = 0.0;
  for (const auto& item : items) {
    // Mesma regra do Lua: blocos não contam e qualquer filho de região "--"
    // é ignorado, porque o tempo do pai já cobre os filhos.
    if (item.isBlock || item.isHashChild) continue;
    total += nativeRawDurationSec(item);
  }
  return total;
}

static std::string nativeFormatTotalTextFromSeconds(double seconds)
{
  if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
  const long long total = static_cast<long long>(std::floor(seconds + 0.5));
  const long long h = total / 3600;
  const int m = static_cast<int>((total % 3600) / 60);
  const int sec = static_cast<int>(total % 60);
  char buf[32] = "";
  std::snprintf(buf, sizeof(buf), "%02lld:%02d:%02d", h, m, sec);
  return std::string(buf);
}

static std::vector<double> nativeComputeStoredPlaylistTotals(const std::string& data)
{
  std::vector<double> totals;
  int playlistIndex = -1;
  const auto lines = nativeSplit(data, '\n');
  for (const std::string& rawLine : lines) {
    if (rawLine.empty()) continue;
    const auto fields = nativeSplit(rawLine, '\t');
    if (fields.empty()) continue;
    const std::string tag = fields[0];
    if (tag == "PLAYLIST") {
      totals.push_back(0.0);
      playlistIndex = static_cast<int>(totals.size()) - 1;
      continue;
    }
    if (tag != "ITEM" || playlistIndex < 0) continue;

    const std::string itemType = fields.size() > 1 ? fields[1] : "";
    const int sourceNumber = fields.size() > 2 ? std::atoi(fields[2].c_str()) : 0;
    const double startPos = fields.size() > 3 ? std::atof(fields[3].c_str()) : 0.0;
    const double endPos = fields.size() > 4 ? std::atof(fields[4].c_str()) : 0.0;
    const bool storedHashChild = fields.size() > 9 && fields[9] == "1";
    const bool isBlock = itemType == "block" || sourceNumber < 0;

    // Mesma fonte da aba Repertórios do Lua: usa os limites gravados no
    // repertório e arredonda apenas o total final.
    if (!isBlock && !storedHashChild) {
      totals[static_cast<size_t>(playlistIndex)] += std::max(0.0, endPos - startPos);
    }
  }
  return totals;
}

static std::string nativeBuildPlaylistsJson(ReaProject* project, const std::vector<NativeSongWindow>& projectSongs, int& activePlaylistIndexOut, std::string& activePlaylistNameOut, std::vector<NativeSongWindow>* activeItemsOut = nullptr, double* activeStoredTotalOut = nullptr)
{
  activePlaylistIndexOut = 1;
  activePlaylistNameOut = "Músicas";
  if (activeItemsOut) activeItemsOut->clear();
  if (activeStoredTotalOut) *activeStoredTotalOut = 0.0;
  const std::string savedPlaylistName = nativeTrim(nativeGetProjExtStateString(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1"));
  const std::string data = nativeGetProjExtStateString(project, kPlaylistExtSection, kPlaylistExtKey);
  if (data.empty()) {
    if (activeItemsOut) {
      int fallbackIndex = 1;
      for (const auto& src : projectSongs) {
        if (src.isBlock) continue;
        NativeSongWindow item = src;
        item.playlistOrder = fallbackIndex;
        item.playlistEntryId = std::string("1:") + std::to_string(fallbackIndex) + ":" + item.id;
        activeItemsOut->push_back(item);
        ++fallbackIndex;
      }
    }
    const double fallbackTotalSec = nativeComputeRegionsTotalSec(projectSongs);
    if (activeStoredTotalOut) *activeStoredTotalOut = fallbackTotalSec;
    std::ostringstream fallback;
    fallback << "[{\"id\":\"1\",\"name\":\"Músicas\",\"active\":true,\"current\":true,"
             << "\"activePlaylistTotalSec\":" << nativeNumber(fallbackTotalSec) << ","
             << "\"playlistTotalSec\":" << nativeNumber(fallbackTotalSec) << ","
             << "\"totalSec\":" << nativeNumber(fallbackTotalSec) << ","
             << "\"activePlaylistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(fallbackTotalSec)) << ","
             << "\"playlistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(fallbackTotalSec)) << ","
             << "\"totalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(fallbackTotalSec)) << ","
             << "\"songs\":" << nativeBuildRegionsJson(projectSongs) << "}]";
    return fallback.str();
  }

  const std::vector<double> storedPlaylistTotals = nativeComputeStoredPlaylistTotals(data);
  if (activeStoredTotalOut && !storedPlaylistTotals.empty()) *activeStoredTotalOut = storedPlaylistTotals.front();

  auto findActualSong = [&](int primary, int fallback, bool childWanted) -> const NativeSongWindow* {
    if (primary != 0) {
      for (const auto& s : projectSongs) {
        if (s.sourceNumber == primary && (!childWanted || s.isHashChild)) return &s;
      }
      for (const auto& s : projectSongs) {
        if (s.sourceNumber == primary) return &s;
      }
    }
    if (fallback != 0) {
      for (const auto& s : projectSongs) {
        if (s.sourceNumber == fallback && !s.isHashChild) return &s;
      }
    }
    return nullptr;
  };

  std::ostringstream out;
  out << "[";
  bool firstPlaylist = true;
  int playlistIndex = 0;
  bool inPlaylist = false;
  int itemIndex = 0;
  std::string currentBlockColorHex;
  bool currentPlaylistActive = false;

  const auto lines = nativeSplit(data, '\n');
  for (const std::string& rawLine : lines) {
    if (rawLine.empty()) continue;
    const auto fields = nativeSplit(rawLine, '\t');
    if (fields.empty()) continue;
    const std::string tag = fields[0];
    if (tag == "PLAYLIST") {
      if (inPlaylist) out << "]}";
      if (!firstPlaylist) out << ",";
      firstPlaylist = false;
      inPlaylist = true;
      itemIndex = 0;
      currentBlockColorHex.clear();
      ++playlistIndex;
      const std::string name = fields.size() > 1 ? nativeUnescapeExtField(fields[1]) : std::string("Repertório ") + std::to_string(playlistIndex);
      const bool activeThis = (savedPlaylistName.empty() && playlistIndex == 1) || (!savedPlaylistName.empty() && nativeTrim(name) == savedPlaylistName);
      const double storedTotalSec = (playlistIndex > 0 && static_cast<size_t>(playlistIndex - 1) < storedPlaylistTotals.size())
        ? storedPlaylistTotals[static_cast<size_t>(playlistIndex - 1)]
        : 0.0;
      currentPlaylistActive = activeThis;
      if (activeThis) {
        activePlaylistIndexOut = playlistIndex;
        activePlaylistNameOut = name;
        if (activeStoredTotalOut) *activeStoredTotalOut = storedTotalSec;
      }
      out << "{\"id\":" << nativeJsonString(std::to_string(playlistIndex))
          << ",\"name\":" << nativeJsonString(name)
          << ",\"active\":" << (activeThis ? "true" : "false")
          << ",\"current\":" << (activeThis ? "true" : "false")
          << ",\"activePlaylistTotalSec\":" << nativeNumber(storedTotalSec)
          << ",\"playlistTotalSec\":" << nativeNumber(storedTotalSec)
          << ",\"totalSec\":" << nativeNumber(storedTotalSec)
          << ",\"activePlaylistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(storedTotalSec))
          << ",\"playlistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(storedTotalSec))
          << ",\"totalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(storedTotalSec))
          << ",\"songs\":[";
    } else if (tag == "ITEM" && inPlaylist) {
      const std::string itemType = fields.size() > 1 ? fields[1] : "";
      const int sourceNumber = fields.size() > 2 ? std::atoi(fields[2].c_str()) : 0;
      const double startPos = fields.size() > 3 ? std::atof(fields[3].c_str()) : 0.0;
      const double endPos = fields.size() > 4 ? std::atof(fields[4].c_str()) : 0.0;
      const std::string name = fields.size() > 5 ? nativeUnescapeExtField(fields[5]) : "";
      const std::string blockColorKey = fields.size() > 6 ? nativeUnescapeExtField(fields[6]) : "";
      const bool storedHashParent = fields.size() > 8 && fields[8] == "1";
      const bool storedHashChild = fields.size() > 9 && fields[9] == "1";
      const int markerNumber = fields.size() > 10 ? std::atoi(fields[10].c_str()) : 0;
      const bool isBlock = itemType == "block" || sourceNumber < 0;

      NativeSongWindow item;
      if (isBlock) {
        item.id = std::to_string(sourceNumber);
        item.name = name.empty() ? (std::string("BLOCO ") + std::to_string(std::abs(sourceNumber))) : name;
        item.type = "block";
        item.isBlock = true;
        item.sourceNumber = sourceNumber;
        int blockNumber = std::abs(sourceNumber);
        if (blockNumber <= 0) {
          const size_t p = item.name.find("BLOCO");
          blockNumber = (p != std::string::npos) ? std::atoi(item.name.c_str() + p + 5) : (itemIndex + 1);
        }
        item.blockColorKey = blockColorKey;
        item.blockColorHex = nativeBlockColorHexFromKey(blockColorKey, blockNumber);
        currentBlockColorHex = item.blockColorHex;
      } else {
        const NativeSongWindow* actual = findActualSong(storedHashChild && markerNumber ? markerNumber : sourceNumber, sourceNumber, storedHashChild);
        // Se era filho antigo e o marker nao existe mais, nao envia filho fantasma para o app.
        if (!actual) continue;
        item = *actual;
        item.inheritedBlockColorHex = currentBlockColorHex;
        // Se a região deixou de ser pai no projeto, força tratamento como música solta.
        if (!actual->isHashParent && storedHashParent) {
          item.isHashParent = false;
          item.isHashChild = false;
          item.type = "song";
          item.parentId.clear();
          item.parentName.clear();
        }
        // Se era filho salvo mas o projeto atual já não traz esse filho, não mantém flag velha.
        if (!actual->isHashChild && storedHashChild) {
          item.isHashChild = false;
          item.parentId.clear();
          item.parentName.clear();
          item.type = actual->isHashParent ? "hash_parent" : "song";
        }
        (void)startPos; (void)endPos;
      }

      item.playlistOrder = itemIndex + 1;
      item.playlistEntryId = std::to_string(playlistIndex) + ":" + std::to_string(itemIndex + 1) + ":" + item.id;
      if (currentPlaylistActive && activeItemsOut) activeItemsOut->push_back(item);
      if (itemIndex > 0) out << ",";
      out << nativeSongToJson(item, itemIndex + 1);
      ++itemIndex;
    } else if (tag == "END" && inPlaylist) {
      out << "]}";
      inPlaylist = false;
      currentPlaylistActive = false;
    }
  }
  if (inPlaylist) out << "]}";
  out << "]";
  return out.str();
}


static bool nativeLooksNumeric(const std::string& value);

static std::string nativeResolvePlaylistNameByIndex(ReaProject* project, int targetIndex)
{
  if (!project || targetIndex <= 0) return std::string();
  const std::string data = nativeGetProjExtStateString(project, kPlaylistExtSection, kPlaylistExtKey);
  if (data.empty()) return targetIndex == 1 ? std::string("Músicas") : std::string();

  int playlistIndex = 0;
  const auto lines = nativeSplit(data, '\n');
  for (const std::string& rawLine : lines) {
    if (rawLine.empty()) continue;
    const auto fields = nativeSplit(rawLine, '\t');
    if (fields.empty() || fields[0] != "PLAYLIST") continue;
    ++playlistIndex;
    if (playlistIndex != targetIndex) continue;
    return fields.size() > 1
      ? nativeTrim(nativeUnescapeExtField(fields[1]))
      : std::string("Repertório ") + std::to_string(playlistIndex);
  }
  return std::string();
}

static bool nativeApplyPlaylistCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "select_playlist" && type != "playlist_select" && type != "set_playlist" && type != "set_active_playlist") return false;
  if (!SetProjExtState_ptr) return false;

  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return false;

  std::string playlistName = nativeJsonExtractString(commandBody, "playlistName");
  if (playlistName.empty()) playlistName = nativeJsonExtractString(commandBody, "activePlaylistName");
  if (playlistName.empty()) playlistName = nativeJsonExtractString(commandBody, "currentPlaylistName");
  if (playlistName.empty()) playlistName = nativeJsonExtractString(commandBody, "name");
  playlistName = nativeTrim(playlistName);

  std::string playlistId = nativeJsonExtractString(commandBody, "playlistId");
  if (playlistId.empty()) playlistId = nativeJsonExtractString(commandBody, "activePlaylistId");
  if (playlistId.empty()) playlistId = nativeJsonExtractString(commandBody, "targetId");
  if (playlistId.empty()) playlistId = nativeJsonExtractString(commandBody, "id");
  const int playlistIndex = nativeLooksNumeric(playlistId) ? std::atoi(playlistId.c_str()) : 0;

  if (playlistName.empty()) playlistName = nativeResolvePlaylistNameByIndex(project, playlistIndex);
  if (playlistName.empty()) return true;

  SetProjExtState_ptr(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1", playlistName.c_str());

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    // A selecao visual antiga nao pode prender o app no repertorio anterior.
    g_nativeSelectedId.clear();
    g_nativeSelectedTab.clear();
    g_nativeSelectedStart = 0.0;
    g_nativeSelectedEnd = 0.0;
  }

  g_nativeForceSnapshotBuild.store(true);
  g_nativeForceStateBuild.store(true);
  return true;
}

static std::string nativeFindPlayingId(const std::vector<NativeSongWindow>& songs, double pos, std::string& nameOut, double& startOut, double& endOut)
{
  std::string id;
  double bestLen = 1e99;
  bool bestIsChild = false;
  for (const auto& s : songs) {
    if (s.isBlock) continue;
    if (pos >= s.start - 0.0005 && pos < s.end - 0.0005) {
      const double len = std::max(0.0, s.end - s.start);
      // Gaveta aberta/fechada nao participa desta escolha. Prioriza sempre a
      // regiao-filha real; o menor trecho continua sendo o desempate geral.
      const bool betterLength = len < bestLen - 0.0005;
      const bool childTieBreak = std::fabs(len - bestLen) <= 0.0005 && s.isHashChild && !bestIsChild;
      if (betterLength || childTieBreak) {
        bestLen = len;
        bestIsChild = s.isHashChild;
        id = s.id;
        nameOut = s.name;
        startOut = s.start;
        endOut = s.end;
      }
    }
  }
  return id;
}

static std::string nativeStripJsonObjectBraces(std::string json)
{
  json = nativeTrim(json);
  if (json.size() >= 2 && json.front() == '{' && json.back() == '}') {
    json = json.substr(1, json.size() - 2);
  }
  json = nativeTrim(json);
  return json;
}


static const NativeSongWindow* nativeFindSongById(const std::vector<NativeSongWindow>& songs, const std::string& id)
{
  const std::string wanted = nativeTrim(id);
  if (wanted.empty()) return nullptr;
  for (const auto& s : songs) {
    if (s.id == wanted || std::to_string(s.sourceNumber) == wanted) return &s;
  }
  return nullptr;
}

static bool nativeLooksNumeric(const std::string& value);

static bool nativeBoolFromText(std::string value, bool fallback)
{
  value = nativeTrim(value);
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  if (value == "true" || value == "1" || value == "on" || value == "yes" || value == "playing" || value == "play") return true;
  if (value == "false" || value == "0" || value == "off" || value == "no" || value == "stopped" || value == "stop") return false;
  return fallback;
}

static bool nativeReadStopPauseModeEnabled()
{
  if (!GetExtState_ptr) return g_stopPauseModeEnabled.load();
  const char* raw = GetExtState_ptr(kNativeExtStateSection, kStopPauseModeEnabledKey);
  return nativeBoolFromText(raw ? raw : "", false);
}

static void nativeRefreshStopPauseModeFromExtState()
{
  g_stopPauseModeEnabled.store(nativeReadStopPauseModeEnabled());
}

static void nativeWriteStopPauseModeEnabled(bool enabled)
{
  g_stopPauseModeEnabled.store(enabled);
  if (SetExtState_ptr) {
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    SetExtState_ptr(kNativeExtStateSection, kStopPauseModeEnabledKey, enabled ? "1" : "0", true);
    SetExtState_ptr(kNativeExtStateSection, kStopPauseModeRevisionKey, std::to_string(stamp).c_str(), true);
  }
  {
    // Stop/Pause participa da mesma revisao de retomada usada por Auto,
    // AT/BL e Auto Stop. Assim o Lua recebe o valor do Diretor antes de abrir.
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeBumpSharedRevisionLocked("director");
  }
  g_nativeForceStateBuild.store(true);
}

static int nativeManualStopFadeoutClampDuration(const std::string& value)
{
  int duration = nativeLooksNumeric(nativeTrim(value)) ? std::atoi(value.c_str()) : 3;
  if (duration < 1) duration = 1;
  if (duration > 5) duration = 5;
  return duration;
}

static std::string nativeReadLuaWindowExtState(const char* key)
{
  if (!GetExtState_ptr || !key) return std::string();
  const char* raw = GetExtState_ptr(kLuaWindowExtStateSection, key);
  return raw ? std::string(raw) : std::string();
}

static std::vector<std::string> nativeManualStopFadeoutTrackIds()
{
  std::vector<std::string> out;
  std::string raw;
  bool projectValueExists = false;
  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  if (project && GetProjExtState_ptr) {
    std::vector<char> buffer(262144, '\0');
    const int rv = GetProjExtState_ptr(project, kManualStopFadeoutProjectSection, kManualStopFadeoutTracksKey, buffer.data(), static_cast<int>(buffer.size()));
    if (rv != 0) { projectValueExists = true; raw = buffer.data(); }
  }
  if (!projectValueExists) raw = nativeReadLuaWindowExtState(kManualStopFadeoutTracksKey);
  size_t begin = 0;
  while (begin <= raw.size()) {
    const size_t end = raw.find(';', begin);
    const std::string id = nativeTrim(raw.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
    if (!id.empty() && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return out;
}

static void nativeWriteManualStopFadeoutTrackIds(const std::vector<std::string>& ids)
{
  if (!SetProjExtState_ptr) return;
  std::ostringstream joined;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (i) joined << ";";
    joined << ids[i];
  }
  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  if (!project) return;
  SetProjExtState_ptr(project, kManualStopFadeoutProjectSection, kManualStopFadeoutTracksKey, joined.str().c_str());
  if (MarkProjectDirty_ptr) MarkProjectDirty_ptr(project);
}

static void nativeBumpManualStopFadeoutRevision()
{
  if (!SetExtState_ptr) return;
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  SetExtState_ptr(kLuaWindowExtStateSection, kManualStopFadeoutRevisionKey, std::to_string(stamp).c_str(), true);
}

static std::string nativeBuildManualStopFadeoutJson()
{
  const bool enabled = nativeBoolFromText(nativeReadLuaWindowExtState(kManualStopFadeoutEnabledKey), false);
  const int duration = nativeManualStopFadeoutClampDuration(nativeReadLuaWindowExtState(kManualStopFadeoutDurationKey));
  const std::vector<std::string> selected = nativeManualStopFadeoutTrackIds();
  bool active = false;
  bool restorePending = false;
  double progress = 0.0;
  {
    std::lock_guard<std::mutex> lock(g_nativeManualStopFadeoutMutex);
    active = g_nativeManualStopFadeoutRuntime.active;
    restorePending = g_nativeManualStopFadeoutRuntime.restorePending;
    progress = g_nativeManualStopFadeoutRuntime.progress;
  }
  std::ostringstream json;
  json << "{";
  json << "\"enabled\":" << (enabled ? "true" : "false") << ",";
  json << "\"active\":" << (active ? "true" : "false") << ",";
  json << "\"fading\":" << (active ? "true" : "false") << ",";
  json << "\"restorePending\":" << (restorePending ? "true" : "false") << ",";
  json << "\"progress\":" << nativeNumber(std::max(0.0, std::min(1.0, progress)), 4) << ",";
  json << "\"remainingRatio\":" << nativeNumber(1.0 - std::max(0.0, std::min(1.0, progress)), 4) << ",";
  json << "\"duration\":" << duration << ",";
  json << "\"durationSec\":" << duration << ",";
  json << "\"selectedCount\":" << selected.size() << ",";
  json << "\"selectedTrackIds\":[";
  for (size_t i = 0; i < selected.size(); ++i) {
    if (i) json << ",";
    json << nativeJsonString(selected[i]);
  }
  json << "]}";
  return json.str();
}

static bool nativeApplyManualStopFadeoutCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  const bool isGenericSet = type == "manual_stop_fadeout_set" || type == "faderout_set";
  if (!isGenericSet && type != "manual_stop_fadeout_set_enabled" && type != "manual_stop_fadeout_set_duration" &&
      type != "manual_stop_fadeout_toggle_track" && type != "manual_stop_fadeout_set_all_tracks") return false;
  if (!SetExtState_ptr) return false;

  bool changed = false;
  if (isGenericSet || type == "manual_stop_fadeout_set_enabled") {
    std::string value = nativeJsonExtractString(commandBody, "enabled");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "desiredState");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "fadeoutEnabled");
    if (!value.empty()) {
      const bool current = nativeBoolFromText(nativeReadLuaWindowExtState(kManualStopFadeoutEnabledKey), false);
      const bool enabled = nativeBoolFromText(value, current);
      SetExtState_ptr(kLuaWindowExtStateSection, kManualStopFadeoutEnabledKey, enabled ? "1" : "0", true);
      changed = true;
    }
  }
  if (isGenericSet || type == "manual_stop_fadeout_set_duration") {
    std::string value = nativeJsonExtractString(commandBody, "duration");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "durationSec");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "seconds");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "fadeoutSeconds");
    if (!value.empty()) {
      const int duration = nativeManualStopFadeoutClampDuration(value);
      SetExtState_ptr(kLuaWindowExtStateSection, kManualStopFadeoutDurationKey, std::to_string(duration).c_str(), true);
      changed = true;
    }
  }
  if (type == "manual_stop_fadeout_toggle_track") {
    std::string id = nativeJsonExtractString(commandBody, "trackId");
    if (id.empty()) id = nativeJsonExtractString(commandBody, "guid");
    if (id.empty()) id = nativeJsonExtractString(commandBody, "targetId");
    id = nativeTrim(id);
    if (!id.empty()) {
      std::vector<std::string> ids = nativeManualStopFadeoutTrackIds();
      const auto it = std::find(ids.begin(), ids.end(), id);
      if (it == ids.end()) ids.push_back(id); else ids.erase(it);
      nativeWriteManualStopFadeoutTrackIds(ids);
      changed = true;
    }
  }
  if (type == "manual_stop_fadeout_set_all_tracks") {
    const bool select = nativeBoolFromText(nativeJsonExtractString(commandBody, "selected"),
      nativeBoolFromText(nativeJsonExtractString(commandBody, "enabled"), true));
    std::vector<std::string> ids;
    if (select && CountTracks_ptr && GetTrack_ptr) {
      char pathBuf[2048] = "";
      ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
      const int count = CountTracks_ptr(project);
      for (int i = 0; i < count; ++i) {
        MediaTrack* track = GetTrack_ptr(project, i);
        const std::string id = nativeTrackGuid(track, i);
        if (!id.empty()) ids.push_back(id);
      }
    }
    nativeWriteManualStopFadeoutTrackIds(ids);
    changed = true;
  }

  if (changed) {
    nativeBumpManualStopFadeoutRevision();
    g_nativeForceStateBuild.store(true);
  }
  return true;
}

static bool nativeSongIsPlayable(const NativeSongWindow& item)
{
  return !item.isBlock && !item.id.empty() && item.end > item.start + 0.0005;
}

static const NativeSongWindow* nativeFindFamilyParent(
  const NativeSongWindow& child,
  const std::vector<NativeSongWindow>& songs)
{
  if (!child.isHashChild || child.parentId.empty()) return nullptr;
  for (const auto& candidate : songs) {
    if (!candidate.isHashParent || !nativeSongIsPlayable(candidate)) continue;
    const bool sameId = candidate.id == child.parentId || std::to_string(candidate.sourceNumber) == child.parentId;
    const bool sameEnum = child.parentEnumIndex >= 0 && candidate.enumIndex == child.parentEnumIndex;
    if (sameId || sameEnum) return &candidate;
  }
  return nullptr;
}

static const NativeSongWindow* nativeFindPlayingFamilyChildLocked(
  const std::vector<NativeSongWindow>& songs,
  const std::string& playingId,
  double playPos,
  double songStart,
  double songEnd)
{
  const NativeSongWindow* best = nullptr;
  double bestLength = 1e99;
  for (const auto& candidate : songs) {
    if (!candidate.isHashChild || !nativeSongIsPlayable(candidate)) continue;
    const bool idMatch = !playingId.empty() &&
      (candidate.id == playingId || std::to_string(candidate.sourceNumber) == playingId || candidate.playlistEntryId == playingId);
    const bool boundsMatch = std::fabs(candidate.start - songStart) <= 0.002 && std::fabs(candidate.end - songEnd) <= 0.002;
    const bool containsPlay = playPos >= candidate.start - 0.0005 && playPos < candidate.end - 0.0005;
    if (!idMatch && !boundsMatch && !containsPlay) continue;
    const double length = candidate.end - candidate.start;
    if (idMatch || boundsMatch || length < bestLength) {
      best = &candidate;
      bestLength = length;
      if (idMatch || boundsMatch) break;
    }
  }
  return best;
}

static bool nativeQueuedChildConflictsWithPlayingFamilyLocked(const NativeSongWindow& target)
{
  if (!target.isHashChild || target.parentId.empty() || !g_nativeCurrentTransportPlaying) return false;
  const NativeSongWindow* playingChild = nativeFindPlayingFamilyChildLocked(
    g_nativeSongWindows,
    g_nativeCurrentPlayingId,
    g_nativeCurrentPlayPosition,
    g_nativeCurrentSongStart,
    g_nativeCurrentSongEnd);
  return playingChild && !playingChild->parentId.empty() && playingChild->parentId == target.parentId;
}

static void nativeResolveQueueSourceBoundary(
  const std::vector<NativeSongWindow>& songs,
  const std::string& playingId,
  double playPos,
  double songStart,
  double songEnd,
  double& boundaryStart,
  double& boundaryEnd,
  std::string& boundaryId)
{
  boundaryStart = songStart;
  boundaryEnd = songEnd;
  boundaryId = playingId;
  const NativeSongWindow* child = nativeFindPlayingFamilyChildLocked(songs, playingId, playPos, songStart, songEnd);
  if (!child) return;
  const NativeSongWindow* parent = nativeFindFamilyParent(*child, songs);
  if (!parent) return;
  boundaryStart = parent->start;
  boundaryEnd = parent->end;
  boundaryId = parent->id;
}

static bool nativeSongBoundsMatch(const NativeSongWindow& item, double startPos, double endPos)
{
  if (startPos <= 0.0 && endPos <= 0.0) return false;
  const bool startOk = startPos <= 0.0 || std::fabs(item.start - startPos) <= 0.002;
  const bool endOk = endPos <= 0.0 || std::fabs(item.end - endPos) <= 0.002;
  return startOk && endOk;
}


static bool nativeApplyPlaylistNumberSortCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "sort_playlist_by_number" && type != "playlist_sort_number" && type != "sort_repertoire_by_number") return false;
  if (!SetProjExtState_ptr) return false;

  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return false;

  const std::string data = nativeGetProjExtStateString(project, kPlaylistExtSection, kPlaylistExtKey);
  if (data.empty()) return true;

  std::string targetName = nativeJsonExtractString(commandBody, "playlistName");
  if (targetName.empty()) targetName = nativeJsonExtractString(commandBody, "activePlaylistName");
  if (targetName.empty()) targetName = nativeJsonExtractString(commandBody, "currentPlaylistName");
  if (targetName.empty()) targetName = nativeGetProjExtStateString(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1");
  targetName = nativeTrim(targetName);

  std::string direction = nativeLower(nativeTrim(nativeJsonExtractString(commandBody, "direction")));
  const std::string descendingText = nativeLower(nativeTrim(nativeJsonExtractString(commandBody, "descending")));
  const bool descending =
    direction == "desc" || direction == "descending" || direction == "9-0" ||
    descendingText == "1" || descendingText == "true" || descendingText == "yes" || descendingText == "on";

  std::vector<std::string> lines = nativeSplit(data, '\n');
  bool inTarget = false;
  bool targetFound = false;
  int playlistIndex = 0;

  struct SortableLine {
    size_t lineIndex = 0;
    std::string raw;
    int key = 0;
    size_t oldOrder = 0;
  };
  std::vector<SortableLine> sortable;

  for (size_t i = 0; i < lines.size(); ++i) {
    const auto fields = nativeSplit(lines[i], '\t');
    if (fields.empty()) continue;

    if (fields[0] == "PLAYLIST") {
      ++playlistIndex;
      const std::string currentName = fields.size() > 1 ? nativeTrim(nativeUnescapeExtField(fields[1])) : std::string();
      inTarget = targetName.empty() ? (playlistIndex == 1) : (currentName == targetName);
      if (inTarget) targetFound = true;
      continue;
    }

    if (fields[0] == "END") {
      inTarget = false;
      continue;
    }

    if (!inTarget || fields[0] != "ITEM") continue;

    const std::string itemType = fields.size() > 1 ? nativeLower(nativeTrim(fields[1])) : std::string();
    const int sourceNumber = fields.size() > 2 ? std::atoi(fields[2].c_str()) : 0;
    const bool isBlock = itemType == "block" || itemType == "bloco" || sourceNumber < 0;
    if (isBlock) continue;

    SortableLine entry;
    entry.lineIndex = i;
    entry.raw = lines[i];
    entry.key = sourceNumber;
    entry.oldOrder = sortable.size();
    sortable.push_back(entry);
  }

  if (!targetFound || sortable.size() <= 1) return true;

  std::vector<SortableLine> sorted = sortable;
  std::stable_sort(sorted.begin(), sorted.end(), [descending](const SortableLine& a, const SortableLine& b) {
    if (a.key == b.key) return a.oldOrder < b.oldOrder;
    return descending ? (a.key > b.key) : (a.key < b.key);
  });

  bool changed = false;
  for (size_t i = 0; i < sortable.size(); ++i) {
    if (lines[sortable[i].lineIndex] != sorted[i].raw) changed = true;
    lines[sortable[i].lineIndex] = sorted[i].raw;
  }

  if (!changed) {
    g_nativeForceStateBuild.store(true);
    return true;
  }

  std::ostringstream rebuilt;
  for (size_t i = 0; i < lines.size(); ++i) {
    rebuilt << lines[i];
    if (i + 1 < lines.size()) rebuilt << '\n';
  }

  SetProjExtState_ptr(project, kPlaylistExtSection, kPlaylistExtKey, rebuilt.str().c_str());

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeSelectedId.clear();
    g_nativeSelectedTab.clear();
    g_nativeSelectedStart = 0.0;
    g_nativeSelectedEnd = 0.0;
  }

  g_nativeForceSnapshotBuild.store(true);
  g_nativeForceStateBuild.store(true);
  return true;
}


static bool nativeSongIdMatches(const NativeSongWindow& item, const std::string& idValue)
{
  if (idValue.empty()) return false;
  return item.id == idValue || std::to_string(item.sourceNumber) == idValue || item.playlistEntryId == idValue;
}

static const NativeSongWindow* nativeFindSongForCommand(const std::string& idValue, double startPos, double endPos);

static bool nativeSongIsCurrentPlayingLocked(const NativeSongWindow& item)
{
  if (!g_nativeCurrentTransportPlaying || !nativeSongIsPlayable(item)) return false;
  if (!g_nativeCurrentPlayingId.empty() && nativeSongIdMatches(item, g_nativeCurrentPlayingId)) return true;
  if (g_nativeCurrentSongEnd > g_nativeCurrentSongStart + 0.0005) {
    const bool sameStart = std::fabs(item.start - g_nativeCurrentSongStart) <= 0.002;
    const bool sameEnd = std::fabs(item.end - g_nativeCurrentSongEnd) <= 0.002;
    if (sameStart && sameEnd) return true;

    // Tanto marker-filho antigo quanto regiao-filho da segunda faixa impedem
    // enfileirar o proprio pai enquanto uma musica-filha estiver tocando.
    const bool itemContainsCurrentChild =
      item.isHashParent &&
      item.start <= g_nativeCurrentSongStart + 0.002 &&
      item.end >= g_nativeCurrentSongEnd - 0.002 &&
      (item.end - item.start) > (g_nativeCurrentSongEnd - g_nativeCurrentSongStart) + 0.002;
    if (itemContainsCurrentChild) return true;
  }
  return false;
}

static void nativeClearQueuedSongLocked()
{
  g_nativeQueuedSongId.clear();
  g_nativeQueuedPlaylistSongId.clear();
  g_nativeQueuedRegionNumber = 0;
  g_nativeQueuedStart = 0.0;
  g_nativeQueuedEnd = 0.0;
  g_nativeQueuedPlaylistIndex = 0;
  g_nativeQueuedManual = false;
  g_nativeQueuedKind = "playlist";
  g_nativeQueuedSeekSignature.clear();
  g_nativeLastAutoQueueForPlayingId.clear();
}

static void nativeClearAutoBlocoTargetLocked()
{
  g_nativeAutoBlocoTargetSongId.clear();
  g_nativeAutoBlocoTargetPlaylistSongId.clear();
  g_nativeAutoBlocoTargetRegionNumber = 0;
  g_nativeAutoBlocoTargetStart = 0.0;
  g_nativeAutoBlocoTargetEnd = 0.0;
  g_nativeAutoBlocoTargetPlaylistIndex = 0;
}

static void nativeClearAllQueueStateLocked()
{
  nativeClearQueuedSongLocked();
  nativeClearAutoBlocoTargetLocked();
  nativeCancelQueueHandoffProtectionLocked();
  g_nativeAutoStopLastStoppedSignature.clear();
}

static void nativeRefreshLuaControlHeartbeatFromExtState()
{
  static bool lastPublishedActive = false;
  if (!GetExtState_ptr) return;
  const char* raw = GetExtState_ptr(kNativeExtStateSection, kLuaControlHeartbeatKey);
  const std::string token = raw ? nativeTrim(raw) : std::string();
  const auto now = std::chrono::steady_clock::now();

  std::lock_guard<std::mutex> lock(g_nativeMutex);

  if (token.empty()) {
    g_nativeLastLuaControlHeartbeat = std::chrono::steady_clock::time_point();
    g_nativeLastLuaControlHeartbeatToken.clear();
  } else if (token != g_nativeLastLuaControlHeartbeatToken) {
    g_nativeLastLuaControlHeartbeatToken = token;
    g_nativeLastLuaControlHeartbeat = now;
  }

  const bool isActive = g_nativeLastLuaControlHeartbeat.time_since_epoch().count() != 0 &&
    std::chrono::duration_cast<std::chrono::milliseconds>(now - g_nativeLastLuaControlHeartbeat).count() < 1800;

  if (lastPublishedActive != isActive) {
    lastPublishedActive = isActive;
    // A fila e parte do estado compartilhado. Nao apaga quando o controle muda
    // entre App Diretor e Lua; isso permite que a fila manual seja retomada.
    // Limpa apenas a sequencia para reler o snapshot mais recente do Lua.
    g_nativeLastLuaQueueSyncSeq.clear();
    nativePublishSharedControlStateLocked();
    g_nativeForceStateBuild.store(true);
  }
}

static void nativeClearExplicitStopQueueState()
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  nativeClearAllQueueStateLocked();
  g_nativeForceStateBuild.store(true);
}

static bool nativeSnapshotStopTargetLocked(std::string& idOut, std::string& tabOut, double& startOut, double& endOut)
{
  // Prioridade correta do Stop:
  // 1) se existe fila, ela vira a nova selecao pronta para Play;
  // 2) se nao existe fila, fica na musica que estava tocando;
  // 3) se nao existe nada, nao força selecao.
  if (!g_nativeQueuedSongId.empty() && g_nativeQueuedEnd > g_nativeQueuedStart + 0.0005) {
    idOut = g_nativeQueuedSongId;
    tabOut = g_nativeQueuedKind == "regions" ? "regions" : "playlist";
    startOut = g_nativeQueuedStart;
    endOut = g_nativeQueuedEnd;
    return true;
  }

  if (!g_nativeAutoBlocoTargetSongId.empty() && g_nativeAutoBlocoTargetEnd > g_nativeAutoBlocoTargetStart + 0.0005) {
    idOut = g_nativeAutoBlocoTargetSongId;
    tabOut = "playlist";
    startOut = g_nativeAutoBlocoTargetStart;
    endOut = g_nativeAutoBlocoTargetEnd;
    return true;
  }

  if (!g_nativeCurrentPlayingId.empty() && g_nativeCurrentSongEnd > g_nativeCurrentSongStart + 0.0005) {
    idOut = g_nativeCurrentPlayingId;
    tabOut = "playlist";
    startOut = g_nativeCurrentSongStart;
    endOut = g_nativeCurrentSongEnd;
    return true;
  }

  return false;
}

static void nativePublishStopReadyTargetLocked(const std::string& id, const std::string& tab, double startPos, double endPos, int playlistIndex)
{
  if (!SetExtState_ptr) return;
  ++g_nativeStopReadySequence;
  SetExtState_ptr(kNativeExtStateSection, "LAST_STOP_READY_TOKEN_V1", std::to_string(g_nativeStopReadySequence).c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, "LAST_STOP_READY_ID_V1", id.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, "LAST_STOP_READY_TAB_V1", tab.c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, "LAST_STOP_READY_START_V1", nativeNumber(startPos).c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, "LAST_STOP_READY_END_V1", nativeNumber(endPos).c_str(), false);
  SetExtState_ptr(kNativeExtStateSection, "LAST_STOP_READY_PLAYLIST_INDEX_V1", std::to_string(playlistIndex).c_str(), false);
}

static bool nativePrepareStopSelectionFromQueueOrCurrent(ReaProject* project, bool moveCursorAfterStop)
{
  std::string stopSelectionId;
  std::string stopSelectionTab;
  double stopStartPos = 0.0;
  double stopEndPos = 0.0;

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (!nativeSnapshotStopTargetLocked(stopSelectionId, stopSelectionTab, stopStartPos, stopEndPos)) {
      nativeClearQueuedSongLocked();
      nativeClearAutoBlocoTargetLocked();
      nativeCancelQueueHandoffProtectionLocked();
      g_nativeForceStateBuild.store(true);
      return false;
    }
  }

  double finalStart = stopStartPos;
  double finalEnd = stopEndPos;
  std::string finalId = stopSelectionId;
  std::string finalTab = stopSelectionTab == "regions" ? "regions" : "playlist";
  int finalPlaylistIndex = 0;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const bool hasExactStopBounds = stopEndPos > stopStartPos + 0.0005;
    const NativeSongWindow* selectedSong = hasExactStopBounds
      ? nullptr
      : nativeFindSongForCommand(stopSelectionId, stopStartPos, stopEndPos);
    if (hasExactStopBounds) {
      g_nativeSelectedId = stopSelectionId;
      g_nativeSelectedTab = stopSelectionTab == "regions" ? "regions" : "playlist";
      g_nativeSelectedStart = stopStartPos;
      g_nativeSelectedEnd = stopEndPos;
      finalId = g_nativeSelectedId;
      finalTab = g_nativeSelectedTab;
      finalStart = stopStartPos;
      finalEnd = stopEndPos;
    } else if (selectedSong) {
      g_nativeSelectedId = selectedSong->id;
      g_nativeSelectedTab = stopSelectionTab == "regions" ? "regions" : "playlist";
      g_nativeSelectedStart = selectedSong->start;
      g_nativeSelectedEnd = selectedSong->end;
      finalId = selectedSong->id;
      finalTab = g_nativeSelectedTab;
      finalStart = selectedSong->start;
      finalEnd = selectedSong->end;
      finalPlaylistIndex = selectedSong->playlistOrder;
    } else {
      g_nativeSelectedId = stopSelectionId;
      g_nativeSelectedTab = stopSelectionTab == "regions" ? "regions" : "playlist";
      g_nativeSelectedStart = stopStartPos;
      g_nativeSelectedEnd = stopEndPos;
      finalId = g_nativeSelectedId;
      finalTab = g_nativeSelectedTab;
    }
    nativePublishStopReadyTargetLocked(finalId, finalTab, finalStart, finalEnd, finalPlaylistIndex);
    nativeCancelQueueHandoffProtectionLocked();
    g_nativeAutoStopLastStoppedSignature.clear();
  }

  if (moveCursorAfterStop && SetEditCurPos2_ptr && finalStart >= 0.0) {
    SetEditCurPos2_ptr(project, finalStart, true, false);
  }
  if (UpdateArrange_ptr) UpdateArrange_ptr();
  g_nativeForceStateBuild.store(true);
  return true;
}


static bool nativeStopTransportAndPrepareExplicitSelection(ReaProject* project, const std::string& explicitSelectionId, const std::string& explicitSelectionTab, double explicitStartPos, double explicitEndPos, bool moveCursorAfterStop)
{
  if (!Main_OnCommand_ptr) return false;

  std::string stopSelectionId;
  std::string stopSelectionTab;
  double stopStartPos = 0.0;
  double stopEndPos = 0.0;

  // Mesma sequencia do Lua: captura um snapshot interno ANTES do Stop.
  // Fila ganha; sem fila, usa a musica atual. O alvo explicito do cliente e
  // apenas fallback quando a extensao ainda nao publicou nenhum desses estados.
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const bool hasInternalTarget = nativeSnapshotStopTargetLocked(
      stopSelectionId, stopSelectionTab, stopStartPos, stopEndPos);
    if (!hasInternalTarget && (!explicitSelectionId.empty() || explicitEndPos > explicitStartPos + 0.0005)) {
      stopSelectionId = explicitSelectionId;
      stopSelectionTab = explicitSelectionTab;
      stopStartPos = explicitStartPos;
      stopEndPos = explicitEndPos;
    }
    nativeCancelQueueHandoffProtectionLocked();
  }

  Main_OnCommand_ptr(1016, 0); // Transport: Stop

  double finalSelectionStart = stopStartPos;
  double finalSelectionEnd = stopEndPos;
  std::string finalSelectionId = stopSelectionId;
  std::string finalSelectionTab = stopSelectionTab == "regions" ? "regions" : "playlist";
  int finalPlaylistIndex = 0;
  bool hasFinalSelection = false;

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (!stopSelectionId.empty() && stopEndPos > stopStartPos + 0.0005) {
      // Aplica o snapshot, sem re-resolver o ID. Isso e essencial para filhos:
      // o ID m... nao e uma regiao independente, mas seus limites sao completos.
      g_nativeSelectedId = stopSelectionId;
      g_nativeSelectedTab = stopSelectionTab == "regions" ? "regions" : "playlist";
      g_nativeSelectedStart = stopStartPos;
      g_nativeSelectedEnd = stopEndPos;
      finalSelectionId = g_nativeSelectedId;
      finalSelectionTab = g_nativeSelectedTab;
      finalSelectionStart = stopStartPos;
      finalSelectionEnd = stopEndPos;
      hasFinalSelection = true;
    }
    if (hasFinalSelection) {
      nativePublishStopReadyTargetLocked(finalSelectionId, finalSelectionTab, finalSelectionStart, finalSelectionEnd, finalPlaylistIndex);
    } else {
      nativePublishStopReadyTargetLocked("", "", 0.0, 0.0, 0);
    }

    // A selecao pronta permanece em g_nativeSelected*. A fila, por outro lado,
    // precisa desaparecer da extensao assim que o Stop do Diretor for processado.
    nativeClearAllQueueStateLocked();
    nativeBumpSharedRevisionLocked("director");
    g_nativeSuppressStoppedTransitionPrepareUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
  }

  if (moveCursorAfterStop && hasFinalSelection && SetEditCurPos2_ptr && finalSelectionStart >= 0.0) {
    SetEditCurPos2_ptr(project, finalSelectionStart, true, false);
  }
  if (UpdateArrange_ptr) UpdateArrange_ptr();
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeStopTransportAndPrepareSelectionFromQueueOrCurrent(ReaProject* project, bool moveCursorAfterStop)
{
  return nativeStopTransportAndPrepareExplicitSelection(project, "", "", 0.0, 0.0, moveCursorAfterStop);
}

static void nativeSetAutoBlocoTargetLocked(const NativeSongWindow& song)
{
  g_nativeAutoBlocoTargetSongId = song.id;
  g_nativeAutoBlocoTargetPlaylistSongId = song.playlistEntryId.empty() ? song.id : song.playlistEntryId;
  g_nativeAutoBlocoTargetRegionNumber = song.sourceNumber;
  g_nativeAutoBlocoTargetStart = song.start;
  g_nativeAutoBlocoTargetEnd = song.end;
  g_nativeAutoBlocoTargetPlaylistIndex = song.playlistOrder;
}

static void nativeSetQueuedSongLocked(const NativeSongWindow& song, bool manual, const std::string& kind = "playlist")
{
  if (nativeSongIsCurrentPlayingLocked(song)) {
    nativeClearQueuedSongLocked();
    return;
  }
  g_nativeQueuedSongId = song.id;
  g_nativeQueuedPlaylistSongId = song.playlistEntryId.empty() ? song.id : song.playlistEntryId;
  g_nativeQueuedRegionNumber = song.sourceNumber;
  g_nativeQueuedStart = song.start;
  g_nativeQueuedEnd = song.end;
  g_nativeQueuedPlaylistIndex = song.playlistOrder;
  g_nativeQueuedManual = manual;
  g_nativeQueuedKind = nativeLower(kind) == "regions" ? "regions" : "playlist";
  g_nativeQueuedSeekSignature.clear();
  nativeClearAutoBlocoTargetLocked();
  if (manual) {
    // FIX100: intervencao manual cancela a fila automatica atual. O Auto volta
    // a calcular a proxima somente depois que esta fila manual entrar em playback.
    g_nativeLastAutoQueueForPlayingId.clear();
  } else {
    g_nativeLastAutoQueueForPlayingId = g_nativeCurrentPlayingId;
  }
}

static const NativeSongWindow* nativeFindLuaQueueSong(
  const std::vector<NativeSongWindow>& activeItems,
  const std::vector<NativeSongWindow>& songs,
  const std::string& idValue,
  int playlistOrder,
  double startPos,
  double endPos)
{
  const bool hasBounds = endPos > startPos + 0.0005;
  const NativeSongWindow* bestByBounds = nullptr;
  double bestScore = 1e99;

  auto scanExact = [&](const std::vector<NativeSongWindow>& list) -> const NativeSongWindow* {
    for (const auto& item : list) {
      if (!nativeSongIsPlayable(item)) continue;
      const bool idMatch = !idValue.empty() && nativeSongIdMatches(item, idValue);
      const bool boundsMatch = hasBounds && nativeSongBoundsMatch(item, startPos, endPos);
      if (idMatch && (!hasBounds || boundsMatch)) return &item;
      if (boundsMatch) return &item;
      if (hasBounds) {
        const double score = std::fabs(item.start - startPos) + std::fabs(item.end - endPos);
        if (score < bestScore) { bestByBounds = &item; bestScore = score; }
      }
    }
    return nullptr;
  };

  // O Lua fornece id + limites reais. Eles ganham do indice do repertorio,
  // porque o indice visual pode incluir blocos que a extensao nao conta igual.
  if (const NativeSongWindow* exact = scanExact(activeItems)) return exact;
  if (const NativeSongWindow* exact = scanExact(songs)) return exact;

  if (playlistOrder > 0) {
    for (const auto& item : activeItems) {
      if (nativeSongIsPlayable(item) && item.playlistOrder == playlistOrder) return &item;
    }
    if (playlistOrder <= static_cast<int>(activeItems.size())) {
      const NativeSongWindow& item = activeItems[static_cast<size_t>(playlistOrder - 1)];
      if (nativeSongIsPlayable(item)) return &item;
    }
  }

  return bestScore <= 0.050 ? bestByBounds : nullptr;
}


static void nativeSyncControlStateFromLuaExtState()
{
  if (!GetExtState_ptr) return;
  // Aparencia da gaveta pertence ao Lua, inclusive enquanto o Diretor e a
  // autoridade dos controles de transporte.
  const char* outlineEnabledRaw = GetExtState_ptr(kNativeExtStateSection, kLuaDrawerOutlineEnabledKey);
  const char* outlineColorRaw = GetExtState_ptr(kNativeExtStateSection, kLuaDrawerOutlineColorKey);
  const char* symbolEnabledRaw = GetExtState_ptr(kNativeExtStateSection, kLuaDrawerSymbolEnabledKey);
  const char* symbolColorRaw = GetExtState_ptr(kNativeExtStateSection, kLuaDrawerSymbolColorKey);
  bool drawerStyleChanged = false;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const bool outlineEnabled = nativeBoolFromText(outlineEnabledRaw ? outlineEnabledRaw : "", g_nativeDrawerOutlineEnabled);
    const bool symbolEnabled = nativeBoolFromText(symbolEnabledRaw ? symbolEnabledRaw : "", g_nativeDrawerSymbolEnabled);
    const std::string outlineColor = nativeTrim(outlineColorRaw ? outlineColorRaw : "").empty()
      ? g_nativeDrawerOutlineColor : nativeTrim(outlineColorRaw);
    const std::string symbolColor = nativeTrim(symbolColorRaw ? symbolColorRaw : "").empty()
      ? g_nativeDrawerSymbolColor : nativeTrim(symbolColorRaw);
    drawerStyleChanged = outlineEnabled != g_nativeDrawerOutlineEnabled ||
      symbolEnabled != g_nativeDrawerSymbolEnabled || outlineColor != g_nativeDrawerOutlineColor ||
      symbolColor != g_nativeDrawerSymbolColor;
    g_nativeDrawerOutlineEnabled = outlineEnabled;
    g_nativeDrawerSymbolEnabled = symbolEnabled;
    g_nativeDrawerOutlineColor = outlineColor;
    g_nativeDrawerSymbolColor = symbolColor;
  }
  if (drawerStyleChanged) g_nativeForceStateBuild.store(true);
  if (!nativeIsLuaControlActive()) return;
  // Enquanto o Diretor esta conectado, a extensao e a autoridade dos controles.
  // Impede um snapshot antigo do Lua de religar o AutoStop alguns segundos depois.
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (g_nativeLastDirectorHeartbeat.time_since_epoch().count() != 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_nativeLastDirectorHeartbeat).count();
      if (elapsed >= 0 && elapsed < 12000) return;
    }
  }
  const char* seqRaw = GetExtState_ptr(kNativeExtStateSection, kLuaStateSyncSeqKey);
  const std::string seq = seqRaw ? nativeTrim(seqRaw) : std::string();
  if (seq.empty()) return;
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (seq == g_nativeLastLuaStateSyncSeq) return;
  g_nativeLastLuaStateSyncSeq = seq;
  const char* autoRaw = GetExtState_ptr(kNativeExtStateSection, kLuaAutoplayStateKey);
  const char* atblRaw = GetExtState_ptr(kNativeExtStateSection, kLuaAutoBlocoStateKey);
  const char* stopRaw = GetExtState_ptr(kNativeExtStateSection, kLuaAutoStopStateKey);
  const char* stopPauseRaw = GetExtState_ptr(kNativeExtStateSection, kLuaStopPauseStateKey);
  const char* drawerIdsRaw = GetExtState_ptr(kNativeExtStateSection, kLuaFamilyDrawersIdsKey);
  std::map<std::string, bool> luaOpenDrawers;
  const std::vector<std::string> luaDrawerIds = nativeSplit(drawerIdsRaw ? drawerIdsRaw : "", '|');
  for (const std::string& rawId : luaDrawerIds) {
    const std::string id = nativeTrim(rawId);
    if (!id.empty()) luaOpenDrawers[id] = true;
  }
  g_nativeAutoplayEnabled = nativeBoolFromText(autoRaw ? autoRaw : "", g_nativeAutoplayEnabled);
  g_nativeAutoBlocoEnabled = nativeBoolFromText(atblRaw ? atblRaw : "", g_nativeAutoBlocoEnabled);
  if (!g_nativeAutoplayEnabled) nativeClearAutoBlocoTargetLocked();
  g_nativeAutoStopEnabled = nativeBoolFromText(stopRaw ? stopRaw : "", g_nativeAutoStopEnabled);
  g_stopPauseModeEnabled.store(nativeBoolFromText(stopPauseRaw ? stopPauseRaw : "", g_stopPauseModeEnabled.load()));
  if (luaOpenDrawers != g_nativeDirectorOpenFamilyDrawers || g_nativeFamilyDrawersOwner != "lua") {
    g_nativeDirectorOpenFamilyDrawers = std::move(luaOpenDrawers);
    g_nativeFamilyDrawersOwner = "lua";
    ++g_nativeFamilyDrawersRevision;
    ++g_nativeDirectorListScrollRevision;
  }
  g_nativeControlOwner = "lua";
  nativeSaveAutoplayEnabledLocked();
  nativeSaveAutoBlocoEnabledLocked();
  nativeSaveAutoStopEnabledLocked();
  nativePublishSharedControlStateLocked();
  g_nativeForceStateBuild.store(true);
}

static void nativeSyncQueueFromLuaExtState(
  const std::vector<NativeSongWindow>& activeItems,
  const std::vector<NativeSongWindow>& songs)
{
  if (!GetExtState_ptr || !nativeIsLuaControlActive()) return;

  const char* seqRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueSyncSeqKey);
  const std::string seq = seqRaw ? nativeTrim(seqRaw) : std::string();
  if (seq.empty()) return;

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (seq == g_nativeLastLuaQueueSyncSeq) return;
    g_nativeLastLuaQueueSyncSeq = seq;
  }

  const char* activeRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueActiveKey);
  const bool queueActive = nativeBoolFromText(activeRaw ? activeRaw : "", false);
  if (!queueActive) {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeClearAllQueueStateLocked();
    g_nativeForceStateBuild.store(true);
    return;
  }

  const char* idRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueIdKey);
  const char* indexRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueIndexKey);
  const char* startRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueStartKey);
  const char* endRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueEndKey);
  const char* kindRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueKindKey);
  const char* manualRaw = GetExtState_ptr(kNativeExtStateSection, kLuaQueueManualKey);
  const std::string idValue = idRaw ? nativeTrim(idRaw) : std::string();
  const int playlistOrder = indexRaw && nativeLooksNumeric(indexRaw) ? std::atoi(indexRaw) : 0;
  const double startPos = startRaw && nativeLooksNumeric(startRaw) ? std::atof(startRaw) : 0.0;
  const double endPos = endRaw && nativeLooksNumeric(endRaw) ? std::atof(endRaw) : 0.0;
  const std::string queueKind = kindRaw && nativeLower(nativeTrim(kindRaw)) == "regions" ? "regions" : "playlist";
  const bool queueManual = nativeBoolFromText(manualRaw ? manualRaw : "", true);

  const NativeSongWindow* song = nativeFindLuaQueueSong(activeItems, songs, idValue, playlistOrder, startPos, endPos);
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (song && nativeSongIsPlayable(*song)) {
    nativeSetQueuedSongLocked(*song, queueManual, queueKind);
  } else if (!idValue.empty() && endPos > startPos + 0.0005) {
    g_nativeQueuedSongId = idValue;
    g_nativeQueuedPlaylistSongId = idValue;
    g_nativeQueuedRegionNumber = nativeLooksNumeric(idValue) ? std::atoi(idValue.c_str()) : 0;
    g_nativeQueuedStart = startPos;
    g_nativeQueuedEnd = endPos;
    g_nativeQueuedPlaylistIndex = playlistOrder;
    g_nativeQueuedManual = queueManual;
    g_nativeQueuedKind = queueKind;
    g_nativeQueuedSeekSignature.clear();
    nativeClearAutoBlocoTargetLocked();
    nativeCancelQueueHandoffProtectionLocked();
  } else {
    nativeClearAllQueueStateLocked();
  }
  g_nativeForceStateBuild.store(true);
}

static int nativeFindCurrentIndexInActivePlaylist(const std::vector<NativeSongWindow>& items, const std::string& playingId, double songStart, double songEnd)
{
  if (items.empty()) return -1;
  int firstIdMatch = -1;
  int exactMatch = -1;
  int containingParent = -1;
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    const NativeSongWindow& item = items[static_cast<size_t>(i)];
    if (!nativeSongIsPlayable(item)) continue;
    if (!playingId.empty() && item.id == playingId && firstIdMatch < 0) firstIdMatch = i;
    if (std::fabs(item.start - songStart) <= 0.002 && std::fabs(item.end - songEnd) <= 0.002) exactMatch = i;
    if (item.isHashParent && songStart >= item.start - 0.002 && songEnd <= item.end + 0.002) containingParent = i;
  }

  int matched = exactMatch >= 0 ? exactMatch : firstIdMatch;
  if (matched >= 0) {
    const NativeSongWindow& item = items[static_cast<size_t>(matched)];
    if (item.isHashChild && !item.parentId.empty()) {
      for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const NativeSongWindow& candidate = items[static_cast<size_t>(i)];
        if (candidate.isHashParent &&
          (candidate.id == item.parentId || std::to_string(candidate.sourceNumber) == item.parentId)) return i;
      }
    }
    return matched;
  }
  return containingParent;
}

static const NativeSongWindow* nativeResolveNextAutoQueueTarget(const std::vector<NativeSongWindow>& items, int currentIndex, bool autoBlocoEnabled, const NativeSongWindow** autoBlocoBoundaryTarget = nullptr)
{
  if (autoBlocoBoundaryTarget) *autoBlocoBoundaryTarget = nullptr;
  if (currentIndex < 0 || currentIndex >= static_cast<int>(items.size())) return nullptr;
  bool crossedBlock = false;
  for (int i = currentIndex + 1; i < static_cast<int>(items.size()); ++i) {
    const NativeSongWindow& candidate = items[static_cast<size_t>(i)];
    if (candidate.isBlock) {
      crossedBlock = true;
      continue;
    }
    if (!nativeSongIsPlayable(candidate)) continue;
    // O Auto nunca escolhe uma musica-filho. Um filho so pode receber a
    // marcacao de fila por uma escolha manual explicita do usuario.
    if (candidate.isHashChild) continue;
    if (autoBlocoEnabled && crossedBlock) {
      if (autoBlocoBoundaryTarget) *autoBlocoBoundaryTarget = &candidate;
      return nullptr;
    }
    return &candidate;
  }
  return nullptr;
}

// FIX100: a fila manual precisa ganhar da fila automatica.
// O Lua sabe a posicao real do item no repertorio; a extensao agora usa essa
// posicao antes de cair no fallback por id/start/end. Sem isso, com Auto ligado,
// a extensao podia continuar usando a proxima musica sequencial que ela ja tinha
// armado automaticamente.
static const NativeSongWindow* nativeFindActivePlaylistSongByOrderLocked(int playlistOrder)
{
  if (playlistOrder <= 0) return nullptr;

  for (const auto& item : g_nativeActivePlaylistItems) {
    if (!nativeSongIsPlayable(item)) continue;
    if (item.playlistOrder == playlistOrder) return &item;
  }

  if (playlistOrder <= static_cast<int>(g_nativeActivePlaylistItems.size())) {
    const NativeSongWindow& item = g_nativeActivePlaylistItems[static_cast<size_t>(playlistOrder - 1)];
    if (nativeSongIsPlayable(item)) return &item;
  }

  return nullptr;
}

static bool nativeQueuedSongIsSameAsPlayingLocked(const std::string& playingId, double playPos)
{
  if (g_nativeQueuedSongId.empty()) return false;
  if (!playingId.empty() && playingId == g_nativeQueuedSongId) return true;
  if (g_nativeQueuedStart > 0.0 && g_nativeQueuedEnd > g_nativeQueuedStart) {
    if (playPos >= g_nativeQueuedStart - 0.0005 && playPos < g_nativeQueuedEnd - 0.0005) return true;
  }
  return false;
}

static void nativeMaintainQueueAutomation(ReaProject* project, bool playing, const std::string& playingId, double playPos, double songStart, double songEnd, const std::vector<NativeSongWindow>& activeItems, const std::vector<NativeSongWindow>& projectSongs)
{
  if (!project || nativeIsLuaControlActive()) return;

  if (!playing || playingId.empty()) {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const bool hasPendingQueueTarget = !g_nativeQueuedSongId.empty() || !g_nativeAutoBlocoTargetSongId.empty();
    if (!hasPendingQueueTarget) {
      nativeClearQueuedSongLocked();
      nativeClearAutoBlocoTargetLocked();
      g_nativeAutoStopLastStoppedSignature.clear();
      return;
    }
    // Stop/PlayStop deve preservar a fila/Auto como alvo pronto para o próximo Play.
    // O alvo só deve ser consumido quando a música efetivamente entra em playback.
    g_nativeForceStateBuild.store(true);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeLoadAutomationSettingsOnceLocked();
    if (nativeQueuedSongIsSameAsPlayingLocked(playingId, playPos)) {
      nativeClearQueuedSongLocked();
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (!g_nativeAutoplayEnabled || !g_nativeAutoBlocoEnabled || g_nativeQueuedManual) {
      nativeClearAutoBlocoTargetLocked();
    }
    if (!g_nativeQueuedManual && !g_nativeQueuedSongId.empty()) {
      for (const auto& candidate : projectSongs) {
        if (!candidate.isHashChild) continue;
        if (nativeSongIdMatches(candidate, g_nativeQueuedSongId) ||
          nativeSongBoundsMatch(candidate, g_nativeQueuedStart, g_nativeQueuedEnd)) {
          nativeClearQueuedSongLocked();
          break;
        }
      }
    }
    if (g_nativeAutoplayEnabled && !g_nativeQueuedManual && g_nativeQueuedSongId.empty()) {
      const int currentIndex = nativeFindCurrentIndexInActivePlaylist(activeItems, playingId, songStart, songEnd);
      const NativeSongWindow* boundaryTarget = nullptr;
      const NativeSongWindow* next = nativeResolveNextAutoQueueTarget(activeItems, currentIndex, g_nativeAutoBlocoEnabled, &boundaryTarget);
      if (next && nativeSongIsPlayable(*next) && !nativeSongIsCurrentPlayingLocked(*next)) {
        nativeSetQueuedSongLocked(*next, false);
      } else if (boundaryTarget && nativeSongIsPlayable(*boundaryTarget)) {
        nativeClearQueuedSongLocked();
        nativeSetAutoBlocoTargetLocked(*boundaryTarget);
        g_nativeLastAutoQueueForPlayingId = playingId;
      } else {
        nativeClearAutoBlocoTargetLocked();
        g_nativeLastAutoQueueForPlayingId = playingId;
      }
    } else if (g_nativeAutoplayEnabled && g_nativeAutoBlocoEnabled && !g_nativeQueuedManual && !g_nativeQueuedSongId.empty()) {
      const int currentIndex = nativeFindCurrentIndexInActivePlaylist(activeItems, playingId, songStart, songEnd);
      const NativeSongWindow* boundaryTarget = nullptr;
      nativeResolveNextAutoQueueTarget(activeItems, currentIndex, true, &boundaryTarget);
      if (boundaryTarget && nativeSongIsPlayable(*boundaryTarget) && nativeSongIdMatches(*boundaryTarget, g_nativeQueuedSongId)) {
        nativeClearQueuedSongLocked();
        nativeSetAutoBlocoTargetLocked(*boundaryTarget);
        g_nativeLastAutoQueueForPlayingId = playingId;
      }
    }
  }

  std::string queuedId;
  double queuedStart = 0.0;
  double queuedEnd = 0.0;
  bool queuedManual = false;
  bool hasQueue = false;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    queuedId = g_nativeQueuedSongId;
    queuedStart = g_nativeQueuedStart;
    queuedEnd = g_nativeQueuedEnd;
    queuedManual = g_nativeQueuedManual;
    hasQueue = !queuedId.empty() && queuedEnd > queuedStart + 0.0005;
  }

  if (!hasQueue || !SetEditCurPos2_ptr) return;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (g_nativeAutoStopEnabled && !queuedManual) return;
  }
  double sourceBoundaryStart = songStart;
  double sourceBoundaryEnd = songEnd;
  std::string sourceBoundaryId = playingId;
  nativeResolveQueueSourceBoundary(projectSongs, playingId, playPos, songStart, songEnd,
    sourceBoundaryStart, sourceBoundaryEnd, sourceBoundaryId);
  if (std::fabs(queuedStart - sourceBoundaryStart) <= 0.002 && std::fabs(queuedEnd - sourceBoundaryEnd) <= 0.002) return;

  // O seek antecipado so nasce no fim de uma regiao normal ou no fim do pai.
  // O limite individual de uma musica-filho nunca conclui a fila.
  const double remaining = sourceBoundaryEnd - playPos;
  if (remaining <= 0.0 || remaining > 0.5) return;

  const std::string seekSignature = sourceBoundaryId + "->" + queuedId + "@" + nativeNumber(sourceBoundaryStart, 3) + ":" + nativeNumber(queuedStart, 3);
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (g_nativeQueuedSeekSignature == seekSignature) return;
    g_nativeQueuedSeekSignature = seekSignature;
  }

  SetEditCurPos2_ptr(project, queuedStart, true, true);
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeQueueHandoffPlayId = queuedId;
    g_nativeQueueHandoffStart = queuedStart;
    g_nativeQueueHandoffEnd = queuedEnd;
    g_nativeQueueHandoffProtectUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
  }
  // FIX74: o seek ja faz a transição, mas garantimos estado PLAY depois dele.
  if (Main_OnCommand_ptr) Main_OnCommand_ptr(1007, 0); // Transport: Play
  if (UpdateArrange_ptr) UpdateArrange_ptr();
  g_nativeForceStateBuild.store(true);
}

static const NativeSongWindow* nativeResolveAutoStopTarget(const std::vector<NativeSongWindow>& activeItems, const std::string& playingId, double songStart, double songEnd)
{
  const int currentIndex = nativeFindCurrentIndexInActivePlaylist(activeItems, playingId, songStart, songEnd);
  bool autoplayEnabled = false;
  bool queuedManual = false;
  bool autoBlocoEnabled = false;
  int queuedPlaylistIndex = 0;
  std::string queuedSongId;
  std::string autoBlocoTargetSongId;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    autoplayEnabled = g_nativeAutoplayEnabled;
    queuedManual = g_nativeQueuedManual;
    autoBlocoEnabled = g_nativeAutoBlocoEnabled;
    queuedPlaylistIndex = g_nativeQueuedPlaylistIndex;
    queuedSongId = g_nativeQueuedSongId;
    autoBlocoTargetSongId = g_nativeAutoBlocoTargetSongId;
  }
  if (!autoplayEnabled || queuedManual) return nullptr;
  if (!queuedSongId.empty()) {
    for (const auto& item : activeItems) {
      if (!nativeSongIsPlayable(item)) continue;
      if ((queuedPlaylistIndex > 0 && item.playlistOrder == queuedPlaylistIndex) || nativeSongIdMatches(item, queuedSongId)) return &item;
    }
  }
  if (autoBlocoEnabled && !autoBlocoTargetSongId.empty()) {
    for (const auto& item : activeItems) {
      if (nativeSongIsPlayable(item) && nativeSongIdMatches(item, autoBlocoTargetSongId)) return &item;
    }
  }
  const NativeSongWindow* boundaryTarget = nullptr;
  const NativeSongWindow* nextTarget = nativeResolveNextAutoQueueTarget(activeItems, currentIndex, autoBlocoEnabled, &boundaryTarget);
  if (nextTarget) return nextTarget;
  if (autoBlocoEnabled && boundaryTarget) return boundaryTarget;
  return nullptr;
}

static void nativeMaintainAutoStop(
  ReaProject* project,
  bool playing,
  const std::string& playingId,
  double playPos,
  double songStart,
  double songEnd,
  const std::vector<NativeSongWindow>& activeItems,
  const std::vector<NativeSongWindow>& projectSongs)
{
  if (!project || !Main_OnCommand_ptr || !playing || nativeIsLuaControlActive()) return;

  std::string currentPlayingId = playingId;
  double currentSongStart = songStart;
  double currentSongEnd = songEnd;

  // Musicas-filhos continuam pertencendo ao mesmo bloco continuo do pai.
  // Mesmo quando o filho e uma regiao real da segunda ruler lane, nunca usa
  // o fim do filho como limite do Auto Stop: usa exclusivamente o fim do pai.
  const NativeSongWindow* playingChild = nullptr;
  double bestChildLength = 1e99;
  for (const auto& candidate : projectSongs) {
    if (!candidate.isHashChild || !nativeSongIsPlayable(candidate)) continue;
    if (playPos < candidate.start - 0.0005 || playPos >= candidate.end - 0.0005) continue;
    const double length = candidate.end - candidate.start;
    if (length < bestChildLength) {
      playingChild = &candidate;
      bestChildLength = length;
    }
  }
  if (playingChild) {
    for (const auto& candidate : projectSongs) {
      if (!candidate.isHashParent || !nativeSongIsPlayable(candidate)) continue;
      const bool sameParentId = !playingChild->parentId.empty() && candidate.id == playingChild->parentId;
      const bool sameParentEnum = playingChild->parentEnumIndex >= 0 && candidate.enumIndex == playingChild->parentEnumIndex;
      if ((sameParentId || sameParentEnum) && candidate.end > candidate.start + 0.0005) {
        currentSongStart = candidate.start;
        currentSongEnd = candidate.end;
        break;
      }
    }
  }

  if (!currentPlayingId.empty() && currentSongEnd > currentSongStart + 0.0005) {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeAutoStopLastPlayingId = currentPlayingId;
    g_nativeAutoStopLastPlayingStart = currentSongStart;
    g_nativeAutoStopLastPlayingEnd = currentSongEnd;
  } else {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (g_nativeAutoStopLastPlayingId.empty() || g_nativeAutoStopLastPlayingEnd <= g_nativeAutoStopLastPlayingStart + 0.0005) return;
    currentPlayingId = g_nativeAutoStopLastPlayingId;
    currentSongStart = g_nativeAutoStopLastPlayingStart;
    currentSongEnd = g_nativeAutoStopLastPlayingEnd;
  }

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeLoadAutomationSettingsOnceLocked();
    if (!g_nativeAutoStopEnabled) {
      g_nativeAutoStopLastStoppedSignature.clear();
      return;
    }
    if (g_nativeQueuedManual && !g_nativeQueuedSongId.empty() && g_nativeQueuedEnd > g_nativeQueuedStart + 0.0005) {
      return;
    }
  }

  const double remaining = currentSongEnd - playPos;
  if (remaining > 0.200 || remaining < -1.250) return;

  const std::string stopSignature = currentPlayingId + "@" + nativeNumber(currentSongStart, 3) + ":" + nativeNumber(currentSongEnd, 3);
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (g_nativeAutoStopLastStoppedSignature == stopSignature) return;
    g_nativeAutoStopLastStoppedSignature = stopSignature;
  }

  const NativeSongWindow* target = nativeResolveAutoStopTarget(activeItems, currentPlayingId, currentSongStart, currentSongEnd);
  double finalSelectionStart = currentSongStart;
  double finalSelectionEnd = currentSongEnd;
  std::string finalSelectionId = currentPlayingId;
  std::string finalSelectionTab = "playlist";
  int finalPlaylistIndex = 0;
  if (target && nativeSongIsPlayable(*target)) {
    finalSelectionId = target->id;
    finalSelectionStart = target->start;
    finalSelectionEnd = target->end;
    finalPlaylistIndex = target->playlistOrder;
  }

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeCancelQueueHandoffProtectionLocked();
  }
  Main_OnCommand_ptr(1016, 0); // Transport: Stop

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeSelectedId = finalSelectionId;
    g_nativeSelectedTab = finalSelectionTab;
    g_nativeSelectedStart = finalSelectionStart;
    g_nativeSelectedEnd = finalSelectionEnd;
    nativePublishStopReadyTargetLocked(finalSelectionId, finalSelectionTab, finalSelectionStart, finalSelectionEnd, finalPlaylistIndex);
    nativeCancelQueueHandoffProtectionLocked();
    g_nativeAutoStopLastStoppedSignature.clear();
  }

  if (SetEditCurPos2_ptr && finalSelectionStart >= 0.0) {
    SetEditCurPos2_ptr(project, finalSelectionStart, true, false);
  }
  if (UpdateArrange_ptr) UpdateArrange_ptr();
  g_nativeForceStateBuild.store(true);
}

static bool nativeIsRepeatEnabled(ReaProject* project)
{
  if (GetToggleCommandStateEx_ptr) {
    const int state = GetToggleCommandStateEx_ptr(0, 1068);
    if (state >= 0) return state == 1;
  }
  if (GetToggleCommandState_ptr) {
    const int state = GetToggleCommandState_ptr(1068);
    if (state >= 0) return state == 1;
  }
  if (GetSetRepeatEx_ptr) return GetSetRepeatEx_ptr(project, -1) == 1;
  return false;
}

static int nativeAppActiveReadWindowInt(const char* key, int fallback)
{
  if (!GetExtState_ptr || !key) return fallback;
  const char* raw = GetExtState_ptr(kLuaWindowExtStateSection, key);
  if (!raw || !*raw) return fallback;
  char* end = nullptr;
  const long value = std::strtol(raw, &end, 10);
  return end && end != raw ? static_cast<int>(value) : fallback;
}

static void nativeAppActiveWriteWindowInt(const char* key, int value)
{
  if (!SetExtState_ptr || !key) return;
  SetExtState_ptr(kLuaWindowExtStateSection, key, std::to_string(value).c_str(), true);
}

static void nativeAppActiveSaveWindowState()
{
  if (!nativeAppActivePanelIsOpen()) return;

  bool floatingDocker = false;
  int dockerIndex = -1;
  if (DockIsChildOfDock_ptr) {
    dockerIndex = DockIsChildOfDock_ptr(g_nativeAppActivePanelHwnd, &floatingDocker);
  }

  if (dockerIndex >= 0) {
    const int dockState = dockerIndex * 256 + 1;
    nativeAppActiveWriteWindowInt("DOCKSTATE_V1", dockState);
    nativeAppActiveWriteWindowInt("LAST_DOCKSTATE_V1", dockState);
    return;
  }

  RECT windowRect{0, 0, 0, 0};
  RECT clientRect{0, 0, 0, 0};
  if (!GetWindowRect(g_nativeAppActivePanelHwnd, &windowRect)) return;
#ifdef _WIN32
  if (!GetClientRect(g_nativeAppActivePanelHwnd, &clientRect)) return;
#else
  // No SWELL/macOS GetClientRect retorna void, ao contrario da API Win32.
  GetClientRect(g_nativeAppActivePanelHwnd, &clientRect);
#endif

  nativeAppActiveWriteWindowInt("DOCKSTATE_V1", 0);
  nativeAppActiveWriteWindowInt("WINDOW_X_V1", windowRect.left);
  nativeAppActiveWriteWindowInt("WINDOW_Y_V1", windowRect.top);
  nativeAppActiveWriteWindowInt("WINDOW_W_V1", std::max(300, static_cast<int>(clientRect.right - clientRect.left)));
  nativeAppActiveWriteWindowInt("WINDOW_H_V1", std::max(220, static_cast<int>(clientRect.bottom - clientRect.top)));
}

static void nativeAppActiveFillRoundRect(HDC dc, const RECT& rect, COLORREF fill, COLORREF border, int radius)
{
  HBRUSH brush = CreateSolidBrush(fill);
  HPEN pen = CreatePen(PS_SOLID, 1, border);
  HGDIOBJ oldBrush = SelectObject(dc, brush);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
  SelectObject(dc, oldPen);
  SelectObject(dc, oldBrush);
  DeleteObject(pen);
  DeleteObject(brush);
}

static void nativeAppActiveFillRect(HDC dc, const RECT& rect, COLORREF fill)
{
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

static void nativeAppActiveDrawBlackShadow(HDC dc, const RECT& rect)
{
  if (!dc || rect.right <= rect.left || rect.bottom <= rect.top) return;
#ifdef _WIN32
  using AlphaBlendProc = BOOL (WINAPI*)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);
  static HMODULE module = LoadLibraryW(L"msimg32.dll");
  static AlphaBlendProc alphaBlend = module
    ? reinterpret_cast<AlphaBlendProc>(GetProcAddress(module, "AlphaBlend"))
    : nullptr;
  if (alphaBlend) {
    HDC layer = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, 1, 1);
    HGDIOBJ oldBitmap = SelectObject(layer, bitmap);
    SetPixelV(layer, 0, 0, RGB(0, 0, 0));
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 176;
    alphaBlend(dc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      layer, 0, 0, 1, 1, blend);
    SelectObject(layer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(layer);
    return;
  }
#endif
  // Fallback SWELL: cobre dois de cada tres pixels para manter a sombra forte
  // sem exigir APIs graficas extras no macOS.
  for (int y = rect.top; y < rect.bottom; y += 3) {
    const LONG lineBottom = std::min<LONG>(rect.bottom, static_cast<LONG>(y + 2));
    RECT line{rect.left, y, rect.right, lineBottom};
    nativeAppActiveFillRect(dc, line, RGB(0, 0, 0));
  }
}

static void nativeAppActiveDrawText(HDC dc, const std::string& text, RECT rect, UINT flags, COLORREF color, HFONT font)
{
  HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, color);
#ifdef _WIN32
  const int required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (required > 0) {
    std::vector<wchar_t> wide(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), required);
    DrawTextW(dc, wide.data(), -1, &rect, flags);
  }
#else
  DrawText(dc, text.c_str(), -1, &rect, flags);
#endif
  if (oldFont) SelectObject(dc, oldFont);
}

static std::string nativeAppActiveSongNameForState(
  const std::vector<NativeSongWindow>& playlistItems,
  const std::vector<NativeSongWindow>& songs,
  const std::string& id,
  double start,
  double end)
{
  const NativeSongWindow* rangeMatch = nullptr;
  double rangeMatchDuration = 1.0e100;
  auto scan = [&](const std::vector<NativeSongWindow>& items) -> const NativeSongWindow* {
    for (const auto& item : items) {
      if (!nativeSongIsPlayable(item)) continue;
      if (!id.empty() && nativeSongIdMatches(item, id)) return &item;
      if (end > start + 0.0005 &&
          std::fabs(item.start - start) <= 0.003 && std::fabs(item.end - end) <= 0.003) {
        const double duration = item.end - item.start;
        if (!rangeMatch || duration < rangeMatchDuration) {
          rangeMatch = &item;
          rangeMatchDuration = duration;
        }
      }
    }
    return nullptr;
  };

  const NativeSongWindow* exact = scan(playlistItems);
  if (!exact) exact = scan(songs);
  const NativeSongWindow* result = exact ? exact : rangeMatch;
  return result ? nativeCleanSongName(result->name) : std::string();
}

static std::string nativeAppActiveArmedPartName(ReaProject* project, const std::string& markerId)
{
  if (!project || markerId.empty() || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return std::string();
  int wantedNumber = 0;
  std::string numericId = markerId;
  if (!numericId.empty() && (numericId[0] == 'm' || numericId[0] == 'M')) numericId.erase(numericId.begin());
  if (!nativeLooksNumeric(numericId)) return std::string();
  wantedNumber = std::atoi(numericId.c_str());

  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;
  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* rawName = nullptr;
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &rawName, &number, &color)) continue;
    if (isRegion || number != wantedNumber) continue;
    std::string name = nativeTrim(rawName ? rawName : "");
    if (nativeStartsWith(name, "*1") || nativeStartsWith(name, "*2")) name = nativeTrim(name.substr(2));
    else if (!name.empty() && (name[0] == '$' || name[0] == '!')) name = nativeTrim(name.substr(1));
    while (!name.empty() && (name[0] == '-' || name[0] == ':' || name[0] == ' ')) name.erase(name.begin());
    return nativeTrim(name);
  }
  return std::string();
}

static std::string nativeAppActiveFormatDuration(double seconds)
{
  if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
  const int total = static_cast<int>(std::floor(seconds + 0.5));
  const int hours = total / 3600;
  const int minutes = (total % 3600) / 60;
  const int secs = total % 60;
  char text[32] = "";
  if (hours > 0) std::snprintf(text, sizeof(text), "%d:%02d:%02d", hours, minutes, secs);
  else std::snprintf(text, sizeof(text), "%02d:%02d", minutes, secs);
  return std::string(text);
}

static bool nativeAppActiveColorFromHex(const std::string& raw, COLORREF& colorOut)
{
  std::string value = nativeTrim(raw);
  if (!value.empty() && value[0] == '#') value.erase(value.begin());
  if (value.size() != 6) return false;
  char* end = nullptr;
  const unsigned long rgb = std::strtoul(value.c_str(), &end, 16);
  if (!end || end != value.c_str() + value.size()) return false;
  colorOut = RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
  return true;
}

static COLORREF nativeDrawerColor(const std::string& raw)
{
  const std::string value = nativeLower(nativeTrim(raw));
  if (value == "green") return RGB(26, 255, 87);
  if (value == "blue") return RGB(51, 148, 255);
  if (value == "purple") return RGB(184, 82, 255);
  if (value == "red") return RGB(255, 56, 46);
  if (value == "orange") return RGB(255, 133, 20);
  if (value == "cyan") return RGB(31, 235, 255);
  if (value == "white") return RGB(235, 242, 255);
  if (value == "gray" || value == "grey") return RGB(143, 153, 168);
  return RGB(255, 240, 46);
}

static bool nativeAppActiveRowMatches(
  const NativeAppActivePanelModel::Row& row,
  const std::string& id,
  double start,
  double end)
{
  if (!id.empty()) return row.id == id;
  return end > start + 0.0005 &&
    std::fabs(row.start - start) <= 0.003 &&
    std::fabs(row.end - end) <= 0.003;
}

static bool nativeAppActiveRowsEqual(
  const std::vector<NativeAppActivePanelModel::Row>& left,
  const std::vector<NativeAppActivePanelModel::Row>& right)
{
  if (left.size() != right.size()) return false;
  for (size_t i = 0; i < left.size(); ++i) {
    const NativeAppActivePanelModel::Row& a = left[i];
    const NativeAppActivePanelModel::Row& b = right[i];
    if (a.id != b.id || a.name != b.name || a.parentId != b.parentId ||
        a.blockColorHex != b.blockColorHex || a.inheritedColorHex != b.inheritedColorHex ||
        std::fabs(a.start - b.start) > 0.0005 || std::fabs(a.end - b.end) > 0.0005 ||
        a.order != b.order || a.block != b.block || a.child != b.child) return false;
  }
  return true;
}

static void nativePaintAppActivePanel(HWND hwnd)
{
  PAINTSTRUCT ps{};
  HDC paintDc = BeginPaint(hwnd, &ps);
  if (!paintDc) return;

  RECT client{0, 0, 0, 0};
  GetClientRect(hwnd, &client);
  const int width = std::max(1, static_cast<int>(client.right - client.left));
  const int height = std::max(1, static_cast<int>(client.bottom - client.top));
  HDC dc = paintDc;
#ifdef _WIN32
  // Desenha o quadro inteiro fora da tela e o apresenta de uma vez. Isso evita
  // a piscada da janela durante progresso e sincronizacao de scroll.
  HDC bufferDc = CreateCompatibleDC(paintDc);
  HBITMAP bufferBitmap = bufferDc ? CreateCompatibleBitmap(paintDc, width, height) : nullptr;
  HGDIOBJ previousBitmap = (bufferDc && bufferBitmap) ? SelectObject(bufferDc, bufferBitmap) : nullptr;
  if (bufferDc && bufferBitmap) dc = bufferDc;
#endif
  nativeAppActiveFillRect(dc, client, RGB(5, 9, 18));

  const int pad = std::max(5, std::min(8, width / 55));
  const int gap = 4;
  const int headerH = 26;
  const int tabsH = 22;
  const int timerH = 24;
  const int cardH = 24;
  const int partsH = 20;
  const int buttonH = 44;
  const int rowH = 18;

  HFONT titleFont = CreateFont(-std::max(12, std::min(16, width / 30)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
  HFONT labelFont = CreateFont(-std::max(9, std::min(11, width / 42)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
  HFONT nameFont = CreateFont(-std::max(10, std::min(13, width / 34)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
  HFONT statusFont = CreateFont(-std::max(9, std::min(11, width / 42)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
  const int totalW = std::max(96, std::min(138, width / 3));
  RECT playlistHeader{pad, pad, width - pad - gap - totalW, pad + headerH};
  RECT totalHeader{playlistHeader.right + gap, pad, width - pad, pad + headerH};
  nativeAppActiveFillRoundRect(dc, playlistHeader, RGB(10, 16, 29), RGB(71, 85, 105), 5);
  nativeAppActiveFillRoundRect(dc, totalHeader, RGB(10, 16, 29), RGB(71, 85, 105), 5);
  RECT playlistHeaderText{playlistHeader.left + 8, playlistHeader.top + 2, playlistHeader.right - 8, playlistHeader.bottom - 2};
  RECT totalHeaderText{totalHeader.left + 5, totalHeader.top + 2, totalHeader.right - 5, totalHeader.bottom - 2};
  nativeAppActiveDrawText(dc, g_nativeAppActivePanelModel.playlistName, playlistHeaderText,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, RGB(250, 204, 21), titleFont);
  nativeAppActiveDrawText(dc, std::string("TOTAL: ") + g_nativeAppActivePanelModel.playlistTotalText, totalHeaderText,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, RGB(250, 204, 21), labelFont);

  int y = playlistHeader.bottom + gap;
  const int tabWidth = std::max(1, (width - pad * 2 - gap) / 2);
  RECT playlistTab{pad, y, pad + tabWidth, y + tabsH};
  RECT regionsTab{playlistTab.right + gap, y, width - pad, y + tabsH};
  nativeAppActiveFillRoundRect(dc, playlistTab,
    g_nativeAppActivePanelModel.regionsPage ? RGB(30, 41, 59) : RGB(22, 163, 74),
    g_nativeAppActivePanelModel.regionsPage ? RGB(71, 85, 105) : RGB(74, 222, 128), 5);
  nativeAppActiveFillRoundRect(dc, regionsTab,
    g_nativeAppActivePanelModel.regionsPage ? RGB(22, 163, 74) : RGB(30, 41, 59),
    g_nativeAppActivePanelModel.regionsPage ? RGB(74, 222, 128) : RGB(71, 85, 105), 5);
  nativeAppActiveDrawText(dc, "REPERTÓRIO", playlistTab,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, RGB(248, 250, 252), labelFont);
  nativeAppActiveDrawText(dc, "MÚSICAS", regionsTab,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, RGB(248, 250, 252), labelFont);
  y = playlistTab.bottom + gap;

  const int labelW = std::max(88, std::min(112, width / 3));
  RECT timerRect{pad, y, width - pad, y + timerH};
  const COLORREF timerAccent = !g_nativeAppActivePanelModel.timerVisible
    ? RGB(71, 85, 105)
    : (g_nativeAppActivePanelModel.timerExpired
      ? RGB(248, 75, 75)
      : (g_nativeAppActivePanelModel.timerMode == "countdown"
        ? RGB(250, 204, 21)
        : (g_nativeAppActivePanelModel.timerMode == "local_time" ? RGB(34, 211, 238) : RGB(26, 255, 87))));
  nativeAppActiveFillRoundRect(dc, timerRect, RGB(10, 18, 33), timerAccent, 6);
  RECT timerLabel{timerRect.left + 8, timerRect.top + 3, timerRect.left + 8 + labelW, timerRect.bottom - 3};
  nativeAppActiveDrawText(dc, "CRONÔMETRO", timerLabel,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
    g_nativeAppActivePanelModel.timerVisible ? timerAccent : RGB(148, 163, 184), labelFont);
  std::string timerStatus = "DESATIVADO";
  if (g_nativeAppActivePanelModel.timerVisible) {
    std::string timerModeLabel = "PROGRESSIVO";
    if (g_nativeAppActivePanelModel.timerMode == "countdown") timerModeLabel = "REGRESSIVO";
    else if (g_nativeAppActivePanelModel.timerMode == "local_time") timerModeLabel = "HORÁRIO LOCAL";
    timerStatus = timerModeLabel + "   |   " + g_nativeAppActivePanelModel.timerDisplayText;
  }
  RECT timerStatusRect{timerLabel.right + 6, timerRect.top + 3, timerRect.right - 8, timerRect.bottom - 3};
  nativeAppActiveDrawText(dc, timerStatus, timerStatusRect,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
    g_nativeAppActivePanelModel.timerVisible ? timerAccent : RGB(148, 163, 184), nameFont);
  y = timerRect.bottom + gap;

  auto drawProgressBar = [&](const RECT& card, double ratio, COLORREF color) {
    ratio = std::max(0.0, std::min(1.0, ratio));
    RECT track{card.left + 7, card.bottom - 4, card.right - 7, card.bottom - 2};
    nativeAppActiveFillRect(dc, track, RGB(51, 65, 85));
    if (ratio <= 0.0) return;
    const int fillW = static_cast<int>((track.right - track.left) * ratio);
    RECT fill = track;
    fill.right = fill.left + fillW;
    nativeAppActiveFillRect(dc, fill, color);
  };

  RECT playingRect{pad, y, width - pad, y + cardH};
  nativeAppActiveFillRoundRect(dc, playingRect, RGB(15, 23, 42), RGB(71, 85, 105), 6);
  drawProgressBar(playingRect, g_nativeAppActivePanelModel.progress, RGB(34, 197, 94));
  RECT playingLabel{playingRect.left + 8, playingRect.top + 4, playingRect.left + 8 + labelW, playingRect.bottom - 4};
  nativeAppActiveDrawText(dc, "TOCANDO AGORA", playingLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
    g_nativeAppActivePanelModel.playing ? RGB(26, 255, 87) : RGB(148, 163, 184), labelFont);
  RECT playingName{playingLabel.right + 6, playingRect.top + 4, playingRect.right - 8, playingRect.bottom - 4};
  nativeAppActiveDrawText(dc, g_nativeAppActivePanelModel.playingName, playingName,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
    g_nativeAppActivePanelModel.playing ? RGB(26, 255, 87) : RGB(248, 250, 252), nameFont);

  y = playingRect.bottom + gap;
  RECT queueRect{pad, y, width - pad, y + cardH};
  nativeAppActiveFillRoundRect(dc, queueRect, RGB(15, 23, 42), RGB(71, 85, 105), 6);
  drawProgressBar(queueRect,
    g_nativeAppActivePanelModel.queueActive ? (1.0 - g_nativeAppActivePanelModel.progress) : 0.0,
    RGB(250, 204, 21));
  RECT queueLabel{queueRect.left + 8, queueRect.top + 4, queueRect.left + 8 + labelW, queueRect.bottom - 4};
  nativeAppActiveDrawText(dc, "FILA DE ESPERA", queueLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
    g_nativeAppActivePanelModel.queueActive ? RGB(250, 204, 21) : RGB(148, 163, 184), labelFont);
  RECT queueName{queueLabel.right + 6, queueRect.top + 4, queueRect.right - 8, queueRect.bottom - 4};
  nativeAppActiveDrawText(dc, g_nativeAppActivePanelModel.queuedName, queueName,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
    g_nativeAppActivePanelModel.queueActive ? RGB(250, 204, 21) : RGB(248, 250, 252), nameFont);

  y = queueRect.bottom + gap;
  RECT partsRect{pad, y, width - pad, y + partsH};
  nativeAppActiveFillRoundRect(dc, partsRect, RGB(11, 26, 47), RGB(14, 116, 144), 6);
  RECT partsLabel{partsRect.left + 8, partsRect.top + 4, partsRect.left + 8 + labelW, partsRect.bottom - 4};
  nativeAppActiveDrawText(dc, "PARTS / LOOP", partsLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
    RGB(34, 211, 238), labelFont);
  std::string status;
  if (g_nativeAppActivePanelModel.partArmed) {
    status = "ENGATILHADA: ";
    status += g_nativeAppActivePanelModel.armedPartName.empty() ? "PART ENGATILHADA" : g_nativeAppActivePanelModel.armedPartName;
  }
  if (g_nativeAppActivePanelModel.loopActive) {
    if (!status.empty()) status += "   |   ";
    status += "EM LOOP";
    if (!g_nativeAppActivePanelModel.loopPartName.empty()) status += ": " + g_nativeAppActivePanelModel.loopPartName;
  }
  if (status.empty()) status = "SEM PARTE ENGATILHADA OU EM LOOP";
  RECT statusRect{partsLabel.right + 6, partsRect.top + 4, partsRect.right - 8, partsRect.bottom - 4};
  nativeAppActiveDrawText(dc, status, statusRect,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
    (g_nativeAppActivePanelModel.loopActive || g_nativeAppActivePanelModel.partArmed) ? RGB(250, 204, 21) : RGB(248, 250, 252), statusFont);

  const int partsBottom = static_cast<int>(partsRect.bottom);
  const int listTop = partsBottom + gap;
  const int listBottom = std::max(listTop, height - pad - buttonH - gap);
  RECT listRect{pad, listTop, width - pad, listBottom};
  nativeAppActiveFillRoundRect(dc, listRect, RGB(9, 14, 25), RGB(51, 65, 85), 4);

  const int listInnerTop = std::min(static_cast<int>(listRect.bottom) - 2,
    static_cast<int>(listRect.top) + 2);
  const int listInnerBottom = listRect.bottom - 2;
  const int viewportPixels = std::max(0, listInnerBottom - listInnerTop);
  const int visibleRows = viewportPixels > 0 ? (viewportPixels + rowH - 1) / rowH : 0;
  const int rowCount = static_cast<int>(g_nativeAppActivePanelModel.rows.size());
  const int maxScrollPixels = std::max(0, rowCount * rowH - viewportPixels);
  std::string focusSignature = g_nativeAppActivePanelModel.playlistName + "|" +
    g_nativeAppActivePanelModel.selectedId + "|" + g_nativeAppActivePanelModel.playingId + "|" +
    std::to_string(rowCount);
  const bool syncedScrollChanged =
    g_nativeAppActivePanelModel.listScrollRevision != g_nativeAppActiveLastAppliedScrollRevision;
  if (syncedScrollChanged) {
    g_nativeAppActiveLastAppliedScrollRevision = g_nativeAppActivePanelModel.listScrollRevision;
    // As duas telas podem exibir quantidades diferentes de linhas. A posicao
    // deve acompanhar a porcentagem percorrida, inclusive ao sair do final.
    g_nativeAppActiveListScrollPixels = static_cast<int>(std::floor(
      maxScrollPixels * std::max(0.0, std::min(1.0, g_nativeAppActivePanelModel.listScrollRatio)) + 0.5));
    g_nativeAppActiveListFocusSignature = focusSignature;
  } else if (focusSignature != g_nativeAppActiveListFocusSignature) {
    g_nativeAppActiveListFocusSignature = focusSignature;
    int focusIndex = -1;
    for (int i = 0; i < rowCount; ++i) {
      const NativeAppActivePanelModel::Row& row = g_nativeAppActivePanelModel.rows[static_cast<size_t>(i)];
      if (nativeAppActiveRowMatches(row, g_nativeAppActivePanelModel.selectedId,
          g_nativeAppActivePanelModel.selectedStart, g_nativeAppActivePanelModel.selectedEnd)) {
        focusIndex = i;
        break;
      }
    }
    if (focusIndex < 0) {
      for (int i = 0; i < rowCount; ++i) {
        const NativeAppActivePanelModel::Row& row = g_nativeAppActivePanelModel.rows[static_cast<size_t>(i)];
        if (nativeAppActiveRowMatches(row, g_nativeAppActivePanelModel.playingId,
            g_nativeAppActivePanelModel.playingStart, g_nativeAppActivePanelModel.playingEnd)) {
          focusIndex = i;
          break;
        }
      }
    }
    if (focusIndex >= 0) {
      // Move somente o necessario para revelar a selecao, como uma lista normal.
      const int focusTop = focusIndex * rowH;
      const int focusBottom = focusTop + rowH;
      if (focusTop < g_nativeAppActiveListScrollPixels) {
        g_nativeAppActiveListScrollPixels = focusTop;
      } else if (focusBottom > g_nativeAppActiveListScrollPixels + viewportPixels) {
        g_nativeAppActiveListScrollPixels = focusBottom - viewportPixels;
      }
    }
  }
  g_nativeAppActiveListScrollPixels = std::max(0, std::min(g_nativeAppActiveListScrollPixels, maxScrollPixels));
  g_nativeAppActiveListFirstRow = rowH > 0 ? g_nativeAppActiveListScrollPixels / rowH : 0;
  const int firstRowPixelOffset = rowH > 0 ? g_nativeAppActiveListScrollPixels % rowH : 0;

  if (rowCount <= 0 || visibleRows <= 0) {
    RECT emptyRect{listRect.left + 2, listInnerTop, listRect.right - 2, listInnerBottom};
    nativeAppActiveDrawText(dc, g_nativeAppActivePanelModel.regionsPage ? "MÚSICAS VAZIAS" : "REPERTÓRIO VAZIO", emptyRect,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, RGB(100, 116, 139), labelFont);
  } else {
    bool hasBlock = false;
    for (size_t i = 0; i < g_nativeAppActivePanelModel.rows.size(); ++i) {
      if (g_nativeAppActivePanelModel.rows[i].block) { hasBlock = true; break; }
    }
#ifdef _WIN32
    const int rowsClipState = SaveDC(dc);
    IntersectClipRect(dc, listRect.left + 2, listInnerTop, listRect.right - 7, listInnerBottom);
#else
    const RECT rowsClipRect{listRect.left + 2, listInnerTop, listRect.right - 7, listInnerBottom};
    SWELL_PushClipRegion(dc);
    SWELL_SetClipRegion(dc, &rowsClipRect);
#endif
    const int endRow = std::min(rowCount, g_nativeAppActiveListFirstRow + visibleRows + 1);
    for (int i = g_nativeAppActiveListFirstRow; i < endRow; ++i) {
      const NativeAppActivePanelModel::Row& row = g_nativeAppActivePanelModel.rows[static_cast<size_t>(i)];
      const int visualIndex = i - g_nativeAppActiveListFirstRow;
      RECT rowRect{listRect.left + 2, listInnerTop + visualIndex * rowH - firstRowPixelOffset,
        listRect.right - 8, listInnerTop + (visualIndex + 1) * rowH - firstRowPixelOffset};
      const bool isPlaying = nativeAppActiveRowMatches(row, g_nativeAppActivePanelModel.playingId,
        g_nativeAppActivePanelModel.playingStart, g_nativeAppActivePanelModel.playingEnd);
      const bool isQueued = nativeAppActiveRowMatches(row, g_nativeAppActivePanelModel.queuedId,
        g_nativeAppActivePanelModel.queuedStart, g_nativeAppActivePanelModel.queuedEnd);
      const bool isSelected = nativeAppActiveRowMatches(row, g_nativeAppActivePanelModel.selectedId,
        g_nativeAppActivePanelModel.selectedStart, g_nativeAppActivePanelModel.selectedEnd);
      const bool drawerFamilyTop = !row.child && i + 1 < rowCount &&
        g_nativeAppActivePanelModel.rows[static_cast<size_t>(i + 1)].child &&
        g_nativeAppActivePanelModel.rows[static_cast<size_t>(i + 1)].parentId == row.id;
      const bool drawerFamilyChild = row.child && !row.parentId.empty();
      const bool drawerFamilyBottom = drawerFamilyChild && (i + 1 >= rowCount ||
        !g_nativeAppActivePanelModel.rows[static_cast<size_t>(i + 1)].child ||
        g_nativeAppActivePanelModel.rows[static_cast<size_t>(i + 1)].parentId != row.parentId);
      const bool drawerFamilyMember = drawerFamilyTop || drawerFamilyChild;
      const COLORREF drawerOutlineColor = nativeDrawerColor(g_nativeAppActivePanelModel.drawerOutlineColor);
      const COLORREF drawerSymbolColor = nativeDrawerColor(g_nativeAppActivePanelModel.drawerSymbolColor);

      COLORREF rowFill = (i % 2 == 0) ? RGB(13, 20, 34) : RGB(18, 27, 43);
      COLORREF textColor = hasBlock ? RGB(248, 250, 252) : RGB(250, 204, 21);
      COLORREF parsedColor = RGB(0, 0, 0);
      if (row.block && nativeAppActiveColorFromHex(row.blockColorHex, parsedColor)) {
        rowFill = parsedColor;
        textColor = RGB(255, 255, 255);
      } else if (!row.block && nativeAppActiveColorFromHex(row.inheritedColorHex, parsedColor)) {
        textColor = parsedColor;
      }
      if (isQueued) { rowFill = RGB(230, 122, 41); textColor = RGB(5, 9, 18); }
      if (isSelected && !isPlaying) { rowFill = RGB(37, 99, 235); textColor = RGB(255, 255, 255); }
      if (isPlaying) { rowFill = RGB(184, 61, 61); textColor = RGB(5, 9, 18); }
      nativeAppActiveFillRect(dc, rowRect, rowFill);

      if (isPlaying || isQueued) {
        const double ratio = isPlaying ? g_nativeAppActivePanelModel.progress : (1.0 - g_nativeAppActivePanelModel.progress);
        const COLORREF barColor = isPlaying ? RGB(26, 255, 87) : RGB(250, 224, 21);
        const int fillWidth = std::max(0, static_cast<int>((rowRect.right - rowRect.left - 8) * std::max(0.0, std::min(1.0, ratio))));
        if (fillWidth > 0) {
          RECT bottomBar{rowRect.left + 4, rowRect.bottom - 3, rowRect.left + 4 + fillWidth, rowRect.bottom - 1};
          nativeAppActiveFillRect(dc, bottomBar, barColor);
        }
      }

      const int childIndent = row.child ? 15 : 0;
      if (row.child && g_nativeAppActivePanelModel.drawerSymbolEnabled) {
        RECT childGuide{rowRect.left + 7, rowRect.top + 4, rowRect.left + 9, rowRect.bottom - 4};
        nativeAppActiveFillRect(dc, childGuide, drawerSymbolColor);
      }
      const std::string durationText = row.block ? std::string() : nativeAppActiveFormatDuration(row.end - row.start);
      RECT durationRect{rowRect.right - 52, rowRect.top + 2, rowRect.right - 5, rowRect.bottom - 2};
      RECT nameRect{rowRect.left + 8 + childIndent, rowRect.top + 2, durationRect.left - 5, rowRect.bottom - 2};
      nativeAppActiveDrawText(dc, row.name, nameRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, textColor, statusFont);
      if (!durationText.empty()) {
        nativeAppActiveDrawText(dc, durationText, durationRect,
          DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, textColor, statusFont);
      }
      RECT separator{rowRect.left, rowRect.bottom - 1, rowRect.right, rowRect.bottom};
      nativeAppActiveFillRect(dc, separator, RGB(30, 41, 59));
      if (drawerFamilyMember && g_nativeAppActivePanelModel.drawerOutlineEnabled) {
        const int lineW = 2;
        const int capW = std::min(28, std::max(8, static_cast<int>(rowRect.right - rowRect.left) / 3));
        RECT leftSide{rowRect.left, rowRect.top, rowRect.left + lineW, rowRect.bottom};
        RECT rightSide{rowRect.right - lineW, rowRect.top, rowRect.right, rowRect.bottom};
        nativeAppActiveFillRect(dc, leftSide, drawerOutlineColor);
        nativeAppActiveFillRect(dc, rightSide, drawerOutlineColor);
        if (drawerFamilyTop) {
          RECT topLeft{rowRect.left, rowRect.top, rowRect.left + capW, rowRect.top + lineW};
          RECT topRight{rowRect.right - capW, rowRect.top, rowRect.right, rowRect.top + lineW};
          nativeAppActiveFillRect(dc, topLeft, drawerOutlineColor);
          nativeAppActiveFillRect(dc, topRight, drawerOutlineColor);
        }
        if (drawerFamilyBottom) {
          RECT bottomLeft{rowRect.left, rowRect.bottom - lineW, rowRect.left + capW, rowRect.bottom};
          RECT bottomRight{rowRect.right - capW, rowRect.bottom - lineW, rowRect.right, rowRect.bottom};
          nativeAppActiveFillRect(dc, bottomLeft, drawerOutlineColor);
          nativeAppActiveFillRect(dc, bottomRight, drawerOutlineColor);
        }
      }
    }
#ifdef _WIN32
    if (rowsClipState != 0) RestoreDC(dc, rowsClipState);
#else
    SWELL_PopClipRegion(dc);
#endif

    if (maxScrollPixels > 0) {
      const int trackTop = listInnerTop + 1;
      const int trackBottom = listRect.bottom - 3;
      RECT scrollTrack{listRect.right - 5, trackTop, listRect.right - 2, trackBottom};
      nativeAppActiveFillRect(dc, scrollTrack, RGB(30, 41, 59));
      const int trackH = std::max(1, trackBottom - trackTop);
      const int thumbH = std::max(14, static_cast<int>(trackH * (static_cast<double>(visibleRows) / rowCount)));
      const int travel = std::max(0, trackH - thumbH);
      const int thumbY = trackTop + static_cast<int>(travel *
        (static_cast<double>(g_nativeAppActiveListScrollPixels) / maxScrollPixels));
      RECT thumb{scrollTrack.left, thumbY, scrollTrack.right, thumbY + thumbH};
      nativeAppActiveFillRect(dc, thumb, RGB(100, 116, 139));
    }
  }

  // Enquanto o Diretor controla, toda a area informativa fica sob uma sombra.
  // O unico elemento livre e acionavel e o botao de retomar acesso ao PC.
  RECT readOnlyShadow{client.left, client.top, client.right, listRect.bottom};
  nativeAppActiveDrawBlackShadow(dc, readOnlyShadow);

  g_nativeAppActiveResumeButtonRect = RECT{pad, height - pad - buttonH, width - pad, height - pad};
  nativeAppActiveFillRoundRect(dc, g_nativeAppActiveResumeButtonRect, RGB(22, 163, 74), RGB(74, 222, 128), 6);
  nativeAppActiveDrawText(dc, "RETOMAR ACESSO NO PC", g_nativeAppActiveResumeButtonRect,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, RGB(255, 255, 255), nameFont);

  if (g_nativeAppActiveResumeConfirmOpen) {
    nativeAppActiveDrawBlackShadow(dc, client);
    const int modalW = std::max(280, std::min(width - pad * 2, 430));
    const int modalH = std::max(130, std::min(height - pad * 2, 156));
    const int modalLeft = (width - modalW) / 2;
    const int modalTop = (height - modalH) / 2;
    RECT modal{modalLeft, modalTop, modalLeft + modalW, modalTop + modalH};
    nativeAppActiveFillRoundRect(dc, modal, RGB(10, 16, 29), RGB(250, 204, 21), 9);

    RECT confirmTitle{modal.left + 14, modal.top + 11, modal.right - 14, modal.top + 38};
    nativeAppActiveDrawText(dc, "RETOMAR ACESSO NO PC?", confirmTitle,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, RGB(250, 204, 21), titleFont);
    RECT confirmText{modal.left + 18, confirmTitle.bottom + 2, modal.right - 18, modal.bottom - 51};
    nativeAppActiveDrawText(dc, "O app do Diretor sera deslogado.", confirmText,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
      RGB(226, 232, 240), labelFont);

    const int confirmGap = 7;
    const int confirmButtonW = (modalW - 28 - confirmGap) / 2;
    g_nativeAppActiveResumeConfirmNoRect = RECT{
      modal.left + 14, modal.bottom - 42, modal.left + 14 + confirmButtonW, modal.bottom - 12};
    g_nativeAppActiveResumeConfirmYesRect = RECT{
      g_nativeAppActiveResumeConfirmNoRect.right + confirmGap, modal.bottom - 42,
      modal.right - 14, modal.bottom - 12};
    nativeAppActiveFillRoundRect(dc, g_nativeAppActiveResumeConfirmNoRect,
      RGB(51, 65, 85), RGB(100, 116, 139), 6);
    nativeAppActiveDrawText(dc, "CANCELAR", g_nativeAppActiveResumeConfirmNoRect,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, RGB(255, 255, 255), labelFont);
    nativeAppActiveFillRoundRect(dc, g_nativeAppActiveResumeConfirmYesRect,
      RGB(22, 163, 74), RGB(74, 222, 128), 6);
    nativeAppActiveDrawText(dc, "SIM, RETOMAR", g_nativeAppActiveResumeConfirmYesRect,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, RGB(255, 255, 255), labelFont);
  } else {
    g_nativeAppActiveResumeConfirmYesRect = RECT{0, 0, 0, 0};
    g_nativeAppActiveResumeConfirmNoRect = RECT{0, 0, 0, 0};
  }

  DeleteObject(titleFont);
  DeleteObject(labelFont);
  DeleteObject(nameFont);
  DeleteObject(statusFont);
#ifdef _WIN32
  if (dc != paintDc) {
    BitBlt(paintDc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, previousBitmap);
    DeleteObject(bufferBitmap);
    DeleteDC(bufferDc);
  } else {
    if (bufferBitmap) DeleteObject(bufferBitmap);
    if (bufferDc) DeleteDC(bufferDc);
  }
#endif
  EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK nativeAppActivePanelWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    case WM_CREATE:
    case WM_INITDIALOG:
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      nativePaintAppActivePanel(hwnd);
      return 0;
    case WM_SIZE:
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_LBUTTONUP: {
      const POINT point{static_cast<short>(lParam & 0xffff), static_cast<short>((lParam >> 16) & 0xffff)};
      if (g_nativeAppActiveResumeConfirmOpen) {
        if (PtInRect(&g_nativeAppActiveResumeConfirmYesRect, point)) {
          resumePcAccessFromAppActiveScreen();
        } else if (PtInRect(&g_nativeAppActiveResumeConfirmNoRect, point)) {
          g_nativeAppActiveResumeConfirmOpen = false;
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
      }
      if (PtInRect(&g_nativeAppActiveResumeButtonRect, point)) {
        g_nativeAppActiveResumeConfirmOpen = true;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (wParam == VK_RETURN) {
        if (g_nativeAppActiveResumeConfirmOpen) {
          resumePcAccessFromAppActiveScreen();
        } else {
          g_nativeAppActiveResumeConfirmOpen = true;
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
      }
      if (wParam == VK_ESCAPE) {
        if (g_nativeAppActiveResumeConfirmOpen) {
          g_nativeAppActiveResumeConfirmOpen = false;
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
      }
      break;
    case WM_CLOSE:
      if (!g_nativeAppActivePanelClosing) {
        if (!g_nativeAppActiveResumeConfirmOpen) {
          g_nativeAppActiveResumeConfirmOpen = true;
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
      }
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
    case WM_NCDESTROY:
      if (g_nativeAppActivePanelHwnd == hwnd) g_nativeAppActivePanelHwnd = nullptr;
      return 0;
  }
#ifdef _WIN32
  // A classe desta janela e registrada com RegisterClassExW/CreateWindowExW.
  // Usar o DefWindowProc ANSI fazia o titulo Unicode ser interpretado como
  // bytes ("V\0S\0..."), deixando a janela solta com apenas "V".
  return DefWindowProcW(hwnd, message, wParam, lParam);
#else
  return 0;
#endif
}

static bool nativeAppActivePanelIsOpen()
{
  return g_nativeAppActivePanelHwnd && IsWindow(g_nativeAppActivePanelHwnd);
}

static bool nativeOpenAppActivePanel()
{
  if (nativeAppActivePanelIsOpen()) {
    if (DockWindowActivate_ptr) DockWindowActivate_ptr(g_nativeAppActivePanelHwnd);
    ShowWindow(g_nativeAppActivePanelHwnd, SW_SHOW);
    return true;
  }

  // Primeira abertura: Docker direito. Depois segue o estado salvo pelo VS Hook.
  const int savedDockState = nativeAppActiveReadWindowInt("DOCKSTATE_V1", 513);
  const int savedX = nativeAppActiveReadWindowInt("WINDOW_X_V1", 100);
  const int savedY = nativeAppActiveReadWindowInt("WINDOW_Y_V1", 100);
  const int savedW = std::max(360, nativeAppActiveReadWindowInt("WINDOW_W_V1", 460));
  const int savedH = std::max(260, nativeAppActiveReadWindowInt("WINDOW_H_V1", 586));

  HWND hwnd = nullptr;
#ifdef _WIN32
  static const wchar_t* kWindowClass = L"VS_HOOK_NATIVE_APP_ACTIVE_PANEL";
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  if (!GetClassInfoExW(g_pluginInstance, kWindowClass, &wc)) {
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = nativeAppActivePanelWndProc;
    wc.hInstance = g_pluginInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) return false;
  }

  RECT outer{0, 0, savedW, savedH};
  AdjustWindowRectEx(&outer, WS_OVERLAPPEDWINDOW, FALSE, 0);
  hwnd = CreateWindowExW(0, kWindowClass, L"VS Hook - APP ATIVO", WS_OVERLAPPEDWINDOW,
    savedX, savedY, outer.right - outer.left, outer.bottom - outer.top,
    GetMainHwnd_ptr ? GetMainHwnd_ptr() : nullptr, nullptr, g_pluginInstance, nullptr);
#else
  const char* resizableWindow = reinterpret_cast<const char*>(static_cast<INT_PTR>(0x400001));
  hwnd = CreateDialogParam(g_pluginInstance, resizableWindow, GetMainHwnd_ptr ? GetMainHwnd_ptr() : nullptr,
    reinterpret_cast<DLGPROC>(nativeAppActivePanelWndProc), 0);
  if (hwnd) {
    SetWindowText(hwnd, "VS Hook - APP ATIVO");
    SetWindowPos(hwnd, nullptr, savedX, savedY, savedW, savedH, SWP_NOZORDER | SWP_NOACTIVATE);
  }
#endif
  if (!hwnd) return false;

  g_nativeAppActivePanelHwnd = hwnd;
  g_nativeAppActivePanelClosing = false;
  g_nativeAppActiveResumeConfirmOpen = false;
  nativeRefreshAppActivePanelModel();

  if (savedDockState != 0 && DockWindowAdd_ptr) {
    const int dockerIndex = std::max(0, savedDockState >> 8);
    DockWindowAdd_ptr(hwnd, "VS Hook - APP ATIVO", dockerIndex, true);
    if (DockWindowActivate_ptr) DockWindowActivate_ptr(hwnd);
    if (DockWindowRefreshForHWND_ptr) DockWindowRefreshForHWND_ptr(hwnd);
  } else {
    ShowWindow(hwnd, SW_SHOW);
  }
  InvalidateRect(hwnd, nullptr, FALSE);
  return true;
}

static void nativeCloseAppActivePanel()
{
  if (!nativeAppActivePanelIsOpen()) {
    g_nativeAppActivePanelHwnd = nullptr;
    return;
  }

  HWND hwnd = g_nativeAppActivePanelHwnd;
  nativeAppActiveSaveWindowState();
  g_nativeAppActivePanelClosing = true;
  if (DockWindowRemove_ptr) DockWindowRemove_ptr(hwnd);
  if (IsWindow(hwnd)) DestroyWindow(hwnd);
  g_nativeAppActivePanelHwnd = nullptr;
  g_nativeAppActivePanelClosing = false;
  g_nativeAppActiveResumeConfirmOpen = false;
  g_nativeAppActiveResumeButtonRect = RECT{0, 0, 0, 0};
  g_nativeAppActiveResumeConfirmYesRect = RECT{0, 0, 0, 0};
  g_nativeAppActiveResumeConfirmNoRect = RECT{0, 0, 0, 0};
}

static void nativeRefreshAppActivePanelModel()
{
  if (!nativeAppActivePanelIsOpen()) return;

  NativeAppActivePanelModel next;
  std::vector<NativeSongWindow> songs;
  std::vector<NativeSongWindow> playlistItems;
  std::map<std::string, bool> openFamilyDrawers;
  std::string playingId;
  std::string queuedId;
  std::string selectedId;
  std::string armedMarkerId;
  std::string activePage;
  double playingStart = 0.0;
  double playingEnd = 0.0;
  double playPosition = 0.0;
  double queuedStart = 0.0;
  double queuedEnd = 0.0;
  double selectedStart = 0.0;
  double selectedEnd = 0.0;

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    songs = g_nativeSongWindows;
    playlistItems = g_nativeActivePlaylistItems;
    openFamilyDrawers = g_nativeDirectorOpenFamilyDrawers;
    next.playing = g_nativeCurrentTransportPlaying;
    playingId = g_nativeCurrentPlayingId;
    playingStart = g_nativeCurrentSongStart;
    playingEnd = g_nativeCurrentSongEnd;
    playPosition = g_nativeCurrentPlayPosition;
    queuedId = !g_nativeQueuedSongId.empty() ? g_nativeQueuedSongId : g_nativeAutoBlocoTargetSongId;
    if (!g_nativeQueuedSongId.empty()) {
      queuedStart = g_nativeQueuedStart;
      queuedEnd = g_nativeQueuedEnd;
    } else {
      queuedStart = g_nativeAutoBlocoTargetStart;
      queuedEnd = g_nativeAutoBlocoTargetEnd;
    }
    selectedId = g_nativeSelectedId;
    selectedStart = g_nativeSelectedStart;
    selectedEnd = g_nativeSelectedEnd;
    armedMarkerId = g_nativeArmedMarkerId;
    activePage = g_nativeDirectorActivePage;
    next.autoplayEnabled = g_nativeAutoplayEnabled;
    next.autoBlocoEnabled = g_nativeAutoBlocoEnabled;
    next.autoStopEnabled = g_nativeAutoStopEnabled;
    next.timerRunning = g_nativeTimerRunning;
    next.timerMode = nativeNormalizeTimerMode(g_nativeTimerMode);
    next.timerVisible = next.timerRunning || next.timerMode == "local_time";
    next.timerExpired = nativeTimerCountdownExpiredLocked();
    const double timerDisplaySec = nativeTimerDisplaySecLocked();
    next.timerDisplayText = next.timerMode == "local_time"
      ? nativeLocalTimeText()
      : nativeFormatTimerTextFromSeconds(timerDisplaySec, next.timerExpired);
    next.liveEnabled = g_nativeLiveMarkEnabled;
    next.previewMode = g_nativePreviewMode;
    next.drawerOutlineEnabled = g_nativeDrawerOutlineEnabled;
    next.drawerSymbolEnabled = g_nativeDrawerSymbolEnabled;
    next.drawerOutlineColor = g_nativeDrawerOutlineColor;
    next.drawerSymbolColor = g_nativeDrawerSymbolColor;
    next.listScrollRatio = activePage == "regions"
      ? g_nativeDirectorRegionsScrollRatio
      : g_nativeDirectorPlaylistScrollRatio;
    next.listScrollRevision = g_nativeDirectorListScrollRevision;
  }

  next.activePage = activePage.empty() ? "playlist" : activePage;
  next.regionsPage = next.activePage == "regions";
  next.mixerPage = next.activePage == "mixer";
  if (GetExtState_ptr) {
    const char* numberMode = GetExtState_ptr(kLuaWindowExtStateSection, "NUMBER_COLUMN_MODE_V1");
    next.numberRegionMode = numberMode && std::string(numberMode) == "region_id";
  }

  next.playingName = nativeAppActiveSongNameForState(playlistItems, songs, playingId, playingStart, playingEnd);
  if (next.playingName.empty()) next.playingName = "NENHUMA MUSICA EM REPRODUCAO";
  if (next.playing && playingEnd > playingStart + 0.0005) {
    next.progress = std::max(0.0, std::min(1.0, (playPosition - playingStart) / (playingEnd - playingStart)));
  }

  next.queuedName = nativeAppActiveSongNameForState(playlistItems, songs, queuedId, queuedStart, queuedEnd);
  next.queueActive = !queuedId.empty() && queuedEnd > queuedStart + 0.0005;
  if (next.queuedName.empty()) next.queuedName = "FILA DE ESPERA VAZIA";

  next.playingId = playingId;
  next.queuedId = queuedId;
  next.selectedId = selectedId;
  next.playingStart = playingStart;
  next.playingEnd = playingEnd;
  next.queuedStart = queuedStart;
  next.queuedEnd = queuedEnd;
  next.selectedStart = selectedStart;
  next.selectedEnd = selectedEnd;

  next.rows.reserve(songs.size() + playlistItems.size());
  int displayOrder = 0;
  const auto drawerIsOpen = [&](const std::string& parentId) {
    const auto found = openFamilyDrawers.find(parentId);
    return found != openFamilyDrawers.end() && found->second;
  };
  const auto appendRow = [&](const NativeSongWindow& item, const std::string& inheritedColorOverride = std::string()) {
    ++displayOrder;
    NativeAppActivePanelModel::Row row;
    row.id = item.id;
    row.name = nativeCleanSongName(item.name);
    if (row.name.empty()) row.name = item.isBlock ? "BLOCO" : "SEM NOME";
    row.parentId = item.parentId;
    row.blockColorHex = item.blockColorHex;
    row.inheritedColorHex = inheritedColorOverride.empty()
      ? item.inheritedBlockColorHex
      : inheritedColorOverride;
    row.start = item.start;
    row.end = item.end;
    row.order = next.regionsPage ? displayOrder : (item.playlistOrder > 0 ? item.playlistOrder : displayOrder);
    row.block = item.isBlock;
    row.child = item.isHashChild || item.isRegionChild;
    next.rows.push_back(std::move(row));
  };

  if (next.regionsPage) {
    for (size_t i = 0; i < songs.size(); ++i) {
      const NativeSongWindow& item = songs[i];
      if (item.isBlock) continue;
      if ((item.isHashChild || item.isRegionChild) && !drawerIsOpen(item.parentId)) continue;
      appendRow(item);
    }
  } else {
    std::map<std::string, bool> parentsInPlaylist;
    for (size_t i = 0; i < playlistItems.size(); ++i) {
      if (playlistItems[i].isHashParent && !playlistItems[i].id.empty()) {
        parentsInPlaylist[playlistItems[i].id] = true;
      }
    }

    for (size_t i = 0; i < playlistItems.size(); ++i) {
      const NativeSongWindow& item = playlistItems[i];
      const bool childOfListedParent = (item.isHashChild || item.isRegionChild) &&
        !item.parentId.empty() && parentsInPlaylist.find(item.parentId) != parentsInPlaylist.end();
      if (childOfListedParent) continue;

      appendRow(item);
      if (!item.isHashParent || !drawerIsOpen(item.id)) continue;

      // No Repertorio, os filhos pertencem ao pai mesmo quando apenas o pai foi
      // gravado na lista. Insere as regioes da lane 2 logo abaixo dele, igual ao app.
      for (size_t childIndex = 0; childIndex < songs.size(); ++childIndex) {
        const NativeSongWindow& child = songs[childIndex];
        if (!(child.isHashChild || child.isRegionChild) || child.parentId != item.id) continue;
        appendRow(child, item.inheritedBlockColorHex);
      }
    }
  }
  next.playlistTotalText = nativeFormatTotalTextFromSeconds(
    next.regionsPage ? nativeComputeRegionsTotalSec(songs) : nativeComputePlaylistTotalSec(playlistItems));

  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  if (next.mixerPage) {
    next.playlistName = "Groups/Tracks/Master";
  } else if (next.regionsPage) {
    next.playlistName = "Músicas";
  } else if (project) {
    next.playlistName = nativeTrim(nativeGetProjExtStateString(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1"));
  }
  if (next.playlistName.empty()) next.playlistName = "Músicas";
  next.loopActive = nativeIsRepeatEnabled(project);
  next.partArmed = !armedMarkerId.empty();
  if (next.partArmed) next.armedPartName = nativeAppActiveArmedPartName(project, armedMarkerId);
  if (next.loopActive) {
    double loopStart = 0.0;
    double loopEnd = 0.0;
    if (nativeGetLoopTimeRange(project, loopStart, loopEnd)) {
      const std::string startName = nativeFindLoopBoundaryLabel(project, loopStart);
      const std::string endName = nativeFindLoopBoundaryLabel(project, loopEnd);
      if (!startName.empty() && !endName.empty()) next.loopPartName = startName + " - " + endName;
      else if (!startName.empty()) next.loopPartName = startName;
      else if (!endName.empty()) next.loopPartName = endName;
      else next.loopPartName = nativeFindLoopRegionLabel(project, loopStart, loopEnd);
    }
  }

  const bool changed =
    next.playing != g_nativeAppActivePanelModel.playing ||
    next.queueActive != g_nativeAppActivePanelModel.queueActive ||
    next.loopActive != g_nativeAppActivePanelModel.loopActive ||
    next.partArmed != g_nativeAppActivePanelModel.partArmed ||
    next.regionsPage != g_nativeAppActivePanelModel.regionsPage ||
    next.mixerPage != g_nativeAppActivePanelModel.mixerPage ||
    next.autoplayEnabled != g_nativeAppActivePanelModel.autoplayEnabled ||
    next.autoBlocoEnabled != g_nativeAppActivePanelModel.autoBlocoEnabled ||
    next.autoStopEnabled != g_nativeAppActivePanelModel.autoStopEnabled ||
    next.timerRunning != g_nativeAppActivePanelModel.timerRunning ||
    next.timerVisible != g_nativeAppActivePanelModel.timerVisible ||
    next.timerExpired != g_nativeAppActivePanelModel.timerExpired ||
    next.timerMode != g_nativeAppActivePanelModel.timerMode ||
    next.timerDisplayText != g_nativeAppActivePanelModel.timerDisplayText ||
    next.liveEnabled != g_nativeAppActivePanelModel.liveEnabled ||
    next.numberRegionMode != g_nativeAppActivePanelModel.numberRegionMode ||
    next.previewMode != g_nativeAppActivePanelModel.previewMode ||
    next.drawerOutlineEnabled != g_nativeAppActivePanelModel.drawerOutlineEnabled ||
    next.drawerSymbolEnabled != g_nativeAppActivePanelModel.drawerSymbolEnabled ||
    next.drawerOutlineColor != g_nativeAppActivePanelModel.drawerOutlineColor ||
    next.drawerSymbolColor != g_nativeAppActivePanelModel.drawerSymbolColor ||
    next.activePage != g_nativeAppActivePanelModel.activePage ||
    std::fabs(next.progress - g_nativeAppActivePanelModel.progress) > 0.001 ||
    std::fabs(next.listScrollRatio - g_nativeAppActivePanelModel.listScrollRatio) > 0.000001 ||
    next.listScrollRevision != g_nativeAppActivePanelModel.listScrollRevision ||
    next.playlistName != g_nativeAppActivePanelModel.playlistName ||
    next.playlistTotalText != g_nativeAppActivePanelModel.playlistTotalText ||
    next.playingId != g_nativeAppActivePanelModel.playingId ||
    next.queuedId != g_nativeAppActivePanelModel.queuedId ||
    next.selectedId != g_nativeAppActivePanelModel.selectedId ||
    std::fabs(next.playingStart - g_nativeAppActivePanelModel.playingStart) > 0.0005 ||
    std::fabs(next.playingEnd - g_nativeAppActivePanelModel.playingEnd) > 0.0005 ||
    std::fabs(next.queuedStart - g_nativeAppActivePanelModel.queuedStart) > 0.0005 ||
    std::fabs(next.queuedEnd - g_nativeAppActivePanelModel.queuedEnd) > 0.0005 ||
    std::fabs(next.selectedStart - g_nativeAppActivePanelModel.selectedStart) > 0.0005 ||
    std::fabs(next.selectedEnd - g_nativeAppActivePanelModel.selectedEnd) > 0.0005 ||
    next.playingName != g_nativeAppActivePanelModel.playingName ||
    next.queuedName != g_nativeAppActivePanelModel.queuedName ||
    next.armedPartName != g_nativeAppActivePanelModel.armedPartName ||
    next.loopPartName != g_nativeAppActivePanelModel.loopPartName ||
    !nativeAppActiveRowsEqual(next.rows, g_nativeAppActivePanelModel.rows);
  if (!changed) return;

  g_nativeAppActivePanelModel = std::move(next);
  InvalidateRect(g_nativeAppActivePanelHwnd, nullptr, FALSE);
}

static void nativeSetRepeatEnabled(ReaProject* project, bool enabled)
{
  if (GetSetRepeatEx_ptr) GetSetRepeatEx_ptr(project, enabled ? 1 : 0);
  const bool current = nativeIsRepeatEnabled(project);
  if (current != enabled && Main_OnCommand_ptr) Main_OnCommand_ptr(1068, 0);
}

static void nativeSetLoopTimeRange(ReaProject* project, double start, double end)
{
  if (!GetSet_LoopTimeRange2_ptr) return;
  double timeStart = start;
  double timeEnd = end;
  GetSet_LoopTimeRange2_ptr(project, true, false, &timeStart, &timeEnd, false);
  double loopStart = start;
  double loopEnd = end;
  GetSet_LoopTimeRange2_ptr(project, true, true, &loopStart, &loopEnd, false);
}

static bool nativeGetLoopTimeRange(ReaProject* project, double& startOut, double& endOut)
{
  startOut = 0.0;
  endOut = 0.0;
  if (!project || !GetSet_LoopTimeRange2_ptr) return false;
  GetSet_LoopTimeRange2_ptr(project, false, true, &startOut, &endOut, false);
  if (endOut > startOut + 0.0005) return true;
  startOut = 0.0;
  endOut = 0.0;
  GetSet_LoopTimeRange2_ptr(project, false, false, &startOut, &endOut, false);
  return endOut > startOut + 0.0005;
}

static void nativeClearLoopTimeRange(ReaProject* project)
{
  nativeSetLoopTimeRange(project, 0.0, 0.0);
}

static std::string nativeLoopMarkerDisplayName(std::string name)
{
  name = nativeTrim(name);
  if (nativeStartsWith(name, "*1") || nativeStartsWith(name, "*2")) name = nativeTrim(name.substr(2));
  else if (nativeStartsWith(name, "$") || nativeStartsWith(name, "!")) name = nativeTrim(name.substr(1));
  while (!name.empty() && (name.front() == '-' || name.front() == ':' || name.front() == ' ' || name.front() == '\t')) name.erase(name.begin());
  return nativeTrim(name);
}

static std::string nativeFindLoopBoundaryLabel(ReaProject* project, double boundaryPos)
{
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return std::string();
  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;
  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* rawName = nullptr;
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &rawName, &number, &color)) continue;
    if (isRegion || std::fabs(pos - boundaryPos) > 0.001) continue;
    const std::string raw = rawName ? rawName : "";
    if (nativeStartsWith(nativeTrim(raw), "!")) continue;
    const std::string display = nativeLoopMarkerDisplayName(raw);
    if (!display.empty()) return display;
  }
  return std::string();
}

static std::string nativeFindLoopRegionLabel(ReaProject* project, double loopStart, double loopEnd)
{
  if (!project || loopEnd <= loopStart + 0.0005) return std::string();
  std::string ignoredMarkersJson;
  const std::vector<NativeSongWindow> songs = nativeCollectProjectSongs(project, ignoredMarkersJson);
  const double eps = 0.001;
  const NativeSongWindow* best = nullptr;
  for (const auto& song : songs) {
    if (!nativeSongIsPlayable(song) || song.isHashParent) continue;
    if (std::fabs(song.start - loopStart) > eps) continue;
    if (loopEnd > song.end + eps) continue;
    if (!best || (song.end - song.start) < (best->end - best->start)) best = &song;
  }
  return best ? nativeCleanSongName(best->name) : std::string();
}

static bool nativeFindTransitionLoopRange(ReaProject* project, double playPos, double& startOut, double& endOut)
{
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return false;

  std::string ignoredMarkersJson;
  const std::vector<NativeSongWindow> songs = nativeCollectProjectSongs(project, ignoredMarkersJson);
  const double eps = 0.0005;
  const NativeSongWindow* activeSong = nullptr;

  for (const auto& song : songs) {
    if (!song.isHashChild) continue;
    if (playPos >= song.start - eps && playPos < song.end - eps) { activeSong = &song; break; }
  }
  if (!activeSong) {
    for (const auto& song : songs) {
      if (song.isBlock || song.isHashParent) continue;
      if (playPos >= song.start - eps && playPos < song.end - eps) { activeSong = &song; break; }
    }
  }
  if (!activeSong) {
    for (const auto& song : songs) {
      if (song.isBlock) continue;
      if (playPos >= song.start - eps && playPos < song.end - eps) { activeSong = &song; break; }
    }
  }
  if (!activeSong || activeSong->end <= activeSong->start + eps) return false;

  std::vector<double> boundaries;
  boundaries.push_back(activeSong->start);

  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;
  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* rawName = nullptr;
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &rawName, &number, &color)) continue;
    if (isRegion) continue;
    const std::string markerName = nativeTrim(rawName ? rawName : "");
    if (nativeStartsWith(markerName, "!")) continue;
    if (pos > activeSong->start + eps && pos < activeSong->end - eps) boundaries.push_back(pos);
  }
  boundaries.push_back(activeSong->end);
  std::sort(boundaries.begin(), boundaries.end());
  boundaries.erase(std::unique(boundaries.begin(), boundaries.end(), [eps](double a, double b){ return std::fabs(a - b) <= eps; }), boundaries.end());
  if (boundaries.size() < 2) return false;

  for (size_t i = 0; i + 1 < boundaries.size(); ++i) {
    const double left = boundaries[i];
    const double right = boundaries[i + 1];
    if (right <= left + eps) continue;
    if (playPos >= left - eps && playPos < right - eps) {
      startOut = left;
      endOut = right;
      return true;
    }
  }

  const double left = boundaries[boundaries.size() - 2];
  const double right = boundaries.back();
  if (playPos <= right + eps && right > left + eps) {
    startOut = left;
    endOut = right;
    return true;
  }
  return false;
}

static void nativeBuildMixerTrackListsJson(ReaProject* project, std::string& tracksOut, std::string& groupsOut)
{
  std::ostringstream tracks;
  std::ostringstream groups;
  tracks << "[";
  groups << "[";
  if (!CountTracks_ptr || !GetTrack_ptr) {
    tracksOut = "[]";
    groupsOut = "[]";
    return;
  }
  bool firstTrack = true;
  bool firstGroup = true;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;
    const double folderDepth = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "I_FOLDERDEPTH") : 0.0;
    const std::string guid = nativeTrackGuid(track, i);
    const std::string name = nativeTrackName(track, i);
    const double vol = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "D_VOL") : 1.0;
    const bool mute = GetMediaTrackInfo_Value_ptr ? (GetMediaTrackInfo_Value_ptr(track, "B_MUTE") > 0.5) : false;
    const bool solo = GetMediaTrackInfo_Value_ptr ? (GetMediaTrackInfo_Value_ptr(track, "I_SOLO") > 0.5) : false;
    std::ostringstream row;
    row << "{";
    row << "\"id\":" << nativeJsonString(guid) << ",";
    row << "\"guid\":" << nativeJsonString(guid) << ",";
    row << "\"index\":" << (i + 1) << ",";
    row << "\"name\":" << nativeJsonString(name) << ",";
    row << "\"label\":" << nativeJsonString(name) << ",";
    row << "\"volume\":" << nativeNumber(vol) << ",";
    row << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(vol), 6) << ",";
    row << "\"db\":" << nativeNumber(nativeVolumeToDb(vol), 3) << ",";
    row << "\"displayScale\":\"db\",";
    row << "\"mute\":" << (mute ? "true" : "false") << ",";
    row << "\"solo\":" << (solo ? "true" : "false") << ",";
    row << "\"folderDepth\":" << static_cast<int>(folderDepth);
    row << "}";
    const std::string rowJson = row.str();

    if (!firstTrack) tracks << ",";
    firstTrack = false;
    tracks << rowJson;
    if (static_cast<int>(folderDepth) > 0) {
      if (!firstGroup) groups << ",";
      firstGroup = false;
      groups << rowJson;
    }
  }
  tracks << "]";
  groups << "]";
  tracksOut = tracks.str();
  groupsOut = groups.str();
}

static std::string nativeBuildMixerMasterJson(ReaProject* project)
{
  MediaTrack* master = GetMasterTrack_ptr ? GetMasterTrack_ptr(project) : nullptr;
  if (!master) return "null";
  const double vol = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(master, "D_VOL") : 1.0;
  const bool mute = GetMediaTrackInfo_Value_ptr ? (GetMediaTrackInfo_Value_ptr(master, "B_MUTE") > 0.5) : false;
  const bool solo = GetMediaTrackInfo_Value_ptr ? (GetMediaTrackInfo_Value_ptr(master, "I_SOLO") > 0.5) : false;
  std::ostringstream oss;
  oss << "{";
  oss << "\"id\":\"MASTER_TRACK\",\"guid\":\"MASTER_TRACK\",\"index\":0,";
  oss << "\"name\":\"MASTER\",\"label\":\"MASTER\",";
  oss << "\"volume\":" << nativeNumber(vol) << ",";
  oss << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(vol), 6) << ",";
  oss << "\"db\":" << nativeNumber(nativeVolumeToDb(vol), 3) << ",";
  oss << "\"displayScale\":\"db\",";
  oss << "\"mute\":" << (mute ? "true" : "false") << ",";
  oss << "\"solo\":" << (solo ? "true" : "false");
  oss << "}";
  return oss.str();
}

static std::string nativePremixUnescape(std::string value)
{
  struct Pair { const char* from; const char* to; };
  const Pair pairs[] = {{"%0D", "\r"}, {"%0A", "\n"}, {"%09", "\t"}, {"%25", "%"}};
  for (const auto& pair : pairs) {
    size_t pos = 0;
    while ((pos = value.find(pair.from, pos)) != std::string::npos) {
      value.replace(pos, std::strlen(pair.from), pair.to);
      pos += std::strlen(pair.to);
    }
  }
  return value;
}

struct NativePremixTrackState {
  bool mute = false;
  bool solo = false;
  double volume = 1.0;
  bool phase = false;
};

using NativePremixPreset = std::map<std::string, NativePremixTrackState>;
using NativePremixDatabase = std::map<std::string, NativePremixPreset>;

static std::vector<std::string> nativeSplitPremixRecords(const std::string& raw)
{
  std::vector<std::string> out;
  std::string current;
  for (char c : raw) {
    if (c == '\n' || c == '\r' || c == ';') {
      if (!current.empty()) out.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

static std::vector<std::string> nativeSplitTabs(const std::string& line)
{
  std::vector<std::string> fields;
  size_t start = 0;
  while (start <= line.size()) {
    const size_t pos = line.find('\t', start);
    fields.push_back(line.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
    if (pos == std::string::npos) break;
    start = pos + 1;
  }
  return fields;
}

static NativePremixDatabase nativeLoadPremixDatabase()
{
  NativePremixDatabase db;
  if (!GetExtState_ptr) return db;
  const char* rawPtr = GetExtState_ptr("JBKEYS_VSLIVE_WINDOW", "PREMIX_V1");
  const std::string raw = rawPtr ? rawPtr : "";
  for (const auto& line : nativeSplitPremixRecords(raw)) {
    const auto fields = nativeSplitTabs(line);
    if (fields.size() < 5) continue;
    const std::string key = nativePremixUnescape(fields[0]);
    const std::string guid = nativePremixUnescape(fields[1]);
    if (key.empty() || guid.empty()) continue;
    NativePremixTrackState state;
    state.mute = fields[2] == "1";
    state.solo = fields[3] == "1";
    state.volume = std::atof(fields[4].c_str());
    if (state.volume <= 0.0) state.volume = 1.0;
    state.phase = fields.size() >= 6 && fields[5] == "1";
    db[key][guid] = state;
  }
  return db;
}

static std::map<std::string, bool> nativeLoadPremixEnabledKeys()
{
  std::map<std::string, bool> enabled;
  if (!GetExtState_ptr) return enabled;
  const char* rawPtr = GetExtState_ptr("JBKEYS_VSLIVE_WINDOW", "PREMIX_ENABLED_V1");
  const std::string raw = rawPtr ? rawPtr : "";
  for (const auto& record : nativeSplitPremixRecords(raw)) {
    const std::string key = nativePremixUnescape(nativeTrim(record));
    if (!key.empty()) enabled[key] = true;
  }
  return enabled;
}

static bool nativePremixGlobalEnabled()
{
  if (!GetExtState_ptr) return false;
  const char* raw = GetExtState_ptr("JBKEYS_VSLIVE_WINDOW", "PREMIX_GLOBAL_ENABLED_V1");
  return raw && std::strcmp(raw, "1") == 0;
}

static const NativeSongWindow* nativeFindPremixSelectedSong(const std::vector<NativeSongWindow>& songs)
{
  std::string explicitPremixId;
  double explicitStart = 0.0;
  double explicitEnd = 0.0;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    explicitPremixId = g_nativePremixSelectedSongId;
    explicitStart = g_nativePremixSelectedSongStart;
    explicitEnd = g_nativePremixSelectedSongEnd;
  }

  if (!explicitPremixId.empty()) {
    const NativeSongWindow* fallback = nullptr;
    for (const auto& song : songs) {
      if (song.isBlock || song.id != explicitPremixId) continue;
      if (!fallback) fallback = &song;
      if (explicitEnd > explicitStart + 0.0005 &&
          std::fabs(song.start - explicitStart) <= 0.001 &&
          std::fabs(song.end - explicitEnd) <= 0.001) {
        return &song;
      }
      // Quando o comando não traz limites, prioriza a região pai para que
      // o Premix possa publicar a família inteira em vez de cair no primeiro filho.
      if (!(explicitEnd > explicitStart + 0.0005) && song.isHashParent) fallback = &song;
    }
    if (fallback) return fallback;
  }

  if (!g_nativeCurrentPlayingId.empty()) {
    for (const auto& song : songs) if (!song.isBlock && !song.isHashParent && song.id == g_nativeCurrentPlayingId) return &song;
  }
  if (!g_nativeSelectedId.empty()) {
    for (const auto& song : songs) if (!song.isBlock && !song.isHashParent && song.id == g_nativeSelectedId) return &song;
  }
  for (const auto& song : songs) if (!song.isBlock && !song.isHashParent) return &song;
  return nullptr;
}


static std::string nativeBuildPremixSongsJson(const std::vector<NativeSongWindow>& songs, const std::map<std::string, bool>& enabled)
{
  std::ostringstream oss;
  oss << "[";
  bool first = true;
  int index = 1;
  for (const auto& s : songs) {
    if (s.isBlock || s.isHashParent) continue;
    if (!first) oss << ",";
    first = false;
    const std::string key = nativePremixSongKey(s);
    NativeSongWindow copy = s;
    if (copy.playlistOrder <= 0) copy.playlistOrder = index;
    std::string base = nativeSongToJson(copy, index++);
    if (!base.empty() && base.back() == '}') base.pop_back();
    oss << base << ",\"premixEnabled\":" << (enabled.count(key) ? "true" : "false") << "}";
  }
  oss << "]";
  return oss.str();
}

static double nativePremixVolumeToRatio(double volume)
{
  if (!std::isfinite(volume) || volume <= 0.0000001) return 0.0;
  const double db = 20.0 * std::log10(volume);
  const double minDb = -90.0;
  const double maxDb = 12.0;
  const double zeroRatio = 0.76;
  const double curve = 1.35;
  if (db <= minDb) return 0.0;
  if (db <= 0.0) {
    const double t = std::max(0.0, std::min(1.0, (db - minDb) / (0.0 - minDb)));
    return std::max(0.0, std::min(zeroRatio, zeroRatio * std::pow(t, curve)));
  }
  return std::max(zeroRatio, std::min(1.0, zeroRatio + ((db / maxDb) * (1.0 - zeroRatio))));
}

static double nativePremixRatioToVolume(double ratio)
{
  if (!std::isfinite(ratio)) ratio = 0.0;
  ratio = std::max(0.0, std::min(1.0, ratio));
  if (ratio <= 0.0) return 0.0;
  const double minDb = -90.0;
  const double maxDb = 12.0;
  const double zeroRatio = 0.76;
  const double curve = 1.35;
  double db = 0.0;
  if (ratio <= zeroRatio) {
    const double t = std::pow(ratio / std::max(0.000001, zeroRatio), 1.0 / curve);
    db = minDb + (t * (0.0 - minDb));
  } else {
    db = ((ratio - zeroRatio) / std::max(0.000001, 1.0 - zeroRatio)) * maxDb;
  }
  return std::pow(10.0, db / 20.0);
}

static std::string nativeBuildPremixItemRowsJson(ReaProject* project, const NativeSongWindow* song)
{
  std::ostringstream oss;
  oss << "[";
  if (!project || !song || song->end <= song->start + 0.0005 ||
      !CountTracks_ptr || !GetTrack_ptr || !GetTrackNumMediaItems_ptr || !GetTrackMediaItem_ptr ||
      !GetMediaItemInfo_Value_ptr) {
    oss << "]";
    return oss.str();
  }

  const double eps = 0.001;
  bool first = true;
  int visualIndex = 0;
  const int trackCount = CountTracks_ptr(project);
  for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
    MediaTrack* track = GetTrack_ptr(project, trackIndex);
    if (!track) continue;
    const std::string trackName = nativeTrackName(track, trackIndex);
    const std::string trackGuid = nativeTrackGuid(track, trackIndex);
    const int itemCount = GetTrackNumMediaItems_ptr(track);
    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      MediaItem* item = GetTrackMediaItem_ptr(track, itemIndex);
      if (!item) continue;
      const double itemStart = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
      const double itemLength = std::max(0.0, GetMediaItemInfo_Value_ptr(item, "D_LENGTH"));
      const double itemEnd = itemStart + itemLength;
      if (song->isRegionChild) {
        // Stems do pai atravessam as regiões-filho. Na lane 2, o Premix usa
        // sobreposição; nas músicas normais continua usando o ponto inicial.
        if (itemEnd <= song->start + eps || itemStart >= song->end - eps) continue;
      } else if (itemStart < song->start - eps || itemStart >= song->end - eps) {
        continue;
      }
      const double volume = GetMediaItemInfo_Value_ptr(item, "D_VOL");
      const bool mute = GetMediaItemInfo_Value_ptr(item, "B_MUTE") > 0.5;
      ++visualIndex;
      const std::string fallbackId = std::string("premix_item_") + std::to_string(trackIndex + 1) + "_" + std::to_string(itemIndex + 1);
      const std::string guid = nativeMediaItemGuid(item, fallbackId);
      const std::string itemName = nativeTakeName(item, std::string("Item ") + std::to_string(visualIndex));
      if (!first) oss << ",";
      first = false;
      oss << "{";
      oss << "\"id\":" << nativeJsonString(guid) << ",";
      oss << "\"itemId\":" << nativeJsonString(guid) << ",";
      oss << "\"guid\":" << nativeJsonString(guid) << ",";
      oss << "\"name\":" << nativeJsonString(itemName) << ",";
      oss << "\"label\":" << nativeJsonString(itemName) << ",";
      oss << "\"trackName\":" << nativeJsonString(trackName) << ",";
      oss << "\"trackId\":" << nativeJsonString(trackGuid) << ",";
      oss << "\"trackGuid\":" << nativeJsonString(trackGuid) << ",";
      oss << "\"trackIndex\":" << (trackIndex + 1) << ",";
      oss << "\"itemIndex\":" << (itemIndex + 1) << ",";
      oss << "\"startPos\":" << nativeNumber(itemStart) << ",";
      oss << "\"endPos\":" << nativeNumber(itemStart + itemLength) << ",";
      oss << "\"volume\":" << nativeNumber(volume) << ",";
      oss << "\"volumeRatio\":" << nativeNumber(nativePremixVolumeToRatio(volume), 6) << ",";
      oss << "\"db\":" << nativeNumber(nativeVolumeToDb(volume), 3) << ",";
      oss << "\"mute\":" << (mute ? "true" : "false");
      oss << "}";
    }
  }
  oss << "]";
  return oss.str();
}

static std::vector<const NativeSongWindow*> nativePremixFamilyChildren(
  const std::vector<NativeSongWindow>& songs,
  const NativeSongWindow* parent)
{
  std::vector<const NativeSongWindow*> children;
  if (!parent || !parent->isHashParent) return children;
  for (const auto& song : songs) {
    if (!song.isHashChild || song.isBlock) continue;
    const bool sameParentEnum =
      parent->enumIndex >= 0 && song.parentEnumIndex >= 0 &&
      parent->enumIndex == song.parentEnumIndex;
    const bool sameParentId =
      !parent->id.empty() && !song.parentId.empty() &&
      parent->id == song.parentId;
    if (sameParentEnum || sameParentId) children.push_back(&song);
  }
  std::sort(children.begin(), children.end(), [](const NativeSongWindow* a, const NativeSongWindow* b) {
    if (!a || !b) return a != nullptr;
    if (std::fabs(a->start - b->start) > 0.0005) return a->start < b->start;
    return a->end < b->end;
  });
  return children;
}


static std::string nativeBuildPremixSongSectionsJson(
  ReaProject* project,
  const std::vector<const NativeSongWindow*>& children)
{
  std::ostringstream oss;
  oss << "[";
  bool first = true;
  int index = 1;
  for (const NativeSongWindow* child : children) {
    if (!child) continue;
    if (!first) oss << ",";
    first = false;
    const std::string rows = nativeBuildPremixItemRowsJson(project, child);
    oss << "{";
    oss << "\"id\":" << nativeJsonString(child->id) << ",";
    oss << "\"songId\":" << nativeJsonString(child->id) << ",";
    oss << "\"name\":" << nativeJsonString(child->name) << ",";
    oss << "\"label\":" << nativeJsonString(child->name) << ",";
    oss << "\"index\":" << index++ << ",";
    oss << "\"startPos\":" << nativeNumber(child->start) << ",";
    oss << "\"endPos\":" << nativeNumber(child->end) << ",";
    oss << "\"sourceNumber\":" << child->sourceNumber << ",";
    oss << "\"markerNumber\":" << child->sourceNumber << ",";
    oss << "\"markerEnumIndex\":" << child->enumIndex << ",";
    oss << "\"parentId\":" << nativeJsonString(child->parentId) << ",";
    oss << "\"isHashChild\":true,";
    oss << "\"items\":" << rows << ",";
    oss << "\"itemRows\":" << rows;
    oss << "}";
  }
  oss << "]";
  return oss.str();
}


static std::string nativeBuildPremixTrackRowsJson(ReaProject* project, const NativePremixPreset* preset, bool groupsOnly)
{
  std::ostringstream oss;
  oss << "[";
  if (!project || !CountTracks_ptr || !GetTrack_ptr) { oss << "]"; return oss.str(); }
  bool first = true;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;
    const int folderDepth = GetMediaTrackInfo_Value_ptr ? static_cast<int>(GetMediaTrackInfo_Value_ptr(track, "I_FOLDERDEPTH")) : 0;
    if (groupsOnly && folderDepth <= 0) continue;
    const std::string guid = nativeTrackGuid(track, i);
    const std::string name = nativeTrackName(track, i);
    NativePremixTrackState state;
    state.volume = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "D_VOL") : 1.0;
    state.mute = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "B_MUTE") > 0.5 : false;
    state.solo = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "I_SOLO") > 0.5 : false;
    state.phase = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "B_PHASE") > 0.5 : false;
    bool hasSaved = false;
    if (preset) {
      const auto it = preset->find(guid);
      if (it != preset->end()) { state = it->second; hasSaved = true; }
    }
    if (!first) oss << ",";
    first = false;
    oss << "{";
    oss << "\"id\":" << nativeJsonString(guid) << ",";
    oss << "\"guid\":" << nativeJsonString(guid) << ",";
    oss << "\"index\":" << (i + 1) << ",";
    oss << "\"name\":" << nativeJsonString(name) << ",";
    oss << "\"label\":" << nativeJsonString(name) << ",";
    oss << "\"volume\":" << nativeNumber(state.volume) << ",";
    oss << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(state.volume), 6) << ",";
    oss << "\"db\":" << nativeNumber(nativeVolumeToDb(state.volume), 3) << ",";
    oss << "\"mute\":" << (state.mute ? "true" : "false") << ",";
    oss << "\"solo\":" << (state.solo ? "true" : "false") << ",";
    oss << "\"phase\":" << (state.phase ? "true" : "false") << ",";
    oss << "\"folderDepth\":" << folderDepth << ",";
    oss << "\"saved\":" << (hasSaved ? "true" : "false");
    oss << "}";
  }
  oss << "]";
  return oss.str();
}

static bool nativeApplyPremixPresetToProject(ReaProject* project, const NativePremixPreset& preset)
{
  if (!project || !CountTracks_ptr || !GetTrack_ptr || !SetMediaTrackInfo_Value_ptr) return false;
  bool changed = false;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;
    const std::string guid = nativeTrackGuid(track, i);
    const auto it = preset.find(guid);
    if (it == preset.end()) continue;
    const auto& state = it->second;
    SetMediaTrackInfo_Value_ptr(track, "B_MUTE", state.mute ? 1.0 : 0.0);
    SetMediaTrackInfo_Value_ptr(track, "I_SOLO", state.solo ? 1.0 : 0.0);
    SetMediaTrackInfo_Value_ptr(track, "D_VOL", state.volume);
    SetMediaTrackInfo_Value_ptr(track, "B_PHASE", state.phase ? 1.0 : 0.0);
    changed = true;
  }
  return changed;
}

static void nativeApplyPremixOnSongStart(ReaProject* project, const NativeSongWindow* song)
{
  if (!project || !song) return;
  const NativePremixDatabase db = nativeLoadPremixDatabase();
  const auto enabled = nativeLoadPremixEnabledKeys();
  bool changed = false;
  if (nativePremixGlobalEnabled()) {
    const auto globalIt = db.find("__GLOBAL_PREMIX__");
    if (globalIt != db.end()) changed = nativeApplyPremixPresetToProject(project, globalIt->second) || changed;
  }
  const std::string key = nativePremixSongKey(*song);
  if (enabled.count(key)) {
    const auto it = db.find(key);
    if (it != db.end()) changed = nativeApplyPremixPresetToProject(project, it->second) || changed;
  }
  if (changed) {
    if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
    if (UpdateArrange_ptr) UpdateArrange_ptr();
  }
}

static std::string nativeBuildPremixJson(ReaProject* project, const std::vector<NativeSongWindow>& songs)
{
  const NativePremixDatabase db = nativeLoadPremixDatabase();
  const auto enabled = nativeLoadPremixEnabledKeys();
  const NativeSongWindow* selected = nativeFindPremixSelectedSong(songs);
  const std::string selectedKey = selected ? nativePremixSongKey(*selected) : std::string();
  const NativePremixPreset* preset = nullptr;
  const auto it = db.find(selectedKey);
  if (it != db.end()) preset = &it->second;

  // No formato atual, músicas-filho são regiões reais da ruler lane 2 e já
  // fazem parte de `songs`. O Premix não recria mais filhos a partir de markers.
  std::vector<const NativeSongWindow*> familyChildren =
    nativePremixFamilyChildren(songs, selected);
  const bool familyMode = selected && !familyChildren.empty();
  const std::string itemRows = familyMode
    ? "[]"
    : nativeBuildPremixItemRowsJson(project, selected);
  const std::string songSections = familyMode
    ? nativeBuildPremixSongSectionsJson(project, familyChildren)
    : "[]";

  std::ostringstream oss;
  oss << "{";
  oss << "\"available\":true,\"enabled\":true,\"mode\":\"native\",";
  oss << "\"selectedSongId\":" << nativeJsonString(selected ? selected->id : "") << ",";
  oss << "\"selectedSongName\":" << nativeJsonString(selected ? selected->name : "") << ",";
  oss << "\"selectedSongStart\":" << nativeNumber(selected ? selected->start : 0.0) << ",";
  oss << "\"selectedSongEnd\":" << nativeNumber(selected ? selected->end : 0.0) << ",";
  oss << "\"selectedIsHashParent\":" << ((selected && selected->isHashParent) ? "true" : "false") << ",";
  oss << "\"selectedIsPremixParent\":" << (familyMode ? "true" : "false") << ",";
  oss << "\"familyMode\":" << (familyMode ? "true" : "false") << ",";
  oss << "\"selectedPremixKey\":" << nativeJsonString(selectedKey) << ",";
  oss << "\"globalEnabled\":" << (nativePremixGlobalEnabled() ? "true" : "false") << ",";
  oss << "\"songs\":" << nativeBuildPremixSongsJson(songs, enabled) << ",";
  oss << "\"sections\":" << songSections << ",";
  oss << "\"songSections\":" << songSections << ",";
  oss << "\"items\":" << itemRows << ",";
  oss << "\"itemRows\":" << itemRows << ",";
  oss << "\"tracks\":" << nativeBuildPremixTrackRowsJson(project, preset, false) << ",";
  oss << "\"groups\":" << nativeBuildPremixTrackRowsJson(project, preset, true);
  oss << "}";
  return oss.str();
}

static std::string nativeMultiLoopEscape(const std::string& value)
{
  std::string out;
  out.reserve(value.size() + 8);
  for (char ch : value) {
    if (ch == '\\') out += "\\\\";
    else if (ch == '\t') out += "\\t";
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else out.push_back(ch);
  }
  return out;
}

static std::string nativeMultiLoopUnescape(const std::string& value)
{
  std::string out;
  out.reserve(value.size());
  bool escaped = false;
  for (char ch : value) {
    if (!escaped && ch == '\\') { escaped = true; continue; }
    if (escaped) {
      if (ch == 't') out.push_back('\t');
      else if (ch == 'n') out.push_back('\n');
      else if (ch == 'r') out.push_back('\r');
      else out.push_back(ch);
      escaped = false;
    } else out.push_back(ch);
  }
  if (escaped) out.push_back('\\');
  return out;
}

static std::vector<std::string> nativeMultiLoopFields(const std::string& line)
{
  std::vector<std::string> fields;
  size_t begin = 0;
  while (begin <= line.size()) {
    const size_t end = line.find('\t', begin);
    fields.push_back(line.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return fields;
}

static std::string nativeMultiLoopProjectStorageKey()
{
  char path[2048] = "";
  ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));
  std::string identity = normalizeSlashes(path);
  if (identity.empty()) identity = std::string("unsaved|") + std::to_string(reinterpret_cast<std::uintptr_t>(project));
  uint32_t hash = 2166136261u;
  for (unsigned char ch : identity) { hash ^= ch; hash *= 16777619u; }
  std::ostringstream out;
  out << kNativeMultiLoopKeyPrefix << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << hash;
  return out.str();
}

static std::string nativeMultiLoopProjectMsStorageKey()
{
  const std::string mainKey = nativeMultiLoopProjectStorageKey();
  const std::string prefix = kNativeMultiLoopKeyPrefix;
  const std::string suffix = mainKey.rfind(prefix, 0) == 0 ? mainKey.substr(prefix.size()) : mainKey;
  return std::string(kNativeMultiLoopMsKeyPrefix) + suffix;
}

static void nativeParseMultiLoopState(const std::string& raw)
{
  g_nativeMultiLoopStates.clear();
  size_t begin = 0;
  while (begin <= raw.size()) {
    const size_t end = raw.find('\n', begin);
    const std::string line = raw.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
    const std::vector<std::string> f = nativeMultiLoopFields(line);
    if (f.size() >= 6) {
      const std::string key = nativeMultiLoopUnescape(f[1]);
      if (!key.empty()) {
        NativeMultiLoopSongState& state = g_nativeMultiLoopStates[key];
        if (f[0] == "E") {
          state.loop[0] = f[2] == "1"; state.loop[1] = f[3] == "1";
          state.ms[0] = f[4] == "1"; state.ms[1] = f[5] == "1";
        } else if ((f[0] == "P" || f[0] == "F" || f[0] == "L") && nativeLooksNumeric(f[2])) {
          const int slot = std::max(0, std::min(1, std::atoi(f[2].c_str()) - 1));
          const std::string guid = nativeMultiLoopUnescape(f[3]);
          if (!guid.empty()) {
            NativeMultiLoopTrackPreset& row = state.tracks[slot][guid];
            if (f[0] == "P") {
              row.mute = f[4] == "1";
              row.solo = f[5] == "1";
              if (row.mute) { row.solo = false; row.autoFader = false; }
            } else if (f[0] == "F") {
              row.autoFader = f[4] == "1" || f[5] == "1";
              if (row.autoFader) row.mute = false;
            } else {
              row.fadeLimitSet = true;
              row.fadeLimitDb = std::max(-90.0, std::min(150.0, std::atof(f[4].c_str())));
            }
          }
        } else if (f[0] == "T" && nativeLooksNumeric(f[2])) {
          const int slot = std::max(0, std::min(1, std::atoi(f[2].c_str()) - 1));
          state.fadeSec[slot] = std::max(1, std::min(5, std::atoi(f[3].c_str())));
        }
      }
    }
    if (end == std::string::npos) break;
    begin = end + 1;
  }
}

static void nativeSaveMultiLoopState();

static void nativeLoadMultiLoopState()
{
  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  const std::string storageKey = nativeMultiLoopProjectStorageKey();
  const std::string msStorageKey = nativeMultiLoopProjectMsStorageKey();
  const bool projectChanged = project != g_nativeMultiLoopProject;
  std::string mainRaw = nativeGetProjExtStateString(project, kNativeMultiLoopProjectSection, kNativeMultiLoopProjectStateKey);
  std::string msRaw = nativeGetProjExtStateString(project, kNativeMultiLoopProjectSection, kNativeMultiLoopProjectMsStateKey);
  bool migratedFromGlobal = false;

  // Migra uma única vez o estado global usado pelas versões anteriores.
  if (mainRaw.empty() && msRaw.empty() && GetExtState_ptr) {
    const char* oldMain = GetExtState_ptr(kNativeExtStateSection, storageKey.c_str());
    const char* oldMs = GetExtState_ptr(kNativeExtStateSection, msStorageKey.c_str());
    if (oldMain) mainRaw = oldMain;
    if (oldMs) msRaw = oldMs;
    migratedFromGlobal = !mainRaw.empty() || !msRaw.empty();
  }
  std::string raw = mainRaw;
  if (!msRaw.empty()) { if (!raw.empty()) raw += "\n"; raw += msRaw; }

  // Migração única do formato antigo compartilhado com o Lua. Depois disso,
  // somente a extensão grava a chave nativa específica deste projeto.
  if (raw.empty() && GetExtState_ptr) {
    const char* migratedPtr = GetExtState_ptr(kNativeExtStateSection, kNativeMultiLoopLegacyMigratedKey);
    const bool alreadyMigrated = migratedPtr && *migratedPtr == '1';
    if (!alreadyMigrated) {
      const char* legacyPtr = GetExtState_ptr(kLuaWindowExtStateSection, kLuaWindowMultiLoopKey);
      if (legacyPtr && *legacyPtr) { raw = legacyPtr; migratedFromGlobal = true; }
      if (SetExtState_ptr) SetExtState_ptr(kNativeExtStateSection, kNativeMultiLoopLegacyMigratedKey, "1", true);
    }
  }

  if (projectChanged || raw != g_nativeMultiLoopRaw) {
    g_nativeMultiLoopStorageKey = storageKey;
    g_nativeMultiLoopMsStorageKey = msStorageKey;
    g_nativeMultiLoopProject = project;
    g_nativeMultiLoopRaw = raw;
    nativeParseMultiLoopState(raw);
    if (migratedFromGlobal && project) nativeSaveMultiLoopState();
  }

  // O Lua publica somente quando o usuário altera algo. Ele não consulta esta
  // chave durante o uso; a extensão consome o evento e continua dona do estado.
  if (GetExtState_ptr) {
    const char* seqPtr = GetExtState_ptr(kNativeExtStateSection, kNativeMultiLoopLuaPushSequenceKey);
    const std::string sequence = seqPtr ? seqPtr : "";
    if (!sequence.empty() && sequence != g_nativeMultiLoopLuaPushSequence) {
      g_nativeMultiLoopLuaPushSequence = sequence;
      const char* projectPtr = GetExtState_ptr(kNativeExtStateSection, kNativeMultiLoopLuaPushProjectKey);
      const char* pushPtr = GetExtState_ptr(kNativeExtStateSection, kNativeMultiLoopLuaPushKey);
      if (projectPtr && storageKey == projectPtr && pushPtr) {
        nativeParseMultiLoopState(pushPtr);
        nativeSaveMultiLoopState();
      }
    }
  }
}

static void nativeSaveMultiLoopState()
{
  std::vector<std::string> mainLines;
  std::vector<std::string> msLines;
  for (const auto& songEntry : g_nativeMultiLoopStates) {
    const std::string key = nativeMultiLoopEscape(songEntry.first);
    const NativeMultiLoopSongState& state = songEntry.second;
    if (state.loop[0] || state.loop[1] || state.ms[0] || state.ms[1]) {
      mainLines.push_back("E\t" + key + "\t" + (state.loop[0] ? "1" : "0") + "\t" + (state.loop[1] ? "1" : "0") + "\t" + (state.ms[0] ? "1" : "0") + "\t" + (state.ms[1] ? "1" : "0"));
    }
    for (int slot = 0; slot < 2; ++slot) {
      if (state.fadeSec[slot] != 3) msLines.push_back("T\t" + key + "\t" + std::to_string(slot + 1) + "\t" + std::to_string(state.fadeSec[slot]) + "\t0\t0");
      for (const auto& trackEntry : state.tracks[slot]) {
        const NativeMultiLoopTrackPreset& row = trackEntry.second;
        const std::string guid = nativeMultiLoopEscape(trackEntry.first);
        if (row.mute || row.solo) msLines.push_back("P\t" + key + "\t" + std::to_string(slot + 1) + "\t" + guid + "\t" + (row.mute ? "1" : "0") + "\t" + (row.solo ? "1" : "0"));
        if (row.autoFader) msLines.push_back("F\t" + key + "\t" + std::to_string(slot + 1) + "\t" + guid + "\t1\t1");
        if (row.fadeLimitSet) msLines.push_back("L\t" + key + "\t" + std::to_string(slot + 1) + "\t" + guid + "\t" + nativeNumber(row.fadeLimitDb, 3) + "\t0");
      }
    }
  }
  std::sort(mainLines.begin(), mainLines.end());
  std::sort(msLines.begin(), msLines.end());
  std::ostringstream mainRaw;
  std::ostringstream msRaw;
  for (size_t i = 0; i < mainLines.size(); ++i) { if (i) mainRaw << "\n"; mainRaw << mainLines[i]; }
  for (size_t i = 0; i < msLines.size(); ++i) { if (i) msRaw << "\n"; msRaw << msLines[i]; }
  g_nativeMultiLoopRaw = mainRaw.str();
  if (!msRaw.str().empty()) { if (!g_nativeMultiLoopRaw.empty()) g_nativeMultiLoopRaw += "\n"; g_nativeMultiLoopRaw += msRaw.str(); }
  if (g_nativeMultiLoopStorageKey.empty()) g_nativeMultiLoopStorageKey = nativeMultiLoopProjectStorageKey();
  if (g_nativeMultiLoopMsStorageKey.empty()) g_nativeMultiLoopMsStorageKey = nativeMultiLoopProjectMsStorageKey();
  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  g_nativeMultiLoopProject = project;
  if (project && SetProjExtState_ptr) {
    SetProjExtState_ptr(project, kNativeMultiLoopProjectSection, kNativeMultiLoopProjectStateKey, mainRaw.str().c_str());
    SetProjExtState_ptr(project, kNativeMultiLoopProjectSection, kNativeMultiLoopProjectMsStateKey, msRaw.str().c_str());
    if (MarkProjectDirty_ptr) MarkProjectDirty_ptr(project);
  }
}

static NativeMultiLoopPair nativeFindMultiLoopPair(ReaProject* project, const std::string& songKey, double songStart, double songEnd, int slot)
{
  NativeMultiLoopPair pair;
  if (!project || songKey.empty() || songEnd <= songStart + 0.001 || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return pair;
  static ReaProject* cacheProject = nullptr;
  static uint64_t cacheRevision = 0;
  static std::map<std::string, NativeMultiLoopPair> cache;
  if (cacheProject != project || cacheRevision != g_nativeMultiLoopPairCacheRevision) { cacheProject = project; cacheRevision = g_nativeMultiLoopPairCacheRevision; cache.clear(); }
  std::ostringstream cacheKeyStream;
  cacheKeyStream << songKey << "|" << slot << "|" << std::fixed << std::setprecision(6) << songStart << "|" << songEnd;
  const std::string cacheKey = cacheKeyStream.str();
  const auto cached = cache.find(cacheKey);
  if (cached != cache.end()) return cached->second;
  const std::string prefix = slot == 1 ? "*2" : "*1";
  std::vector<double> points;
  int markerCount = 0, regionCount = 0;
  const int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  for (int i = 0; i < total; ++i) {
    bool isRegion = false; double pos = 0.0, end = 0.0; const char* rawName = nullptr; int number = 0, color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &rawName, &number, &color) || isRegion) continue;
    const std::string name = nativeTrim(rawName ? rawName : "");
    if (pos > songStart + 0.0005 && pos < songEnd - 0.0005 && nativeStartsWith(name, prefix)) points.push_back(pos);
  }
  std::sort(points.begin(), points.end());
  if (points.size() < 2 || points[1] <= points[0] + 0.0005) { cache[cacheKey] = pair; return pair; }
  pair.valid = true; pair.songKey = songKey; pair.slot = slot; pair.start = points[0]; pair.end = points[1];
  std::ostringstream key; key << songKey << "|" << (slot + 1) << "|" << std::fixed << std::setprecision(6) << pair.start << "|" << pair.end;
  pair.key = key.str();
  cache[cacheKey] = pair;
  return pair;
}

static MediaTrack* nativeFindTrackByGuid(ReaProject* project, const std::string& guid)
{
  if (!project || guid.empty() || !CountTracks_ptr || !GetTrack_ptr) return nullptr;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (track && nativeTrackGuid(track, i) == guid) return track;
  }
  return nullptr;
}

static double nativeMultiLoopFadeTargetVolume(double originalVolume, const NativeMultiLoopTrackPreset& preset)
{
  if (!preset.fadeLimitSet || preset.fadeLimitDb <= -89.999) return 0.0;
  const double targetDb = std::max(-90.0, std::min(150.0, preset.fadeLimitDb));
  const double requestedTarget = std::pow(10.0, targetDb / 20.0);
  // Auto Fader nunca aumenta a pista: o destino absoluto fica limitado ao volume de origem.
  return std::max(0.0, std::min(originalVolume, requestedTarget));
}

static double nativeMultiLoopBaselineVolume(MediaTrack* track, const std::string& guid, double fallback)
{
  for (const auto& row : g_nativeMultiLoopFadeTracks) {
    if ((row.track && row.track == track) || (!guid.empty() && row.guid == guid)) return row.volume;
  }
  return fallback;
}

static void nativeRestoreMultiLoopTracks()
{
  if (SetMediaTrackInfo_Value_ptr) {
    for (const auto& row : g_nativeMultiLoopRestore) if (row.track) {
      SetMediaTrackInfo_Value_ptr(row.track, "B_MUTE", row.mute);
      SetMediaTrackInfo_Value_ptr(row.track, "I_SOLO", row.solo);
    }
    for (const auto& row : g_nativeMultiLoopFadeTracks) if (row.track) SetMediaTrackInfo_Value_ptr(row.track, "D_VOL", row.volume);
  }
  g_nativeMultiLoopRestore.clear(); g_nativeMultiLoopFadeTracks.clear();
  g_nativeMultiLoopFadeOutActive = false; g_nativeMultiLoopFadeInActive = false;
  if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
  if (UpdateArrange_ptr) UpdateArrange_ptr();
}

static void nativeRestoreMultiLoopBypassVolumes(bool clearAfter)
{
  if (g_nativeMultiLoopBypassVolumeBaseline.empty()) return;
  if (SetMediaTrackInfo_Value_ptr) {
    for (const auto& row : g_nativeMultiLoopBypassVolumeBaseline) {
      if (row.track) SetMediaTrackInfo_Value_ptr(row.track, "D_VOL", row.volume);
    }
  }
  if (clearAfter) g_nativeMultiLoopBypassVolumeBaseline.clear();
  if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
  if (UpdateArrange_ptr) UpdateArrange_ptr();
}

static void nativeCaptureMultiLoopTracks(ReaProject* project, const NativeMultiLoopPair& pair)
{
  const auto stateIt = g_nativeMultiLoopStates.find(pair.songKey);
  if (stateIt == g_nativeMultiLoopStates.end()) return;
  const NativeMultiLoopSongState& state = stateIt->second;
  // Quando o cursor chega ao primeiro marker, o Fade Out anterior ja possui
  // os volumes originais e os destinos corretos. Restaurar aqui criava um
  // salto de um frame ao volume original antes de aplicar o destino novamente.
  const bool preservePreFade = g_nativeMultiLoopFadeOutActive &&
    !g_nativeMultiLoopFadeTracks.empty() &&
    std::fabs(g_nativeMultiLoopFadeEnd - pair.start) <= 0.020;
  if (!preservePreFade) {
    nativeRestoreMultiLoopTracks();
  } else {
    g_nativeMultiLoopFadeOutActive = false;
    g_nativeMultiLoopFadeInActive = false;
  }
  for (const auto& entry : state.tracks[pair.slot]) {
    MediaTrack* track = nativeFindTrackByGuid(project, entry.first);
    if (!track || !GetMediaTrackInfo_Value_ptr || !SetMediaTrackInfo_Value_ptr) continue;
    const NativeMultiLoopTrackPreset& preset = entry.second;
    if (preset.autoFader) {
      auto saved = std::find_if(g_nativeMultiLoopFadeTracks.begin(), g_nativeMultiLoopFadeTracks.end(),
        [&](const NativeMultiLoopTrackRestore& row) { return row.guid == entry.first; });
      if (preservePreFade && saved != g_nativeMultiLoopFadeTracks.end()) {
        saved->track = track;
        saved->targetVolume = nativeMultiLoopFadeTargetVolume(saved->volume, preset);
        SetMediaTrackInfo_Value_ptr(track, "D_VOL", saved->targetVolume);
      } else {
        NativeMultiLoopTrackRestore row; row.track = track; row.guid = entry.first; row.volume = GetMediaTrackInfo_Value_ptr(track, "D_VOL");
        row.targetVolume = nativeMultiLoopFadeTargetVolume(row.volume, preset);
        g_nativeMultiLoopFadeTracks.push_back(row);
        SetMediaTrackInfo_Value_ptr(track, "D_VOL", row.targetVolume);
      }
    }
    if (state.ms[pair.slot] && (preset.mute || preset.solo)) {
      NativeMultiLoopTrackRestore row; row.track = track; row.guid = entry.first;
      row.mute = GetMediaTrackInfo_Value_ptr(track, "B_MUTE"); row.solo = GetMediaTrackInfo_Value_ptr(track, "I_SOLO");
      g_nativeMultiLoopRestore.push_back(row);
      if (preset.mute) SetMediaTrackInfo_Value_ptr(track, "B_MUTE", 1.0);
      if (preset.solo) SetMediaTrackInfo_Value_ptr(track, "I_SOLO", 1.0);
    }
  }
  if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
  if (UpdateArrange_ptr) UpdateArrange_ptr();
}

static void nativeProcessMultiLoops(ReaProject* project, const std::vector<NativeSongWindow>& songs, bool playing, double playPos, const std::string& playingId, double songStart, double songEnd)
{
  nativeLoadMultiLoopState();
  if (!playing || playingId.empty()) {
    if (g_nativeMultiLoopActivePair.valid || !g_nativeMultiLoopRestore.empty() || !g_nativeMultiLoopFadeTracks.empty()) nativeRestoreMultiLoopTracks();
    nativeRestoreMultiLoopBypassVolumes(true);
    g_nativeMultiLoopActivePair = NativeMultiLoopPair(); g_nativeMultiLoopDisarmedPairKey.clear();
    g_nativeMultiLoopBypassActive = false;
    g_nativeMultiLoopBypassSongKey.clear();
    g_nativeMultiLoopBypassRequest.store(-1);
    return;
  }
  const NativeSongWindow* song = nullptr;
  for (const auto& item : songs) if (!item.isBlock && item.id == playingId) { song = &item; break; }
  std::string songKey = song ? nativeMultiLoopSongKey(*song) : std::string();
  if (songKey.empty() && g_nativeMultiLoopFocusStart <= songStart + 0.001 && g_nativeMultiLoopFocusEnd >= songEnd - 0.001) songKey = g_nativeMultiLoopFocusKey;
  const auto stateIt = g_nativeMultiLoopStates.find(songKey);
  if (stateIt == g_nativeMultiLoopStates.end()) {
    if (g_nativeMultiLoopActivePair.valid || !g_nativeMultiLoopRestore.empty() || !g_nativeMultiLoopFadeTracks.empty()) nativeRestoreMultiLoopTracks();
    nativeRestoreMultiLoopBypassVolumes(true);
    g_nativeMultiLoopActivePair = NativeMultiLoopPair();
    g_nativeMultiLoopBypassActive = false;
    g_nativeMultiLoopBypassSongKey.clear();
    g_nativeMultiLoopBypassVolumeBaseline.clear();
    g_nativeMultiLoopBypassRequest.store(-1);
    return;
  }
  const NativeMultiLoopSongState& state = stateIt->second;
  std::vector<NativeMultiLoopPair> enabledPairs;
  std::vector<NativeMultiLoopPair> bypassEligiblePairs;
  for (int slot = 0; slot < 2; ++slot) if (state.loop[slot]) {
    const NativeMultiLoopPair pair = nativeFindMultiLoopPair(project, songKey, songStart, songEnd, slot);
    if (!pair.valid) continue;
    enabledPairs.push_back(pair);
    if (playPos < pair.end - 0.0005) bypassEligiblePairs.push_back(pair);
  }

  const int bypassRequest = g_nativeMultiLoopBypassRequest.exchange(-1);
  if (bypassRequest >= 0) {
    bool enableBypass = bypassRequest == 1;
    if (bypassRequest == 2) enableBypass = !(g_nativeMultiLoopBypassActive && g_nativeMultiLoopBypassSongKey == songKey);
    if (enableBypass && !bypassEligiblePairs.empty()) {
      g_nativeMultiLoopBypassActive = true;
      g_nativeMultiLoopBypassSongKey = songKey;
      g_nativeMultiLoopBypassLastPos = playPos;
      g_nativeMultiLoopBypassMaxEnd = 0.0;
      for (const NativeMultiLoopPair& eligiblePair : bypassEligiblePairs) {
        g_nativeMultiLoopBypassMaxEnd = std::max(g_nativeMultiLoopBypassMaxEnd, eligiblePair.end);
      }
      g_nativeMultiLoopBypassVolumeBaseline.clear();
      // Guarda um volume limpo por pista. Se o Auto Fader ja estiver ativo,
      // o snapshot dele substitui o valor reduzido observado neste frame.
      for (const NativeMultiLoopPair& enabledPair : bypassEligiblePairs) {
        for (const auto& entry : state.tracks[enabledPair.slot]) {
          if (!entry.second.autoFader) continue;
          MediaTrack* track = nativeFindTrackByGuid(project, entry.first);
          if (!track || !GetMediaTrackInfo_Value_ptr) continue;
          bool exists = false;
          for (const auto& saved : g_nativeMultiLoopBypassVolumeBaseline) if (saved.guid == entry.first) { exists = true; break; }
          if (!exists) {
            NativeMultiLoopTrackRestore saved;
            saved.track = track; saved.guid = entry.first; saved.volume = GetMediaTrackInfo_Value_ptr(track, "D_VOL");
            g_nativeMultiLoopBypassVolumeBaseline.push_back(saved);
          }
        }
      }
      for (const auto& fadeSaved : g_nativeMultiLoopFadeTracks) {
        bool replaced = false;
        for (auto& saved : g_nativeMultiLoopBypassVolumeBaseline) if (saved.guid == fadeSaved.guid) {
          saved.track = fadeSaved.track; saved.volume = fadeSaved.volume; replaced = true; break;
        }
        if (!replaced) g_nativeMultiLoopBypassVolumeBaseline.push_back(fadeSaved);
      }
      if (g_nativeMultiLoopActivePair.valid && g_nativeMultiLoopActivePair.songKey == songKey) {
        nativeSetRepeatEnabled(project, false);
        nativeClearLoopTimeRange(project);
        g_nativeMultiLoopActivePair = NativeMultiLoopPair();
      }
      // Cancela o Auto Fader e desfaz Mute/Solo no mesmo frame.
      nativeRestoreMultiLoopTracks();
      nativeRestoreMultiLoopBypassVolumes(false);
    } else if (!enableBypass) {
      // O processamento abaixo recalcula imediatamente o ponto correto do fade
      // ou rearma o par caso o cursor ja esteja dentro dele.
      g_nativeMultiLoopBypassActive = false;
      g_nativeMultiLoopBypassSongKey.clear();
      g_nativeMultiLoopBypassVolumeBaseline.clear();
      // OFF dentro do intervalo: aplica imediatamente o estado que existiria
      // sem o bypass (Auto Fader em zero e preset Mute/Solo ativo).
      for (const NativeMultiLoopPair& pair : bypassEligiblePairs) {
        if (playPos >= pair.start - 0.0005 && playPos < pair.end - 0.0005) {
          nativeCaptureMultiLoopTracks(project, pair);
          nativeSetLoopTimeRange(project, pair.start, pair.end);
          nativeSetRepeatEnabled(project, true);
          g_nativeMultiLoopActivePair = pair;
          break;
        }
      }
    }
    g_nativeForceStateBuild.store(true);
  }

  if (g_nativeMultiLoopBypassActive) {
    const bool sameSong = g_nativeMultiLoopBypassSongKey == songKey;
    const bool movedBack = playPos < (g_nativeMultiLoopBypassLastPos - 0.05);
    const bool passedPairs = g_nativeMultiLoopBypassMaxEnd > 0.0 && playPos > (g_nativeMultiLoopBypassMaxEnd + 0.0005);
    if (!sameSong || movedBack || passedPairs) {
      nativeRestoreMultiLoopBypassVolumes(true);
      g_nativeMultiLoopBypassActive = false;
      g_nativeMultiLoopBypassSongKey.clear();
      g_nativeMultiLoopBypassVolumeBaseline.clear();
      g_nativeForceStateBuild.store(true);
    } else {
      g_nativeMultiLoopBypassLastPos = playPos;
      return;
    }
  }

  NativeMultiLoopPair active;
  for (const NativeMultiLoopPair& pair : enabledPairs) {
    const int slot = pair.slot;
    const bool markerArmed = !g_nativeArmedMarkerId.empty();
    const bool markerTargetsPairStart = markerArmed && std::fabs(g_nativeSelectedMarkerPos - pair.start) <= 0.002;
    if (markerTargetsPairStart) {
      g_nativeMultiLoopDisarmedPairKey = pair.key;
      if (!g_nativeMultiLoopFadeTracks.empty() || !g_nativeMultiLoopRestore.empty()) nativeRestoreMultiLoopTracks();
    }
    const int fadeSec = std::max(1, std::min(5, state.fadeSec[slot]));
    if (!markerArmed && !g_nativeMultiLoopActivePair.valid && playPos >= pair.start - fadeSec && playPos < pair.start && !g_nativeMultiLoopFadeOutActive) {
      for (const auto& entry : state.tracks[slot]) if (entry.second.autoFader) {
        MediaTrack* track = nativeFindTrackByGuid(project, entry.first);
        if (track && GetMediaTrackInfo_Value_ptr) { NativeMultiLoopTrackRestore row; row.track = track; row.guid = entry.first; row.volume = GetMediaTrackInfo_Value_ptr(track, "D_VOL"); row.targetVolume = nativeMultiLoopFadeTargetVolume(row.volume, entry.second); g_nativeMultiLoopFadeTracks.push_back(row); }
      }
      if (!g_nativeMultiLoopFadeTracks.empty()) { g_nativeMultiLoopFadeOutActive = true; g_nativeMultiLoopFadeStart = pair.start - fadeSec; g_nativeMultiLoopFadeEnd = pair.start; }
    }
    if (playPos >= pair.start - 0.0005 && playPos < pair.end - 0.0005) { active = pair; break; }
    if (g_nativeMultiLoopDisarmedPairKey == pair.key && playPos > pair.end + 0.0005) g_nativeMultiLoopDisarmedPairKey.clear();
  }
  if (g_nativeMultiLoopFadeOutActive && SetMediaTrackInfo_Value_ptr) {
    const double span = std::max(0.001, g_nativeMultiLoopFadeEnd - g_nativeMultiLoopFadeStart);
    const double ratio = std::max(0.0, std::min(1.0, 1.0 - ((playPos - g_nativeMultiLoopFadeStart) / span)));
    for (auto& row : g_nativeMultiLoopFadeTracks) if (row.track) SetMediaTrackInfo_Value_ptr(row.track, "D_VOL", row.targetVolume + ((row.volume - row.targetVolume) * ratio));
  }
  if (g_nativeMultiLoopFadeInActive && SetMediaTrackInfo_Value_ptr) {
    const double span = std::max(0.001, g_nativeMultiLoopFadeEnd - g_nativeMultiLoopFadeStart);
    const double ratio = std::max(0.0, std::min(1.0, (playPos - g_nativeMultiLoopFadeStart) / span));
    for (auto& row : g_nativeMultiLoopFadeTracks) if (row.track) SetMediaTrackInfo_Value_ptr(row.track, "D_VOL", row.targetVolume + ((row.volume - row.targetVolume) * ratio));
    if (ratio >= 0.999) nativeRestoreMultiLoopTracks();
  }
  if (!active.valid) {
    if (g_nativeMultiLoopActivePair.valid && playPos > g_nativeMultiLoopActivePair.end + 0.0005) { nativeRestoreMultiLoopTracks(); g_nativeMultiLoopActivePair = NativeMultiLoopPair(); }
    return;
  }
  if (g_nativeMultiLoopDisarmedPairKey == active.key) return;
  const bool repeat = nativeIsRepeatEnabled(project);
  if (g_nativeMultiLoopActivePair.valid && g_nativeMultiLoopActivePair.key == active.key && !repeat) {
    g_nativeMultiLoopDisarmedPairKey = active.key;
    for (const auto& row : g_nativeMultiLoopRestore) if (row.track && SetMediaTrackInfo_Value_ptr) { SetMediaTrackInfo_Value_ptr(row.track, "B_MUTE", row.mute); SetMediaTrackInfo_Value_ptr(row.track, "I_SOLO", row.solo); }
    g_nativeMultiLoopRestore.clear();
    if (!g_nativeMultiLoopFadeTracks.empty()) { g_nativeMultiLoopFadeOutActive = false; g_nativeMultiLoopFadeInActive = true; g_nativeMultiLoopFadeStart = playPos; g_nativeMultiLoopFadeEnd = active.end; }
    g_nativeMultiLoopActivePair = NativeMultiLoopPair();
    return;
  }
  if (!g_nativeMultiLoopActivePair.valid || g_nativeMultiLoopActivePair.key != active.key) {
    nativeCaptureMultiLoopTracks(project, active);
    nativeSetLoopTimeRange(project, active.start, active.end);
    nativeSetRepeatEnabled(project, true);
    g_nativeMultiLoopActivePair = active;
  }
}

static void nativePublishMultiLoopBypassState()
{
  if (!SetExtState_ptr) return;
  static bool initialized = false;
  static bool lastActive = false;
  static std::string lastSongKey;
  if (initialized && lastActive == g_nativeMultiLoopBypassActive && lastSongKey == g_nativeMultiLoopBypassSongKey) return;
  initialized = true;
  lastActive = g_nativeMultiLoopBypassActive;
  lastSongKey = g_nativeMultiLoopBypassSongKey;
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "MULTILOOP_BYPASS_ACTIVE_V1", g_nativeMultiLoopBypassActive ? "1" : "0", false);
  SetExtState_ptr("VS_HOOK_NATIVE_BRIDGE", "MULTILOOP_BYPASS_SONG_KEY_V1", g_nativeMultiLoopBypassActive ? g_nativeMultiLoopBypassSongKey.c_str() : "", false);
}

// O processamento de audio do Multiloops nao pode depender do snapshot HTTP
// de 220 ms. Usa apenas a lista de musicas ja armazenada em cache e roda no
// timer principal do REAPER; markers dos pares tambem usam cache por revisao.
static void nativeProcessMultiLoopsOnMainThread()
{
  if (!EnumProjects_ptr || !GetPlayStateEx_ptr || !GetPlayPositionEx_ptr) return;
  char projectPath[2048] = "";
  ReaProject* project = getCurrentProject(projectPath, static_cast<int>(sizeof(projectPath)));
  std::vector<NativeSongWindow> songs;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    songs = g_nativeSongWindows;
  }

  const int playState = project ? GetPlayStateEx_ptr(project) : 0;
  const bool playing = (playState & 1) == 1 || (playState & 4) == 4;
  const bool paused = (playState & 2) == 2;
  const double playPos = project ? GetPlayPositionEx_ptr(project) : 0.0;
  // Pause congela o Multiloops exatamente no ponto atual. Nao restaura pistas,
  // nao avanca fade e nao desarma o loop; ao continuar, o playPos retoma dali.
  if (paused) {
    const double editPos = (project && GetCursorPositionEx_ptr) ? GetCursorPositionEx_ptr(project) : playPos;
    if (!g_nativeMultiLoopWasPaused) {
      g_nativeMultiLoopWasPaused = true;
      g_nativeMultiLoopPauseSeeked = false;
      g_nativeMultiLoopPausedPlayPos = playPos;
      g_nativeMultiLoopPausedEditPos = editPos;
    } else if (std::fabs(editPos - g_nativeMultiLoopPausedEditPos) > 0.050) {
      g_nativeMultiLoopPauseSeeked = true;
    }
    return;
  }
  if (g_nativeMultiLoopWasPaused) {
    const bool resumedAtAnotherPoint = g_nativeMultiLoopPauseSeeked ||
      (playing && std::fabs(playPos - g_nativeMultiLoopPausedPlayPos) > 0.150);
    g_nativeMultiLoopWasPaused = false;
    g_nativeMultiLoopPauseSeeked = false;
    if (resumedAtAnotherPoint) {
      const bool ownedRuntime = g_nativeMultiLoopActivePair.valid ||
        !g_nativeMultiLoopRestore.empty() || !g_nativeMultiLoopFadeTracks.empty();
      if (ownedRuntime) {
        nativeSetRepeatEnabled(project, false);
        nativeClearLoopTimeRange(project);
      }
      nativeRestoreMultiLoopTracks();
      nativeRestoreMultiLoopBypassVolumes(true);
      g_nativeMultiLoopActivePair = NativeMultiLoopPair();
      g_nativeMultiLoopDisarmedPairKey.clear();
      g_nativeMultiLoopBypassActive = false;
      g_nativeMultiLoopBypassSongKey.clear();
      g_nativeMultiLoopBypassRequest.store(-1);
    }
  }
  std::string playingName;
  double songStart = 0.0;
  double songEnd = 0.0;
  std::string playingId = playing ? nativeFindPlayingId(songs, playPos, playingName, songStart, songEnd) : std::string();

  nativeProcessMultiLoops(project, songs, playing, playPos, playingId, songStart, songEnd);
  nativePublishMultiLoopBypassState();
}

static std::string nativeBuildMultiLoopsJson(ReaProject* project)
{
  nativeLoadMultiLoopState();
  bool bypassActiveForState = g_nativeMultiLoopBypassActive;
  std::string bypassSongKeyForState = g_nativeMultiLoopBypassSongKey;
  static std::string cachedSignature;
  static std::string cachedJson;
  const int changeCount = (project && GetProjectStateChangeCount_ptr) ? GetProjectStateChangeCount_ptr(project) : -1;
  std::ostringstream signature;
  signature << project << "|" << changeCount << "|" << g_nativeMultiLoopFocusId << "|" << g_nativeMultiLoopFocusKey << "|"
            << std::fixed << std::setprecision(6) << g_nativeMultiLoopFocusStart << "|" << g_nativeMultiLoopFocusEnd << "|" << g_nativeMultiLoopRaw
            << "|" << (bypassActiveForState ? 1 : 0) << "|" << bypassSongKeyForState;
  if (signature.str() == cachedSignature && !cachedJson.empty()) return cachedJson;
  if (g_nativeMultiLoopFocusKey.empty()) {
    cachedSignature = signature.str();
    std::ostringstream emptyOut;
    emptyOut << "{\"songId\":\"\",\"songKey\":\"\",\"songName\":\"\",\"bypassActive\":" << (bypassActiveForState ? "true" : "false")
             << ",\"bypassSongKey\":" << nativeJsonString(bypassSongKeyForState)
             << ",\"loop1Enabled\":false,\"loop2Enabled\":false,\"ms1Enabled\":false,\"ms2Enabled\":false,\"loop1Available\":false,\"loop2Available\":false,\"fade1Sec\":3,\"fade2Sec\":3,\"tracks\":[]}";
    cachedJson = emptyOut.str();
    return cachedJson;
  }
  NativeMultiLoopSongState empty;
  const auto it = g_nativeMultiLoopStates.find(g_nativeMultiLoopFocusKey);
  const NativeMultiLoopSongState& state = it == g_nativeMultiLoopStates.end() ? empty : it->second;
  const NativeMultiLoopPair pair1 = nativeFindMultiLoopPair(project, g_nativeMultiLoopFocusKey, g_nativeMultiLoopFocusStart, g_nativeMultiLoopFocusEnd, 0);
  const NativeMultiLoopPair pair2 = nativeFindMultiLoopPair(project, g_nativeMultiLoopFocusKey, g_nativeMultiLoopFocusStart, g_nativeMultiLoopFocusEnd, 1);
  std::ostringstream tracks;
  tracks << "[";
  const int count = (project && CountTracks_ptr) ? CountTracks_ptr(project) : 0;
  for (int i = 0; i < count; ++i) {
    if (i) tracks << ",";
    MediaTrack* track = GetTrack_ptr ? GetTrack_ptr(project, i) : nullptr;
    const std::string guid = nativeTrackGuid(track, i);
    const double currentTrackVolume = (track && GetMediaTrackInfo_Value_ptr) ? GetMediaTrackInfo_Value_ptr(track, "D_VOL") : 1.0;
    const double trackVolume = nativeMultiLoopBaselineVolume(track, guid, currentTrackVolume);
    tracks << "{\"id\":" << nativeJsonString(guid) << ",\"guid\":" << nativeJsonString(guid) << ",\"name\":" << nativeJsonString(nativeTrackName(track, i))
           << ",\"volumeDb\":" << nativeNumber(nativeVolumeToDb(trackVolume), 3);
    for (int slot = 0; slot < 2; ++slot) {
      const auto rowIt = state.tracks[slot].find(guid);
      const NativeMultiLoopTrackPreset row = rowIt == state.tracks[slot].end() ? NativeMultiLoopTrackPreset() : rowIt->second;
      tracks << ",\"auto" << (slot + 1) << "\":" << (row.autoFader ? "true" : "false")
             << ",\"mute" << (slot + 1) << "\":" << (row.mute ? "true" : "false")
             << ",\"solo" << (slot + 1) << "\":" << (row.solo ? "true" : "false")
             << ",\"autoLimit" << (slot + 1) << "Set\":" << (row.fadeLimitSet ? "true" : "false")
             << ",\"autoLimit" << (slot + 1) << "Db\":" << (row.fadeLimitSet ? nativeNumber(row.fadeLimitDb, 3) : "null");
    }
    tracks << "}";
  }
  tracks << "]";
  std::ostringstream out;
  out << "{\"songId\":" << nativeJsonString(g_nativeMultiLoopFocusId) << ",\"songKey\":" << nativeJsonString(g_nativeMultiLoopFocusKey)
      << ",\"songName\":" << nativeJsonString(g_nativeMultiLoopFocusName) << ",\"startPos\":" << nativeNumber(g_nativeMultiLoopFocusStart)
      << ",\"endPos\":" << nativeNumber(g_nativeMultiLoopFocusEnd)
      << ",\"bypassActive\":" << (bypassActiveForState ? "true" : "false") << ",\"bypassSongKey\":" << nativeJsonString(bypassSongKeyForState)
      << ",\"loop1Enabled\":" << (state.loop[0] ? "true" : "false") << ",\"loop2Enabled\":" << (state.loop[1] ? "true" : "false")
      << ",\"ms1Enabled\":" << (state.ms[0] && state.loop[0] ? "true" : "false") << ",\"ms2Enabled\":" << (state.ms[1] && state.loop[1] ? "true" : "false")
      << ",\"loop1Available\":" << (pair1.valid ? "true" : "false") << ",\"loop2Available\":" << (pair2.valid ? "true" : "false")
      << ",\"fade1Sec\":" << state.fadeSec[0] << ",\"fade2Sec\":" << state.fadeSec[1] << ",\"tracks\":" << tracks.str() << "}";
  cachedSignature = signature.str();
  cachedJson = out.str();
  return cachedJson;
}



// FIX74: se a fila nativa chegou no alvo e algum estado antigo parou o transporte
// logo depois, religamos o Play apenas dentro da janela de proteção e somente dentro
// da região alvo. Isso evita a música começar e parar após o seek.
static void nativeProtectQueueHandoffPlayback(ReaProject* project, int& playState, double& playPos)
{
  if (!project || !Main_OnCommand_ptr || nativeIsLuaControlActive()) return;
  std::string handoffId;
  double handoffStart = 0.0;
  double handoffEnd = 0.0;
  bool shouldProtect = false;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (!g_nativeQueueHandoffPlayId.empty() && g_nativeQueueHandoffProtectUntil.time_since_epoch().count() != 0) {
      const auto now = std::chrono::steady_clock::now();
      if (now <= g_nativeQueueHandoffProtectUntil) {
        handoffId = g_nativeQueueHandoffPlayId;
        handoffStart = g_nativeQueueHandoffStart;
        handoffEnd = g_nativeQueueHandoffEnd;
        shouldProtect = true;
      } else {
        nativeCancelQueueHandoffProtectionLocked();
      }
    }
  }
  if (!shouldProtect || handoffEnd <= handoffStart + 0.0005) return;
  if (!GetPlayPositionEx_ptr) return;
  playPos = GetPlayPositionEx_ptr(project);
  const bool insideTarget = playPos >= handoffStart - 0.050 && playPos < handoffEnd - 0.050;
  if (!insideTarget) return;
  const bool playing = ((playState & 1) == 1) || ((playState & 4) == 4);
  if (!playing) {
    Main_OnCommand_ptr(1007, 0); // Transport: Play
    if (GetPlayStateEx_ptr) playState = GetPlayStateEx_ptr(project);
    if (GetPlayPositionEx_ptr) playPos = GetPlayPositionEx_ptr(project);
    g_nativeForceStateBuild.store(true);
  }
}

static void nativeRebuildState(bool forceSnapshot)
{
  if (!EnumProjects_ptr) return;
  nativeRefreshLuaControlHeartbeatFromExtState();
  char activePathBuf[2048] = "";
  ReaProject* activeProject = getCurrentProject(activePathBuf, static_cast<int>(sizeof(activePathBuf)));
  std::string activeProjectName;
  std::string activeProjectPath;
  int activeProjectIndex = 0;
  const std::string projectsJson = nativeBuildProjectsJson(activeProject, activeProjectName, activeProjectPath, activeProjectIndex);
  if (activeProjectName.empty()) activeProjectName = nativeBasenameNoRpp(activePathBuf);
  if (activeProjectPath.empty()) activeProjectPath = normalizeSlashes(activePathBuf);

  // O estado vivo continua em 220 ms, mas a estrutura pesada so e refeita
  // quando o projeto realmente muda ou um comando pede atualizacao. Isso evita
  // varrer/serializar regioes, repertorios, mixer e Premix na thread principal
  // a cada heartbeat, deixando o desenho e o zoom do Grid livres.
  static ReaProject* cachedSnapshotProject = nullptr;
  static int cachedSnapshotChangeCount = -1;
  static int observedProjectChangeCount = -1;
  static std::chrono::steady_clock::time_point observedProjectChangeAt;
  static std::vector<NativeSongWindow> cachedSongs;
  static std::string cachedMarkersJson = "[]";
  static std::string cachedRegionsJson = "[]";
  static double cachedRegionsTotalSec = 0.0;
  static int cachedActivePlaylistIndex = 1;
  static std::string cachedActivePlaylistName = "Músicas";
  static std::vector<NativeSongWindow> cachedActivePlaylistItems;
  static double cachedActivePlaylistTotalSec = 0.0;
  static std::string cachedPlaylistsJson = "[]";
  static std::string cachedMixerJson = "{\"tracks\":[],\"groups\":[],\"master\":{}}";
  static std::string cachedMixerTracksJson = "[]";
  static std::string cachedMixerGroupsJson = "[]";
  static std::string cachedMixerMasterJson = "{}";
  static std::string cachedPremixJson = "{}";
  static std::chrono::steady_clock::time_point cachedMixerRefreshAt;
  const int projectChangeCount = GetProjectStateChangeCount_ptr
    ? GetProjectStateChangeCount_ptr(activeProject)
    : -1;
  const auto snapshotNow = std::chrono::steady_clock::now();
  const bool snapshotProjectChanged = cachedSnapshotProject != activeProject;
  if (snapshotProjectChanged || observedProjectChangeCount != projectChangeCount) {
    observedProjectChangeCount = projectChangeCount;
    observedProjectChangeAt = snapshotNow;
  }
  // Alteracoes de volume durante Faderout/Multiloops incrementam o contador do
  // projeto em sequencia. Esperar a revisao estabilizar evita reconstruir
  // regioes, repertorios e Premix a cada passo do fade.
  const bool projectRevisionSettled = cachedSnapshotChangeCount != projectChangeCount &&
    observedProjectChangeAt.time_since_epoch().count() != 0 &&
    std::chrono::duration_cast<std::chrono::milliseconds>(snapshotNow - observedProjectChangeAt).count() >= 300;
  const bool rebuildSnapshot = forceSnapshot || snapshotProjectChanged || projectRevisionSettled || cachedSongs.empty();
  if (rebuildSnapshot) {
    cachedSnapshotProject = activeProject;
    cachedSnapshotChangeCount = projectChangeCount;
    cachedSongs = nativeCollectProjectSongs(activeProject, cachedMarkersJson);
    nativeProcessTunerCommands(activeProject, cachedSongs);
    const auto tunerOffsets = nativeLoadTunerOffsets(activeProject);
    for (auto& song : cachedSongs) {
      if (song.isBlock || song.isHashParent) {
        song.tunerValue = 0;
        continue;
      }
      const auto it = tunerOffsets.find(nativeTunerKey(song));
      song.tunerValue = it == tunerOffsets.end() ? 0 : it->second;
    }
  }
  std::string markersJson = cachedMarkersJson;
  std::vector<NativeSongWindow> songs = cachedSongs;

  int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(activeProject) : 0;
  double playPos = GetPlayPositionEx_ptr ? GetPlayPositionEx_ptr(activeProject) : 0.0;
  const double editCursorPos = GetCursorPositionEx_ptr ? GetCursorPositionEx_ptr(activeProject) : playPos;
  nativeProtectQueueHandoffPlayback(activeProject, playState, playPos);
  const bool playing = (playState & 1) == 1 || (playState & 4) == 4;
  const bool paused = (playState & 2) == 2;
  std::string playingName;
  double songStart = 0.0;
  double songEnd = 0.0;
  std::string playingId = playing ? nativeFindPlayingId(songs, playPos, playingName, songStart, songEnd) : "";

  // Musicas-filho agora sao regioes reais da ruler lane 2 e ja fazem parte de
  // `songs`. Nao existe mais motivo para revarrer markers durante a reproducao.
  nativeUpdateLiveMarkTracker(playing, paused, playingId);
  nativeApplyLiveExecutedFlags(songs);
  const double duration = std::max(0.0, songEnd - songStart);
  const double elapsed = playingId.empty() ? 0.0 : std::max(0.0, std::min(duration, playPos - songStart));
  const double remaining = playingId.empty() ? 0.0 : std::max(0.0, duration - elapsed);
  const double progress = duration > 0.0 ? std::min(1.0, std::max(0.0, elapsed / duration)) : 0.0;

  bool wasPlayingBeforeNativeState = false;
  bool suppressStoppedTransitionPrepare = false;
  bool hasStoppedTransitionQueueTarget = false;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    wasPlayingBeforeNativeState = g_nativeCurrentTransportPlaying;
    suppressStoppedTransitionPrepare =
      g_nativeSuppressStoppedTransitionPrepareUntil.time_since_epoch().count() != 0 &&
      std::chrono::steady_clock::now() <= g_nativeSuppressStoppedTransitionPrepareUntil;
    hasStoppedTransitionQueueTarget =
      (!g_nativeQueuedSongId.empty() && g_nativeQueuedEnd > g_nativeQueuedStart + 0.0005) ||
      (!g_nativeAutoBlocoTargetSongId.empty() && g_nativeAutoBlocoTargetEnd > g_nativeAutoBlocoTargetStart + 0.0005);
  }
  if (!nativeIsLuaControlActive() && !playing && wasPlayingBeforeNativeState && !suppressStoppedTransitionPrepare && hasStoppedTransitionQueueTarget) {
    nativePrepareStopSelectionFromQueueOrCurrent(activeProject, true);
  }

  const std::string previousPlayingId = g_nativeCurrentPlayingId;
  g_nativeCurrentTransportPlaying = playing;
  g_nativeCurrentPlayingId = playingId;
  g_nativeCurrentSongStart = songStart;
  g_nativeCurrentSongEnd = songEnd;
  g_nativeCurrentPlayPosition = playPos;

  if (playing && !playingId.empty() && playingId != previousPlayingId) {
    const NativeSongWindow* premixSong = nullptr;
    for (const auto& song : songs) {
      if (!song.isBlock && !song.isHashParent && song.id == playingId) { premixSong = &song; break; }
    }
    nativeApplyPremixOnSongStart(activeProject, premixSong);
    if (premixSong) nativeApplyTunerValue(activeProject, premixSong->tunerValue);
    g_nativeForceStateBuild.store(true);
  }

  if (rebuildSnapshot) {
    cachedSongs = songs;
    cachedRegionsJson = nativeBuildRegionsJson(songs);
    cachedRegionsTotalSec = nativeComputeRegionsTotalSec(songs);
    cachedPlaylistsJson = nativeBuildPlaylistsJson(activeProject, songs, cachedActivePlaylistIndex, cachedActivePlaylistName, &cachedActivePlaylistItems, &cachedActivePlaylistTotalSec);
    cachedPremixJson = nativeBuildPremixJson(activeProject, songs);
  }
  // Mute, solo e volume podem ser alterados diretamente no REAPER sem mudar
  // de forma confiavel a revisao usada pelo snapshot pesado. Atualiza apenas o
  // mixer em uma cadencia leve; regioes, repertorios e Premix continuam no
  // cache. A composicao reaproveita cada leitura e evita a varredura duplicada
  // que nativeBuildMixerJson faria.
  const bool refreshMixer = rebuildSnapshot || cachedMixerRefreshAt.time_since_epoch().count() == 0 ||
    std::chrono::duration_cast<std::chrono::milliseconds>(snapshotNow - cachedMixerRefreshAt).count() >= 200;
  if (refreshMixer) {
    nativeBuildMixerTrackListsJson(activeProject, cachedMixerTracksJson, cachedMixerGroupsJson);
    cachedMixerMasterJson = nativeBuildMixerMasterJson(activeProject);
    cachedMixerJson = std::string("{\"tracks\":") + cachedMixerTracksJson +
      ",\"groups\":" + cachedMixerGroupsJson + ",\"master\":" + cachedMixerMasterJson + "}";
    cachedMixerRefreshAt = snapshotNow;
  }
  const std::string& regionsJson = cachedRegionsJson;
  const double regionsTotalSec = cachedRegionsTotalSec;
  const int activePlaylistIndex = cachedActivePlaylistIndex;
  const std::string& activePlaylistName = cachedActivePlaylistName;
  const std::vector<NativeSongWindow>& activePlaylistItems = cachedActivePlaylistItems;
  const double activePlaylistTotalSec = cachedActivePlaylistTotalSec;
  const std::string& playlistsJson = cachedPlaylistsJson;
  nativeSyncControlStateFromLuaExtState();
  nativeSyncQueueFromLuaExtState(activePlaylistItems, songs);
  nativeMaintainQueueAutomation(activeProject, playing, playingId, playPos, songStart, songEnd, activePlaylistItems, songs);
  nativeMaintainAutoStop(activeProject, playing, playingId, playPos, songStart, songEnd, activePlaylistItems, songs);
  const std::string& mixerJson = cachedMixerJson;
  const std::string& premixJson = cachedPremixJson;
  const std::string multiLoopsJson = nativeBuildMultiLoopsJson(activeProject);
  const std::string manualStopFadeoutJson = nativeBuildManualStopFadeoutJson();
  nativeRefreshStopPauseModeFromExtState();
  const bool stopPauseModeEnabled = g_stopPauseModeEnabled.load();
  const std::string tp1Json = nativeBuildTelepromptStateJson(activeProject, songs, 1, "TELEPROMPT1", playing, playPos, playingName);
  const std::string tp2Json = nativeBuildTelepromptStateJson(activeProject, songs, 2, "TELEPROMPT2", playing, playPos, playingName);
  const std::string tp1LyricsText = nativeJsonExtractString(tp1Json, "lyricsText");
  const std::string tp1SongName = nativeJsonExtractString(tp1Json, "songName");
  const std::string tp1MediaType = nativeJsonExtractString(tp1Json, "mediaType");
  const std::string tp1MediaPath = nativeJsonExtractString(tp1Json, "mediaPath");
  const std::string tp1MediaExt = nativeJsonExtractString(tp1Json, "mediaExt");
  const std::string tp1MediaCurrentTime = nativeJsonExtractString(tp1Json, "mediaCurrentTime");
  const std::string tp1MediaOffset = nativeJsonExtractString(tp1Json, "mediaOffset");
  const std::string tp1MediaPlayrate = nativeJsonExtractString(tp1Json, "mediaPlayrate");
  const std::string tp1UpdatedAt = nativeJsonExtractString(tp1Json, "updatedAt");
  const std::string tp2LyricsText = nativeJsonExtractString(tp2Json, "lyricsText");
  const std::string tp2SongName = nativeJsonExtractString(tp2Json, "songName");
  const std::string tp2MediaType = nativeJsonExtractString(tp2Json, "mediaType");
  const std::string tp2MediaPath = nativeJsonExtractString(tp2Json, "mediaPath");
  const std::string tp2MediaExt = nativeJsonExtractString(tp2Json, "mediaExt");
  const std::string tp2MediaCurrentTime = nativeJsonExtractString(tp2Json, "mediaCurrentTime");
  const std::string tp2MediaOffset = nativeJsonExtractString(tp2Json, "mediaOffset");
  const std::string tp2MediaPlayrate = nativeJsonExtractString(tp2Json, "mediaPlayrate");
  const std::string tp2UpdatedAt = nativeJsonExtractString(tp2Json, "updatedAt");
  const std::string nowIso = nativeIsoNow();
  const bool appActive = nativeIsDirectorControlActive();
  const bool luaControlActive = nativeIsLuaControlActive();
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (appActive) {
      g_nativeControlOwner = "director";
      g_nativeDirectorWasActive = true;
      nativePublishSharedControlStateLocked();
    } else if (g_nativeDirectorWasActive) {
      g_nativeDirectorWasActive = false;
      nativeBumpSharedRevisionLocked("lua");
    } else {
      g_nativeControlOwner = "lua";
      nativePublishSharedControlStateLocked();
    }
  }
  // FIX83: publica também em ExtState para o Lua mostrar a tela grande mesmo
  // se a chamada direta VS_Hook_Native_GetState ainda não estiver disponível.
  if (SetExtState_ptr) {
    SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_V1", appActive ? "1" : "0", false);
    SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_AT_SEC_V1", "0", false);
  }

  std::string luaLiveRaw;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    // FIX67: marker engatilhado precisa aparecer verde/piscando no front,
    // mas deve limpar quando o seek realmente chegou no alvo. Mantem um hold curto
    // para o Diretor receber ao menos um snapshot com markerGoId ativo.
    if (!g_nativeArmedMarkerId.empty() && g_nativeSelectedMarkerPos > 0.0) {
      // FIX68: nao usa o cursor de edicao para limpar o pisca verde.
      // O comando marker_go move o cursor de edicao imediatamente, mas o visual
      // do Diretor deve continuar piscando ate o cursor de reproducao chegar no alvo.
      const bool hasGraceElapsed = g_nativeArmedMarkerSetAt.time_since_epoch().count() != 0 &&
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_nativeArmedMarkerSetAt).count() >= 150;
      const double target = g_nativeSelectedMarkerPos;
      const double origin = g_nativeArmedMarkerStartPlayPos;
      const bool nearTarget = playing && std::fabs(playPos - target) <= 0.120;
      const bool forwardCrossed = playing && origin <= target && playPos >= target - 0.080;
      const bool backwardCrossed = playing && origin > target && playPos <= target + 0.080;
      const bool playReached = nearTarget || forwardCrossed || backwardCrossed;
      if (playReached && hasGraceElapsed) {
        // FIX73: ao chegar no alvo, limpa tambem a selecao amarela do marker.
        g_nativeArmedMarkerId.clear();
        g_nativeSelectedMarkerId.clear();
        g_nativeSelectedMarkerPos = 0.0;
        g_nativeArmedMarkerSetAt = std::chrono::steady_clock::time_point();
        g_nativeArmedMarkerStartPlayPos = 0.0;
      }
    }
    luaLiveRaw = g_nativeLuaLiveFragment;
  }
  if (luaLiveRaw.empty() && GetExtState_ptr) {
    const char* extLuaLive = GetExtState_ptr(kNativeExtStateSection, kNativeLuaLiveExtKey);
    if (extLuaLive && *extLuaLive) luaLiveRaw = extLuaLive;
  }

  std::string queuedSongId;
  std::string queuedPlaylistSongId;
  int queuedRegionNumber = 0;
  int queuedPlaylistIndex = 0;
  bool queuedManual = false;
  double queuedStart = 0.0;
  double queuedEnd = 0.0;
  std::string autoBlocoTargetSongId;
  std::string autoBlocoTargetPlaylistSongId;
  int autoBlocoTargetRegionNumber = 0;
  double autoBlocoTargetStart = 0.0;
  double autoBlocoTargetEnd = 0.0;
  int autoBlocoTargetPlaylistIndex = 0;
  bool autoplayEnabled = false;
  bool autoBlocoEnabled = false;
  bool autoStopEnabled = true;
  bool timerRunning = false;
  std::string timerMode = "progressive";
  double timerStartedAtMs = 0.0;
  double timerAccumulatedSec = 0.0;
  double timerTargetSec = 0.0;
  double timerElapsedSec = 0.0;
  double timerDisplaySec = 0.0;
  bool timerExpired = false;
  double timerOverrunSec = 0.0;
  std::string timerDisplayText;
  std::string timerLocalTimeText;
  std::string timerJson;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    queuedSongId = g_nativeQueuedSongId;
    queuedPlaylistSongId = g_nativeQueuedPlaylistSongId;
    queuedRegionNumber = g_nativeQueuedRegionNumber;
    queuedPlaylistIndex = g_nativeQueuedPlaylistIndex;
    queuedManual = g_nativeQueuedManual;
    queuedStart = g_nativeQueuedStart;
    queuedEnd = g_nativeQueuedEnd;
    autoBlocoTargetSongId = g_nativeAutoBlocoTargetSongId;
    autoBlocoTargetPlaylistSongId = g_nativeAutoBlocoTargetPlaylistSongId;
    autoBlocoTargetRegionNumber = g_nativeAutoBlocoTargetRegionNumber;
    autoBlocoTargetStart = g_nativeAutoBlocoTargetStart;
    autoBlocoTargetEnd = g_nativeAutoBlocoTargetEnd;
    autoBlocoTargetPlaylistIndex = g_nativeAutoBlocoTargetPlaylistIndex;
    nativeLoadAutomationSettingsOnceLocked();
    autoplayEnabled = g_nativeAutoplayEnabled;
    autoBlocoEnabled = g_nativeAutoBlocoEnabled;
    autoStopEnabled = g_nativeAutoStopEnabled;
    timerRunning = g_nativeTimerRunning;
    timerMode = g_nativeTimerMode;
    timerStartedAtMs = timerRunning ? nativeTimerEpochMs(g_nativeTimerStartedAtSystem) : 0.0;
    timerAccumulatedSec = nativeTimerAccumulatedSecLocked();
    timerTargetSec = g_nativeTimerTargetSec;
    timerElapsedSec = nativeTimerElapsedValueSecLocked();
    timerDisplaySec = nativeTimerDisplaySecLocked();
    timerExpired = nativeTimerCountdownExpiredLocked();
    timerOverrunSec = timerExpired ? std::max(0.0, -timerDisplaySec) : 0.0;
    timerLocalTimeText = nativeLocalTimeText();
    timerDisplayText = timerMode == "local_time" ? timerLocalTimeText : nativeFormatTimerTextFromSeconds(timerDisplaySec, timerExpired);
    timerJson = nativeBuildTimerStateJsonLocked();
    nativePublishTimerStateLocked();
  }

  if (queuedPlaylistSongId.empty() && !queuedSongId.empty()) queuedPlaylistSongId = queuedSongId;
  const bool loopActive = nativeIsRepeatEnabled(activeProject);
  double loopStartPos = 0.0;
  double loopEndPos = 0.0;
  const bool loopRangeActive = nativeGetLoopTimeRange(activeProject, loopStartPos, loopEndPos);
  const std::string loopStartMarkerName = loopRangeActive ? nativeFindLoopBoundaryLabel(activeProject, loopStartPos) : std::string();
  const std::string loopEndMarkerName = loopRangeActive ? nativeFindLoopBoundaryLabel(activeProject, loopEndPos) : std::string();
  // Se o loop comeca no inicio da regiao (trecho anterior ao primeiro marker),
  // publica o nome da musica para o painel de transporte do Diretor.
  const std::string loopRegionName = (loopRangeActive && loopStartMarkerName.empty())
    ? nativeFindLoopRegionLabel(activeProject, loopStartPos, loopEndPos)
    : std::string();
  const std::string loopSourceType = !loopStartMarkerName.empty() ? "marker" : (!loopRegionName.empty() ? "region" : "segment");

  std::string directorLogoutToken;
  std::string directorLogoutAt;
  std::string directorLogoutCommand;
  std::string directorLogoutTarget;
  std::string directorAuthRevision;
  if (GetExtState_ptr) {
    const char* authRevision = GetExtState_ptr(kNativeExtStateSection, kDirectorAuthRevisionKey);
    if (authRevision && *authRevision) directorAuthRevision = authRevision;
    const char* logoutReq = GetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_REQUEST_V1");
    if (logoutReq && *logoutReq) directorLogoutToken = logoutReq;
    const char* logoutAt = GetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_REQUEST_AT_V1");
    if (logoutAt && *logoutAt) directorLogoutAt = logoutAt;
    const char* logoutCmd = GetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_COMMAND_V1");
    if (logoutCmd && *logoutCmd) directorLogoutCommand = logoutCmd;
    const char* logoutTarget = GetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_TARGET_V1");
    if (logoutTarget && *logoutTarget) directorLogoutTarget = logoutTarget;
  }
  if (directorLogoutCommand.empty() && !directorLogoutToken.empty()) directorLogoutCommand = "director_force_logout";
  if (directorLogoutTarget.empty() && !directorLogoutToken.empty()) directorLogoutTarget = "director";
  const bool directorLogoutRequested = nativeDirectorLogoutRequestIsFresh(directorLogoutToken, directorLogoutAt) &&
    (directorLogoutCommand.empty() || directorLogoutCommand == "director_force_logout");
  if (!directorLogoutRequested && !directorLogoutToken.empty() && SetExtState_ptr) {
    // Limpa token vencido para ele nao reaparecer quando o REAPER for reaberto.
    SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_REQUEST_V1", "", false);
    SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_REQUEST_AT_V1", "", false);
    SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_COMMAND_V1", "", false);
    SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_TARGET_V1", "", false);
    directorLogoutCommand.clear();
    directorLogoutTarget.clear();
  }

  // A senha do Diretor e configurada pelo Lua em JBKEYS_VSHOOK_ACCESS.
  // Publicamos apenas o hash atual para invalidar automaticamente uma sessao
  // antiga do celular quando o usuario trocar a senha no Lua.
  const std::string directorPassword = nativeReadDirectorPassword();
  const std::string directorAuthHash = nativeDirectorPasswordHash(directorPassword);
  const bool directorAuthEnabled = !directorAuthHash.empty();

  std::ostringstream json;
  json << "{";
  json << "\"ok\":true,";
  json << "\"nativeBridge\":true,";
  json << "\"bridgeVersion\":2,";
  json << "\"connected\":true,";
  json << "\"directorAuthEnabled\":" << (directorAuthEnabled ? "true" : "false") << ",";
  json << "\"authEnabled\":" << (directorAuthEnabled ? "true" : "false") << ",";
  json << "\"accessAuthEnabled\":" << (directorAuthEnabled ? "true" : "false") << ",";
  json << "\"directorAuthHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"authHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"accessAuthHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"directorPasswordHash\":" << nativeJsonString(directorAuthHash) << ",";
  json << "\"directorAuthRevision\":" << nativeJsonString(directorAuthRevision) << ",";
  json << "\"authRevision\":" << nativeJsonString(directorAuthRevision) << ",";
  json << "\"accessAuthRevision\":" << nativeJsonString(directorAuthRevision) << ",";
  // FIX81: expõe para o Lua quando o app do Diretor está ativo.
  // O Lua usa este sinal para mostrar a janela grande de bloqueio local.
  json << "\"appActive\":" << (appActive ? "true" : "false") << ",";
  json << "\"directorAppActive\":" << (appActive ? "true" : "false") << ",";
  json << "\"directorActive\":" << (appActive ? "true" : "false") << ",";
  json << "\"luaControlActive\":" << (luaControlActive ? "true" : "false") << ",";
  json << "\"nativeQueueExecutionBypassed\":" << (luaControlActive ? "true" : "false") << ",";
  json << "\"forceDirectorLogout\":" << (directorLogoutRequested ? "true" : "false") << ",";
  json << "\"directorLogoutRequested\":" << (directorLogoutRequested ? "true" : "false") << ",";
  json << "\"logoutDirector\":" << (directorLogoutRequested ? "true" : "false") << ",";
  json << "\"directorCommand\":" << nativeJsonString(directorLogoutRequested ? directorLogoutCommand : "") << ",";
  json << "\"appCommand\":" << nativeJsonString(directorLogoutRequested ? directorLogoutCommand : "") << ",";
  json << "\"directorLogoutCommand\":" << nativeJsonString(directorLogoutRequested ? directorLogoutCommand : "") << ",";
  json << "\"appLogoutTarget\":" << nativeJsonString(directorLogoutRequested ? directorLogoutTarget : "") << ",";
  json << "\"logoutTarget\":" << nativeJsonString(directorLogoutRequested ? directorLogoutTarget : "") << ",";
  json << "\"directorLogoutToken\":" << nativeJsonString(directorLogoutToken) << ",";
  json << "\"updatedAt\":" << nativeJsonString(nowIso) << ",";
  json << "\"heartbeatAt\":" << nativeJsonString(nowIso) << ",";
  json << "\"lastHeartbeatAt\":" << nativeJsonString(nowIso) << ",";
  json << "\"stateUpdatedAt\":" << nativeJsonString(nowIso) << ",";
  json << "\"projectName\":" << nativeJsonString(activeProjectName) << ",";
  json << "\"currentProjectName\":" << nativeJsonString(activeProjectName) << ",";
  json << "\"projectPath\":" << nativeJsonString(activeProjectPath) << ",";
  json << "\"projects\":" << projectsJson << ",";
  json << "\"projectTabs\":" << projectsJson << ",";
  json << "\"openProjects\":" << projectsJson << ",";
  json << "\"activeProjectTabIndex\":" << activeProjectIndex << ",";
  json << "\"regions\":" << regionsJson << ",";
  json << "\"markers\":" << markersJson << ",";
  json << "\"playlists\":" << playlistsJson << ",";
  json << "\"mixer\":" << mixerJson << ",";
  json << "\"mixerTracks\":" << cachedMixerTracksJson << ",";
  json << "\"mixerGroups\":" << cachedMixerGroupsJson << ",";
  json << "\"mixerMaster\":" << cachedMixerMasterJson << ",";
  json << "\"manualStopFadeout\":" << manualStopFadeoutJson << ",";
  json << "\"manualStopFadeoutEnabled\":" << (nativeBoolFromText(nativeReadLuaWindowExtState(kManualStopFadeoutEnabledKey), false) ? "true" : "false") << ",";
  json << "\"manualStopFadeoutDuration\":" << nativeManualStopFadeoutClampDuration(nativeReadLuaWindowExtState(kManualStopFadeoutDurationKey)) << ",";
  json << "\"stopPauseModeEnabled\":" << (stopPauseModeEnabled ? "true" : "false") << ",";
  json << "\"editModeStopPauseEnabled\":" << (stopPauseModeEnabled ? "true" : "false") << ",";
  json << "\"currentPlaylistName\":" << nativeJsonString(activePlaylistName) << ",";
  json << "\"activePlaylistName\":" << nativeJsonString(activePlaylistName) << ",";
  json << "\"activePlaylistId\":" << nativeJsonString(std::to_string(activePlaylistIndex)) << ",";
  json << "\"currentPlaylistIndex\":" << activePlaylistIndex << ",";
  json << "\"activePlaylistTotalSec\":" << nativeNumber(activePlaylistTotalSec) << ",";
  json << "\"currentPlaylistTotalSec\":" << nativeNumber(activePlaylistTotalSec) << ",";
  json << "\"playlistTotalSec\":" << nativeNumber(activePlaylistTotalSec) << ",";
  json << "\"totalPlaylistSec\":" << nativeNumber(activePlaylistTotalSec) << ",";
  json << "\"activePlaylistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(activePlaylistTotalSec)) << ",";
  json << "\"currentPlaylistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(activePlaylistTotalSec)) << ",";
  json << "\"playlistTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(activePlaylistTotalSec)) << ",";
  json << "\"totalPlaylistText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(activePlaylistTotalSec)) << ",";
  json << "\"regionsTotalSec\":" << nativeNumber(regionsTotalSec) << ",";
  json << "\"totalRegionsSec\":" << nativeNumber(regionsTotalSec) << ",";
  json << "\"musicasTotalSec\":" << nativeNumber(regionsTotalSec) << ",";
  json << "\"totalMusicasSec\":" << nativeNumber(regionsTotalSec) << ",";
  json << "\"regionsTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(regionsTotalSec)) << ",";
  json << "\"totalRegionsText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(regionsTotalSec)) << ",";
  json << "\"musicasTotalText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(regionsTotalSec)) << ",";
  json << "\"totalMusicasText\":" << nativeJsonString(nativeFormatTotalTextFromSeconds(regionsTotalSec)) << ",";

  json << "\"playing\":" << (playing && !playingId.empty() ? "true" : "false") << ",";
  json << "\"isPlaying\":" << (playing && !playingId.empty() ? "true" : "false") << ",";
  json << "\"transportPlaying\":" << (playing ? "true" : "false") << ",";
  json << "\"paused\":" << (paused ? "true" : "false") << ",";
  json << "\"transportPaused\":" << (paused ? "true" : "false") << ",";
  json << "\"playingId\":" << (playingId.empty() ? "null" : nativeJsonString(playingId)) << ",";
  json << "\"playingSongId\":" << (playingId.empty() ? "null" : nativeJsonString(playingId)) << ",";
  json << "\"currentSongId\":" << (playingId.empty() ? "null" : nativeJsonString(playingId)) << ",";
  json << "\"currentSongName\":" << nativeJsonString(playingName) << ",";
  json << "\"playingSongName\":" << nativeJsonString(playingName) << ",";
  json << "\"songName\":" << nativeJsonString(playingName) << ",";
  json << "\"musicName\":" << nativeJsonString(playingName) << ",";
  json << "\"playPosition\":" << nativeNumber(playPos) << ",";
  json << "\"editCursorPosition\":" << nativeNumber(editCursorPos) << ",";
  json << "\"cursorPosition\":" << nativeNumber(editCursorPos) << ",";
  json << "\"currentSongStart\":" << nativeNumber(songStart) << ",";
  json << "\"currentSongEnd\":" << nativeNumber(songEnd) << ",";
  json << "\"currentSongDurationSec\":" << nativeNumber(duration) << ",";
  json << "\"currentSongElapsedSec\":" << nativeNumber(elapsed) << ",";
  json << "\"currentSongRemainingSec\":" << nativeNumber(remaining) << ",";
  json << "\"playbackDurationSec\":" << nativeNumber(duration) << ",";
  json << "\"playbackElapsedSec\":" << nativeNumber(elapsed) << ",";
  json << "\"playbackRemainingSec\":" << nativeNumber(remaining) << ",";
  json << "\"playbackStartPos\":" << nativeNumber(songStart) << ",";
  json << "\"playbackEndPos\":" << nativeNumber(songEnd) << ",";
  json << "\"durationSec\":" << nativeNumber(duration) << ",";
  json << "\"elapsedSec\":" << nativeNumber(elapsed) << ",";
  json << "\"remainingSec\":" << nativeNumber(remaining) << ",";
  json << "\"currentSongProgress\":" << nativeNumber(progress, 6) << ",";
  json << "\"autoplayEnabled\":" << (autoplayEnabled ? "true" : "false") << ",";
  json << "\"autoStopEnabled\":" << (autoStopEnabled ? "true" : "false") << ",";
  json << "\"autostopEnabled\":" << (autoStopEnabled ? "true" : "false") << ",";
  json << "\"auto_stop_enabled\":" << (autoStopEnabled ? "true" : "false") << ",";
  json << "\"autoStop\":" << (autoStopEnabled ? "true" : "false") << ",";
  json << "\"timerRunning\":" << (timerRunning ? "true" : "false") << ",";
  json << "\"timerActive\":" << (timerRunning ? "true" : "false") << ",";
  json << "\"timerEnabled\":" << (timerRunning ? "true" : "false") << ",";
  json << "\"timerMode\":" << nativeJsonString(timerMode) << ",";
  json << "\"timerType\":" << nativeJsonString(timerMode) << ",";
  json << "\"timerStartedAt\":" << nativeNumber(timerStartedAtMs) << ",";
  json << "\"timerStartedAtMs\":" << nativeNumber(timerStartedAtMs) << ",";
  json << "\"timerAccumulatedSec\":" << nativeNumber(timerAccumulatedSec) << ",";
  json << "\"timerTargetSec\":" << nativeNumber(timerTargetSec) << ",";
  json << "\"timerCountdownStartSec\":" << nativeNumber(timerTargetSec) << ",";
  json << "\"timerElapsedSec\":" << nativeNumber(timerElapsedSec) << ",";
  json << "\"timerDisplaySec\":" << nativeNumber(timerDisplaySec) << ",";
  json << "\"timerExpired\":" << (timerExpired ? "true" : "false") << ",";
  json << "\"timerOverrun\":" << (timerExpired ? "true" : "false") << ",";
  json << "\"timerNegative\":" << (timerExpired ? "true" : "false") << ",";
  json << "\"timerOverrunSec\":" << nativeNumber(timerOverrunSec) << ",";
  json << "\"timerDisplayText\":" << nativeJsonString(timerDisplayText) << ",";
  json << "\"timerText\":" << nativeJsonString(timerDisplayText) << ",";
  json << "\"timerLocalTimeText\":" << nativeJsonString(timerLocalTimeText) << ",";
  json << "\"timer\":" << timerJson << ",";
  json << "\"autoBlocoEnabled\":" << (autoBlocoEnabled ? "true" : "false") << ",";
  json << "\"autoBlocoArmed\":" << (autoBlocoEnabled ? "true" : "false") << ",";
  json << "\"autoBlocoActive\":" << ((autoplayEnabled && autoBlocoEnabled) ? "true" : "false") << ",";
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    json << "\"previewMode\":" << g_nativePreviewMode << ",";
    json << "\"previewActive\":" << (g_nativePreviewMode > 0 ? "true" : "false") << ",";
    json << "\"liveModeEnabled\":" << (g_nativeLiveMarkEnabled ? "true" : "false") << ",";
    json << "\"liveEnabled\":" << (g_nativeLiveMarkEnabled ? "true" : "false") << ",";
    json << "\"liveExecutedCount\":" << g_nativeLiveExecutedItems.size() << ",";
  }
  json << "\"loopActive\":" << (loopActive ? "true" : "false") << ",";
  json << "\"repeatEnabled\":" << (loopActive ? "true" : "false") << ",";
  json << "\"loopRangeActive\":" << (loopRangeActive ? "true" : "false") << ",";
  json << "\"loopStartPos\":" << nativeNumber(loopStartPos) << ",";
  json << "\"loopEndPos\":" << nativeNumber(loopEndPos) << ",";
  json << "\"loopStartMarkerName\":" << nativeJsonString(loopStartMarkerName) << ",";
  json << "\"loopEndMarkerName\":" << nativeJsonString(loopEndMarkerName) << ",";
  json << "\"loopRegionName\":" << nativeJsonString(loopRegionName) << ",";
  json << "\"loopSongName\":" << nativeJsonString(loopRegionName) << ",";
  json << "\"loopSourceType\":" << nativeJsonString(loopSourceType) << ",";
  json << "\"queuedSongId\":" << (queuedSongId.empty() ? std::string("null") : nativeJsonString(queuedSongId)) << ",";
  json << "\"queuedPlaylistSongId\":" << (queuedPlaylistSongId.empty() ? (queuedSongId.empty() ? std::string("null") : nativeJsonString(queuedSongId)) : nativeJsonString(queuedPlaylistSongId)) << ",";
  json << "\"queuedRegionNumber\":" << (queuedRegionNumber == 0 ? std::string("null") : nativeJsonString(std::to_string(queuedRegionNumber))) << ",";
  json << "\"queuedPlaylistIndex\":" << queuedPlaylistIndex << ",";
  json << "\"queuedPlaylistOrder\":" << queuedPlaylistIndex << ",";
  json << "\"queuedSongPlaylistIndex\":" << queuedPlaylistIndex << ",";
  json << "\"queuedManual\":" << (queuedManual ? "true" : "false") << ",";
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    json << "\"queuedKind\":" << nativeJsonString(g_nativeQueuedKind) << ",";
    json << "\"queueKind\":" << nativeJsonString(g_nativeQueuedKind) << ",";
  }
  json << "\"queueActive\":" << (!queuedSongId.empty() ? "true" : "false") << ",";
  json << "\"queuedStartPos\":" << nativeNumber(queuedStart) << ",";
  json << "\"queuedEndPos\":" << nativeNumber(queuedEnd) << ",";
  json << "\"autoBlocoTargetSongId\":" << (autoBlocoTargetSongId.empty() ? std::string("null") : nativeJsonString(autoBlocoTargetSongId)) << ",";
  json << "\"autoBlocoTargetPlaylistSongId\":" << (autoBlocoTargetPlaylistSongId.empty() ? (autoBlocoTargetSongId.empty() ? std::string("null") : nativeJsonString(autoBlocoTargetSongId)) : nativeJsonString(autoBlocoTargetPlaylistSongId)) << ",";
  json << "\"autoBlocoTargetRegionNumber\":" << (autoBlocoTargetRegionNumber == 0 ? std::string("null") : nativeJsonString(std::to_string(autoBlocoTargetRegionNumber))) << ",";
  json << "\"autoBlocoTargetStartPos\":" << nativeNumber(autoBlocoTargetStart) << ",";
  json << "\"autoBlocoTargetEndPos\":" << nativeNumber(autoBlocoTargetEnd) << ",";
  json << "\"autoBlocoTargetPlaylistIndex\":" << autoBlocoTargetPlaylistIndex << ",";
  json << "\"tp1\":" << tp1Json << ",";
  json << "\"tp2\":" << tp2Json << ",";
  json << "\"tp1LyricsText\":" << nativeJsonString(tp1LyricsText) << ",";
  json << "\"tp1Lyrics\":" << nativeJsonString(tp1LyricsText) << ",";
  json << "\"telepromptTp1Lyrics\":" << nativeJsonString(tp1LyricsText) << ",";
  json << "\"telepromptTp1Text\":" << nativeJsonString(tp1LyricsText) << ",";
  json << "\"tp1SongName\":" << nativeJsonString(tp1SongName) << ",";
  json << "\"telepromptTp1SongName\":" << nativeJsonString(tp1SongName) << ",";
  json << "\"tp1MediaType\":" << nativeJsonString(tp1MediaType.empty() ? std::string("text") : tp1MediaType) << ",";
  json << "\"telepromptTp1MediaType\":" << nativeJsonString(tp1MediaType.empty() ? std::string("text") : tp1MediaType) << ",";
  json << "\"tp1MediaPath\":" << nativeJsonString(tp1MediaPath) << ",";
  json << "\"telepromptTp1MediaPath\":" << nativeJsonString(tp1MediaPath) << ",";
  json << "\"tp1MediaUrl\":" << nativeJsonString(tp1MediaPath.empty() ? std::string() : std::string("/media?path=") + nativeUrlEncode(tp1MediaPath)) << ",";
  json << "\"telepromptTp1MediaUrl\":" << nativeJsonString(tp1MediaPath.empty() ? std::string() : std::string("/media?path=") + nativeUrlEncode(tp1MediaPath)) << ",";
  json << "\"tp1MediaExt\":" << nativeJsonString(tp1MediaExt) << ",";
  json << "\"tp1MediaCurrentTime\":" << (tp1MediaCurrentTime.empty() ? std::string("0") : tp1MediaCurrentTime) << ",";
  json << "\"tp1MediaOffset\":" << (tp1MediaOffset.empty() ? std::string("0") : tp1MediaOffset) << ",";
  json << "\"tp1MediaPlayrate\":" << (tp1MediaPlayrate.empty() ? std::string("1") : tp1MediaPlayrate) << ",";
  json << "\"tp1UpdatedAt\":" << nativeJsonString(tp1UpdatedAt) << ",";
  json << "\"tp2LyricsText\":" << nativeJsonString(tp2LyricsText) << ",";
  json << "\"tp2Lyrics\":" << nativeJsonString(tp2LyricsText) << ",";
  json << "\"telepromptTp2Lyrics\":" << nativeJsonString(tp2LyricsText) << ",";
  json << "\"telepromptTp2Text\":" << nativeJsonString(tp2LyricsText) << ",";
  json << "\"tp2SongName\":" << nativeJsonString(tp2SongName) << ",";
  json << "\"telepromptTp2SongName\":" << nativeJsonString(tp2SongName) << ",";
  json << "\"tp2MediaType\":" << nativeJsonString(tp2MediaType.empty() ? std::string("text") : tp2MediaType) << ",";
  json << "\"telepromptTp2MediaType\":" << nativeJsonString(tp2MediaType.empty() ? std::string("text") : tp2MediaType) << ",";
  json << "\"tp2MediaPath\":" << nativeJsonString(tp2MediaPath) << ",";
  json << "\"telepromptTp2MediaPath\":" << nativeJsonString(tp2MediaPath) << ",";
  json << "\"tp2MediaUrl\":" << nativeJsonString(tp2MediaPath.empty() ? std::string() : std::string("/media?path=") + nativeUrlEncode(tp2MediaPath)) << ",";
  json << "\"telepromptTp2MediaUrl\":" << nativeJsonString(tp2MediaPath.empty() ? std::string() : std::string("/media?path=") + nativeUrlEncode(tp2MediaPath)) << ",";
  json << "\"tp2MediaExt\":" << nativeJsonString(tp2MediaExt) << ",";
  json << "\"tp2MediaCurrentTime\":" << (tp2MediaCurrentTime.empty() ? std::string("0") : tp2MediaCurrentTime) << ",";
  json << "\"tp2MediaOffset\":" << (tp2MediaOffset.empty() ? std::string("0") : tp2MediaOffset) << ",";
  json << "\"tp2MediaPlayrate\":" << (tp2MediaPlayrate.empty() ? std::string("1") : tp2MediaPlayrate) << ",";
  json << "\"tp2UpdatedAt\":" << nativeJsonString(tp2UpdatedAt);
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    std::ostringstream openDrawerIds;
    bool firstOpenDrawerId = true;
    for (const auto& entry : g_nativeDirectorOpenFamilyDrawers) {
      if (!entry.second || entry.first.empty()) continue;
      if (!firstOpenDrawerId) openDrawerIds << '|';
      openDrawerIds << entry.first;
      firstOpenDrawerId = false;
    }
    json << ",\"selectedPlaylistSongId\":" << ((g_nativeSelectedTab == "playlist" && !g_nativeSelectedId.empty()) ? nativeJsonString(g_nativeSelectedId) : std::string("null"));
    json << ",\"selectedRegionId\":" << ((g_nativeSelectedTab == "regions" && !g_nativeSelectedId.empty()) ? nativeJsonString(g_nativeSelectedId) : std::string("null"));
    json << ",\"selectedSongId\":" << (!g_nativeSelectedId.empty() ? nativeJsonString(g_nativeSelectedId) : std::string("null"));
    json << ",\"selectedStartPos\":" << nativeNumber(g_nativeSelectedStart);
    json << ",\"selectedEndPos\":" << nativeNumber(g_nativeSelectedEnd);
    json << ",\"selectedMarkerId\":" << (g_nativeSelectedMarkerId.empty() ? std::string("null") : nativeJsonString(g_nativeSelectedMarkerId));
    json << ",\"armedMarkerId\":" << (g_nativeArmedMarkerId.empty() ? std::string("null") : nativeJsonString(g_nativeArmedMarkerId));
    json << ",\"markerGoId\":" << (g_nativeArmedMarkerId.empty() ? std::string("null") : nativeJsonString(g_nativeArmedMarkerId));
    json << ",\"selectedMarkerPos\":" << nativeNumber(g_nativeSelectedMarkerPos);
    json << ",\"selectionUpdatedAt\":" << nativeJsonString(nowIso);
    json << ",\"openDrawerIds\":" << nativeJsonString(openDrawerIds.str());
    json << ",\"familyDrawerOpenIds\":" << nativeJsonString(openDrawerIds.str());
    json << ",\"familyDrawerOwner\":" << nativeJsonString(g_nativeFamilyDrawersOwner);
    json << ",\"familyDrawerRevision\":" << g_nativeFamilyDrawersRevision;
    json << ",\"drawerOutlineEnabled\":" << (g_nativeDrawerOutlineEnabled ? "true" : "false");
    json << ",\"drawerOutlineColor\":" << nativeJsonString(g_nativeDrawerOutlineColor);
    json << ",\"drawerSymbolEnabled\":" << (g_nativeDrawerSymbolEnabled ? "true" : "false");
    json << ",\"drawerSymbolColor\":" << nativeJsonString(g_nativeDrawerSymbolColor);
  }
  // Native premix vem por ultimo para sobrepor qualquer fragmento antigo do Lua.
  json << ",\"premix\":" << premixJson;
  json << ",\"multiloops\":" << multiLoopsJson;
  json << "}";

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeStateJson = json.str();
    g_nativeSongWindows = std::move(songs);
    g_nativeActivePlaylistItems = std::move(activePlaylistItems);
  }
  if (SetExtState_ptr) {
    // HTTP e API nativa usam g_nativeStateJson diretamente. O espelho legado em
    // ExtState nao precisa receber mais de 400 KB a cada 220 ms.
    static std::chrono::steady_clock::time_point lastLegacyPublish;
    const auto legacyNow = std::chrono::steady_clock::now();
    if (lastLegacyPublish.time_since_epoch().count() == 0 ||
        std::chrono::duration_cast<std::chrono::milliseconds>(legacyNow - lastLegacyPublish).count() >= 1000) {
      const std::string legacyPayload = json.str();
      SetExtState_ptr(kNativeExtStateSection, "NATIVE_STATE_JSON_V1", legacyPayload.c_str(), false);
      lastLegacyPublish = legacyNow;
    }
  }
}


static void nativePublishCommandsToExtState()
{
  if (!SetExtState_ptr) return;

  std::string payload;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    std::ostringstream oss;
    oss << "{\"seq\":" << static_cast<unsigned long long>(g_nativeCommandSequence) << ",\"commands\":[";
    // FIX12: não publica histórico antigo em ExtState.
    // O Lua puxa a fila real por VS_Hook_Native_PullCommand; publicar histórico fazia comandos velhos
    // voltarem quando o script era fechado e aberto novamente.
    for (size_t i = 0; i < g_nativeCommandQueue.size(); ++i) {
      if (i) oss << ",";
      oss << g_nativeCommandQueue[i];
    }
    oss << "]}";
    payload = oss.str();
  }

  if (payload == g_nativePublishedCommandsSignature) return;
  g_nativePublishedCommandsSignature = payload;
  SetExtState_ptr(kNativeExtStateSection, kNativeCommandsExtKey, payload.c_str(), false);
}

static void nativeBridgeTick()
{
  nativePublishCommandsToExtState();
  const auto now = std::chrono::steady_clock::now();
  const bool forceLive = g_nativeForceStateBuild.exchange(false);
  const bool forceSnapshot = g_nativeForceSnapshotBuild.exchange(false);
  const bool liveDue = forceLive || forceSnapshot || g_nativeLastLiveBuild.time_since_epoch().count() == 0 ||
    std::chrono::duration_cast<std::chrono::milliseconds>(now - g_nativeLastLiveBuild).count() >= kNativeBridgeLiveIntervalMs;
  if (!liveDue) return;
  g_nativeLastLiveBuild = now;
  nativeRebuildState(forceSnapshot);
}

static bool VS_Hook_Native_PullCommand(char* buf, int bufSize)
{
  if (!buf || bufSize <= 0) return false;
  buf[0] = '\0';
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (g_nativeCommandQueue.empty()) return false;
  std::string command = g_nativeCommandQueue.front();
  g_nativeCommandQueue.pop_front();
  std::strncpy(buf, command.c_str(), static_cast<size_t>(bufSize - 1));
  buf[bufSize - 1] = '\0';
  return true;
}

static bool VS_Hook_Native_SetLuaState(const char* jsonFragment)
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  g_nativeLuaLiveFragment = jsonFragment ? jsonFragment : "";
  return true;
}

static bool VS_Hook_Native_SetActivePlaylist(const char* playlistName)
{
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project || !SetProjExtState_ptr) return false;

  const std::string nextName = nativeTrim(playlistName ? playlistName : "");
  const std::string currentName = nativeTrim(nativeGetProjExtStateString(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1"));
  if (currentName != nextName) {
    SetProjExtState_ptr(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1", nextName.c_str());
    if (MarkProjectDirty_ptr) MarkProjectDirty_ptr(project);
  }

  // O Lua ja atualizou o ProjExtState. Esta chamada existe para a extensao
  // publicar o novo repertorio ativo imediatamente no snapshot do App Diretor.
  g_nativeForceSnapshotBuild.store(true);
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool VS_Hook_Native_GetState(char* buf, int bufSize)
{
  if (!buf || bufSize <= 0) return false;
  buf[0] = '\0';
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  const std::string state = g_nativeStateJson.empty() ? std::string("{\"ok\":false,\"connected\":false}") : g_nativeStateJson;
  std::strncpy(buf, state.c_str(), static_cast<size_t>(bufSize - 1));
  buf[bufSize - 1] = '\0';
  return true;
}

static bool VS_Hook_Native_ResumePcAccess()
{
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeLastDirectorHeartbeat = std::chrono::steady_clock::time_point();
  }
  g_pcResumeRequested.store(true);
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool VS_Hook_Native_ClearQueue()
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  nativeClearAllQueueStateLocked();
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool VS_Hook_Native_StopTransportReadyFromQueue()
{
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!Main_OnCommand_ptr) return false;
  g_transportCommandBypass = true;
  const bool ok = nativeStopTransportAndPrepareSelectionFromQueueOrCurrent(project, true);
  g_transportCommandBypass = false;
  return ok;
}

static bool VS_Hook_Native_SetManualQueueByPlaylistOrder(int playlistOrder)
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  const NativeSongWindow* song = nativeFindActivePlaylistSongByOrderLocked(playlistOrder);
  if (!song || !nativeSongIsPlayable(*song)) return false;
  nativeSetQueuedSongLocked(*song, true);
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool VS_Hook_Native_ArmAutoQueueFromPlaylistOrder(int currentPlaylistOrder)
{
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  nativeLoadAutomationSettingsOnceLocked();
  if (!g_nativeAutoplayEnabled) {
    if (!g_nativeQueuedManual) nativeClearQueuedSongLocked();
    nativeClearAutoBlocoTargetLocked();
    g_nativeForceStateBuild.store(true);
    return false;
  }
  if (g_nativeQueuedManual && !g_nativeQueuedSongId.empty()) return true;

  int currentIndex = -1;
  for (int i = 0; i < static_cast<int>(g_nativeActivePlaylistItems.size()); ++i) {
    const NativeSongWindow& item = g_nativeActivePlaylistItems[static_cast<size_t>(i)];
    if (!nativeSongIsPlayable(item)) continue;
    if (item.playlistOrder == currentPlaylistOrder) { currentIndex = i; break; }
  }
  if (currentIndex < 0 && currentPlaylistOrder > 0 && currentPlaylistOrder <= static_cast<int>(g_nativeActivePlaylistItems.size())) {
    currentIndex = currentPlaylistOrder - 1;
  }

  const NativeSongWindow* boundaryTarget = nullptr;
  const NativeSongWindow* next = nativeResolveNextAutoQueueTarget(g_nativeActivePlaylistItems, currentIndex, g_nativeAutoBlocoEnabled, &boundaryTarget);
  if (next && nativeSongIsPlayable(*next) && !nativeSongIsCurrentPlayingLocked(*next)) {
    nativeSetQueuedSongLocked(*next, false);
    g_nativeForceStateBuild.store(true);
    return true;
  }

  nativeClearQueuedSongLocked();
  if (boundaryTarget && nativeSongIsPlayable(*boundaryTarget)) {
    nativeSetAutoBlocoTargetLocked(*boundaryTarget);
    g_nativeForceStateBuild.store(true);
    return false;
  }
  nativeClearAutoBlocoTargetLocked();
  g_nativeForceStateBuild.store(true);
  return false;
}

static bool VS_Hook_Native_ToggleMultiLoopBypass()
{
  g_nativeMultiLoopBypassRequest.store(2);
  g_nativeForceStateBuild.store(true);
  return true;
}

static char g_apiDefNativePullCommand[] =
  "bool\0char*,int\0commandJsonOutNeedBig,commandJsonOutNeedBig_sz\0Pull one pending VS Hook command from the native bridge.";
static char g_apiDefNativeSetLuaState[] =
  "bool\0const char*\0jsonFragment\0Send small Lua-only live state fields to the VS Hook native bridge.";
static char g_apiDefNativeSetActivePlaylist[] =
  "bool\0const char*\0playlistName\0Publish the playlist selected locally in VS Hook Lua to the native bridge.";
static char g_apiDefNativeGetState[] =
  "bool\0char*,int\0stateJsonOutNeedBig,stateJsonOutNeedBig_sz\0Read the current VS Hook native bridge state.";
static char g_apiDefNativeResumePcAccess[] =
  "bool\0\0\0Resume VS Hook access on the PC and reopen the VS Hook script closed by APP ATIVO.";
static char g_apiDefNativeClearQueue[] =
  "bool\0\0\0Clear the VS Hook native queue immediately.";
static char g_apiDefNativeStopTransportReadyFromQueue[] =
  "bool\0\0\0Stop transport and make queued/current song ready for next Play.";
static char g_apiDefNativeSetManualQueueByPlaylistOrder[] =
  "bool\0int\0playlistOrder\0Set the VS Hook native manual queue by active playlist order.";
static char g_apiDefNativeArmAutoQueueFromPlaylistOrder[] =
  "bool\0int\0currentPlaylistOrder\0Arm the next automatic VS Hook queue target from the active playlist order.";
static char g_apiDefNativeToggleMultiLoopBypass[] =
  "bool\0\0\0Toggle the temporary Multiloops bypass for the current playback pass.";

static bool registerNativeBridgeApi()
{
  if (!plugin_register_ptr) return false;
  bool ok = true;
  ok = (plugin_register_ptr("API_VS_Hook_Native_PullCommand", reinterpret_cast<void*>(&VS_Hook_Native_PullCommand)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_PullCommand", reinterpret_cast<void*>(g_apiDefNativePullCommand)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_SetLuaState", reinterpret_cast<void*>(&VS_Hook_Native_SetLuaState)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_SetLuaState", reinterpret_cast<void*>(g_apiDefNativeSetLuaState)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_SetActivePlaylist", reinterpret_cast<void*>(&VS_Hook_Native_SetActivePlaylist)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_SetActivePlaylist", reinterpret_cast<void*>(g_apiDefNativeSetActivePlaylist)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_GetState", reinterpret_cast<void*>(&VS_Hook_Native_GetState)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_GetState", reinterpret_cast<void*>(g_apiDefNativeGetState)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_ResumePcAccess", reinterpret_cast<void*>(&VS_Hook_Native_ResumePcAccess)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_ResumePcAccess", reinterpret_cast<void*>(g_apiDefNativeResumePcAccess)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_ClearQueue", reinterpret_cast<void*>(&VS_Hook_Native_ClearQueue)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_ClearQueue", reinterpret_cast<void*>(g_apiDefNativeClearQueue)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_StopTransportReadyFromQueue", reinterpret_cast<void*>(&VS_Hook_Native_StopTransportReadyFromQueue)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_StopTransportReadyFromQueue", reinterpret_cast<void*>(g_apiDefNativeStopTransportReadyFromQueue)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_SetManualQueueByPlaylistOrder", reinterpret_cast<void*>(&VS_Hook_Native_SetManualQueueByPlaylistOrder)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_SetManualQueueByPlaylistOrder", reinterpret_cast<void*>(g_apiDefNativeSetManualQueueByPlaylistOrder)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_ArmAutoQueueFromPlaylistOrder", reinterpret_cast<void*>(&VS_Hook_Native_ArmAutoQueueFromPlaylistOrder)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_ArmAutoQueueFromPlaylistOrder", reinterpret_cast<void*>(g_apiDefNativeArmAutoQueueFromPlaylistOrder)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_ToggleMultiLoopBypass", reinterpret_cast<void*>(&VS_Hook_Native_ToggleMultiLoopBypass)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_ToggleMultiLoopBypass", reinterpret_cast<void*>(g_apiDefNativeToggleMultiLoopBypass)) != 0) && ok;
  return ok;
}

static void unregisterNativeBridgeApi()
{
  if (!plugin_register_ptr) return;
  plugin_register_ptr("-APIdef_VS_Hook_Native_ToggleMultiLoopBypass", reinterpret_cast<void*>(g_apiDefNativeToggleMultiLoopBypass));
  plugin_register_ptr("-API_VS_Hook_Native_ToggleMultiLoopBypass", reinterpret_cast<void*>(&VS_Hook_Native_ToggleMultiLoopBypass));
  plugin_register_ptr("-APIdef_VS_Hook_Native_ArmAutoQueueFromPlaylistOrder", reinterpret_cast<void*>(g_apiDefNativeArmAutoQueueFromPlaylistOrder));
  plugin_register_ptr("-API_VS_Hook_Native_ArmAutoQueueFromPlaylistOrder", reinterpret_cast<void*>(&VS_Hook_Native_ArmAutoQueueFromPlaylistOrder));
  plugin_register_ptr("-APIdef_VS_Hook_Native_SetManualQueueByPlaylistOrder", reinterpret_cast<void*>(g_apiDefNativeSetManualQueueByPlaylistOrder));
  plugin_register_ptr("-API_VS_Hook_Native_SetManualQueueByPlaylistOrder", reinterpret_cast<void*>(&VS_Hook_Native_SetManualQueueByPlaylistOrder));
  plugin_register_ptr("-APIdef_VS_Hook_Native_StopTransportReadyFromQueue", reinterpret_cast<void*>(g_apiDefNativeStopTransportReadyFromQueue));
  plugin_register_ptr("-API_VS_Hook_Native_StopTransportReadyFromQueue", reinterpret_cast<void*>(&VS_Hook_Native_StopTransportReadyFromQueue));
  plugin_register_ptr("-APIdef_VS_Hook_Native_ClearQueue", reinterpret_cast<void*>(g_apiDefNativeClearQueue));
  plugin_register_ptr("-API_VS_Hook_Native_ClearQueue", reinterpret_cast<void*>(&VS_Hook_Native_ClearQueue));
  plugin_register_ptr("-APIdef_VS_Hook_Native_ResumePcAccess", reinterpret_cast<void*>(g_apiDefNativeResumePcAccess));
  plugin_register_ptr("-API_VS_Hook_Native_ResumePcAccess", reinterpret_cast<void*>(&VS_Hook_Native_ResumePcAccess));
  plugin_register_ptr("-APIdef_VS_Hook_Native_GetState", reinterpret_cast<void*>(g_apiDefNativeGetState));
  plugin_register_ptr("-API_VS_Hook_Native_GetState", reinterpret_cast<void*>(&VS_Hook_Native_GetState));
  plugin_register_ptr("-APIdef_VS_Hook_Native_SetActivePlaylist", reinterpret_cast<void*>(g_apiDefNativeSetActivePlaylist));
  plugin_register_ptr("-API_VS_Hook_Native_SetActivePlaylist", reinterpret_cast<void*>(&VS_Hook_Native_SetActivePlaylist));
  plugin_register_ptr("-APIdef_VS_Hook_Native_SetLuaState", reinterpret_cast<void*>(g_apiDefNativeSetLuaState));
  plugin_register_ptr("-API_VS_Hook_Native_SetLuaState", reinterpret_cast<void*>(&VS_Hook_Native_SetLuaState));
  plugin_register_ptr("-APIdef_VS_Hook_Native_PullCommand", reinterpret_cast<void*>(g_apiDefNativePullCommand));
  plugin_register_ptr("-API_VS_Hook_Native_PullCommand", reinterpret_cast<void*>(&VS_Hook_Native_PullCommand));
}

static std::string nativeHttpResponse(int status, const std::string& body, const char* contentType = "application/json; charset=utf-8")
{
  std::ostringstream res;
  res << "HTTP/1.1 " << status << (status == 200 ? " OK" : " ERROR") << "\r\n";
  res << "Content-Type: " << contentType << "\r\n";
  res << "Access-Control-Allow-Origin: *\r\n";
  res << "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n";
  res << "Access-Control-Allow-Headers: Content-Type\r\n";
  res << "Cache-Control: no-store\r\n";
  res << "Connection: close\r\n";
  res << "Content-Length: " << body.size() << "\r\n\r\n";
  res << body;
  return res.str();
}

static std::string nativeRequestPath(const std::string& req)
{
  const size_t sp1 = req.find(' ');
  if (sp1 == std::string::npos) return "/";
  const size_t sp2 = req.find(' ', sp1 + 1);
  if (sp2 == std::string::npos) return "/";
  std::string path = req.substr(sp1 + 1, sp2 - sp1 - 1);
  const size_t q = path.find('?');
  if (q != std::string::npos) path = path.substr(0, q);
  return path.empty() ? "/" : path;
}

static bool nativeRequestIsPost(const std::string& req)
{
  return nativeStartsWith(req, "POST ");
}

static std::string nativeRequestBody(const std::string& req)
{
  size_t p = req.find("\r\n\r\n");
  if (p == std::string::npos) return std::string();
  return req.substr(p + 4);
}

static size_t nativeHttpHeaderEnd(const std::string& req)
{
  size_t p = req.find("\r\n\r\n");
  if (p != std::string::npos) return p + 4;
  p = req.find("\n\n");
  if (p != std::string::npos) return p + 2;
  return std::string::npos;
}

static size_t nativeHttpContentLength(const std::string& req)
{
  size_t headerEnd = nativeHttpHeaderEnd(req);
  if (headerEnd == std::string::npos) headerEnd = req.size();
  std::string headers = req.substr(0, headerEnd);

  const char* keys[] = { "Content-Length:", "content-length:", "CONTENT-LENGTH:" };
  for (const char* key : keys) {
    size_t p = headers.find(key);
    if (p == std::string::npos) continue;
    p += std::strlen(key);
    while (p < headers.size() && (headers[p] == ' ' || headers[p] == '\t')) ++p;
    size_t e = p;
    while (e < headers.size() && headers[e] >= '0' && headers[e] <= '9') ++e;
    if (e > p) {
      try { return static_cast<size_t>(std::stoul(headers.substr(p, e - p))); } catch (...) { return 0; }
    }
  }
  return 0;
}

static std::string nativeReadHttpRequest(native_socket_t client)
{
  std::string req;
  req.reserve(8192);
  char buffer[8192];
  const size_t maxRequestBytes = 1024 * 1024;

  for (;;) {
    int received = 0;
#ifdef _WIN32
    received = recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
    received = static_cast<int>(recv(client, buffer, sizeof(buffer), 0));
#endif
    if (received <= 0) break;
    req.append(buffer, static_cast<size_t>(received));
    if (req.size() > maxRequestBytes) break;

    const size_t headerEnd = nativeHttpHeaderEnd(req);
    if (headerEnd != std::string::npos) {
      const size_t contentLength = nativeHttpContentLength(req);
      if (req.size() >= headerEnd + contentLength) break;
    }
  }

  return req;
}


static double nativeRatioToVolume(double ratio)
{
  if (!std::isfinite(ratio)) ratio = 0.5;
  ratio = std::max(0.0, std::min(1.0, ratio));
  const double db = ratio * 72.0 - 60.0;
  return std::pow(10.0, db / 20.0);
}

static bool nativeLooksNumeric(const std::string& value)
{
  const std::string text = nativeTrim(value);
  if (text.empty()) return false;
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  return end && end != text.c_str() && *end == '\0' && std::isfinite(parsed);
}

static MediaTrack* nativeFindTrackById(ReaProject* project, const std::string& rawId)
{
  const std::string id = nativeTrim(rawId);
  if (id.empty()) return nullptr;
  if (id == "MASTER_TRACK" || id == "MASTER" || id == "master") return GetMasterTrack_ptr ? GetMasterTrack_ptr(project) : nullptr;
  if (!CountTracks_ptr || !GetTrack_ptr) return nullptr;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* tr = GetTrack_ptr(project, i);
    if (!tr) continue;
    const std::string guid = nativeTrackGuid(tr, i);
    if (guid == id) return tr;
    if (nativeLooksNumeric(id) && std::atoi(id.c_str()) == i + 1) return tr;
  }
  return nullptr;
}

static bool nativeApplyMixerCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "mixer_set_volume" && type != "mixer_toggle_mute" && type != "mixer_toggle_solo") return false;
  if (!EnumProjects_ptr) return false;
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return false;
  std::string id = nativeJsonExtractString(commandBody, "targetId");
  if (id.empty()) id = nativeJsonExtractString(commandBody, "trackId");
  if (id.empty()) id = nativeJsonExtractString(commandBody, "guid");
  if (id.empty()) id = nativeJsonExtractString(commandBody, "id");
  MediaTrack* tr = nativeFindTrackById(project, id);
  if (!tr || !SetMediaTrackInfo_Value_ptr) return false;
  bool changed = false;
  if (type == "mixer_set_volume") {
    std::string rv = nativeJsonExtractString(commandBody, "ratio");
    if (rv.empty()) rv = nativeJsonExtractString(commandBody, "volumeRatio");
    if (rv.empty()) rv = nativeJsonExtractString(commandBody, "scrollRatio");
    if (rv.empty()) rv = nativeJsonExtractString(commandBody, "value");
    double ratio = rv.empty() ? 0.5 : std::atof(rv.c_str());
    changed = SetMediaTrackInfo_Value_ptr(tr, "D_VOL", nativeRatioToVolume(ratio));
  } else if (type == "mixer_toggle_mute") {
    const double cur = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(tr, "B_MUTE") : 0.0;
    changed = SetMediaTrackInfo_Value_ptr(tr, "B_MUTE", cur > 0.5 ? 0.0 : 1.0);
  } else if (type == "mixer_toggle_solo") {
    const double cur = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(tr, "I_SOLO") : 0.0;
    changed = SetMediaTrackInfo_Value_ptr(tr, "I_SOLO", cur > 0.5 ? 0.0 : 1.0);
  }
  if (changed) {
    if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
    if (UpdateArrange_ptr) UpdateArrange_ptr();
    // O snapshot pesado agora e desacoplado do refresh ao vivo. Comandos de
    // mixer precisam pedi-lo explicitamente para o estado devolvido ao app
    // refletir mute/solo/volume no ciclo seguinte.
    g_nativeForceSnapshotBuild.store(true);
    g_nativeForceStateBuild.store(true);
  }
  return changed;
}


static const NativeSongWindow* nativeFindSongForCommand(const std::string& idValue, double startPos, double endPos)
{
  const NativeSongWindow* firstIdMatch = nullptr;
  const NativeSongWindow* best = nullptr;
  double bestScore = 1e99;

  auto scan = [&](const std::vector<NativeSongWindow>& list) {
    for (const auto& s : list) {
      if (s.isBlock) continue;
      const bool idMatch = nativeSongIdMatches(s, idValue);
      const bool hasRange = startPos > 0.0 || endPos > 0.0;
      const double score = std::fabs(s.start - startPos) + std::fabs(s.end - endPos);
      if (idMatch && !firstIdMatch) firstIdMatch = &s;
      if (hasRange && idMatch && score < bestScore) { best = &s; bestScore = score; }
    }
  };

  scan(g_nativeActivePlaylistItems);
  scan(g_nativeSongWindows);

  if (best) return best;
  if (firstIdMatch) return firstIdMatch;
  if (startPos > 0.0 || endPos > 0.0) {
    auto scanByRange = [&](const std::vector<NativeSongWindow>& list) {
      for (const auto& s : list) {
        if (s.isBlock) continue;
        const double score = std::fabs(s.start - startPos) + std::fabs(s.end - endPos);
        if (score < bestScore) { best = &s; bestScore = score; }
      }
    };
    scanByRange(g_nativeActivePlaylistItems);
    scanByRange(g_nativeSongWindows);
  }
  return best;
}


static bool nativeApplyQueueCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "queue_playlist_song" && type != "queue_region_song" && type != "clear_queue" && type != "clear_queued_song") return false;

  if (type == "clear_queue" || type == "clear_queued_song") {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeClearAllQueueStateLocked();
    nativeBumpSharedRevisionLocked("director");
    g_nativeForceStateBuild.store(true);
    return true;
  }

  std::string idValue = nativeJsonExtractString(commandBody, "targetId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "songId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "selectedPlaylistSongId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "selectedRegionId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "id");

  const std::string sourceValue = nativeLower(nativeJsonExtractString(commandBody, "source"));
  const std::string roleValue = nativeLower(nativeJsonExtractString(commandBody, "role"));
  const std::string clientRoleValue = nativeLower(nativeJsonExtractString(commandBody, "clientRole"));
  const bool fromLua = sourceValue == "vshook_lua" || roleValue == "lua" || clientRoleValue == "lua";
  std::string manualValue = nativeJsonExtractString(commandBody, "manual");
  if (manualValue.empty()) manualValue = nativeJsonExtractString(commandBody, "queuedManual");
  std::string autoQueueValue = nativeJsonExtractString(commandBody, "autoQueue");
  if (autoQueueValue.empty()) autoQueueValue = nativeJsonExtractString(commandBody, "auto");
  const bool commandAutoQueue = nativeBoolFromText(autoQueueValue, false);
  const bool commandManualQueue = commandAutoQueue ? false : nativeBoolFromText(manualValue, true);

  std::string playlistOrderValue = nativeJsonExtractString(commandBody, "playlistItemIndex");
  if (playlistOrderValue.empty()) playlistOrderValue = nativeJsonExtractString(commandBody, "playlistSongIndex");
  if (playlistOrderValue.empty()) playlistOrderValue = nativeJsonExtractString(commandBody, "playlistOrder");
  if (playlistOrderValue.empty()) playlistOrderValue = nativeJsonExtractString(commandBody, "selectedPlaylistIndex");
  if (playlistOrderValue.empty()) playlistOrderValue = nativeJsonExtractString(commandBody, "songIndex");
  if (playlistOrderValue.empty()) playlistOrderValue = nativeJsonExtractString(commandBody, "index");
  if (playlistOrderValue.empty()) playlistOrderValue = nativeJsonExtractString(commandBody, "order");
  // O Lua FIX99 envia playlistIndex como indice do item selecionado no repertorio.
  // Para clientes externos, playlistIndex pode significar o ID do repertorio; por isso
  // so confiamos nele quando o comando vem do Lua ou quando nao veio nenhum id.
  if (playlistOrderValue.empty() && (fromLua || idValue.empty())) playlistOrderValue = nativeJsonExtractString(commandBody, "playlistIndex");
  const int playlistOrder = nativeLooksNumeric(playlistOrderValue) ? std::atoi(playlistOrderValue.c_str()) : 0;

  std::string startValue = nativeJsonExtractString(commandBody, "selectedStartPos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "startPos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "start_pos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "queuedStartPos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "queuedRegionStart");
  std::string endValue = nativeJsonExtractString(commandBody, "selectedEndPos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "endPos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "end_pos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "queuedEndPos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "queuedRegionEnd");
  const double startPos = nativeLooksNumeric(startValue) ? std::atof(startValue.c_str()) : 0.0;
  const double endPos = nativeLooksNumeric(endValue) ? std::atof(endValue.c_str()) : 0.0;

  std::string exactValue = nativeJsonExtractString(commandBody, "queueExactPosition");
  if (exactValue.empty()) exactValue = nativeJsonExtractString(commandBody, "exactPosition");
  if (exactValue.empty()) exactValue = nativeJsonExtractString(commandBody, "useExplicitPosition");
  const bool useExplicitPosition = nativeBoolFromText(exactValue, false);

  std::lock_guard<std::mutex> lock(g_nativeMutex);
  const bool hasExplicitBounds = endPos > startPos + 0.0005;
  const NativeSongWindow* explicitSong = nativeFindSongForCommand(idValue, startPos, endPos);

  // Um filho pode entrar na fila a partir de outra musica/familia, mas nunca
  // enquanto qualquer irmao do mesmo pai estiver tocando.
  if (explicitSong && nativeQueuedChildConflictsWithPlayingFamilyLocked(*explicitSong)) {
    g_nativeForceStateBuild.store(true);
    return true;
  }

  // O Diretor envia os limites exatos do filho; eles precisam ganhar da
  // resolucao por indice do repertorio, que pode apontar para o pai visivel.
  if (useExplicitPosition && hasExplicitBounds && !idValue.empty()) {
    const bool isCurrentExplicitChild = g_nativeCurrentTransportPlaying &&
      (g_nativeCurrentPlayingId == idValue ||
       (std::fabs(g_nativeCurrentSongStart - startPos) <= 0.002 &&
        std::fabs(g_nativeCurrentSongEnd - endPos) <= 0.002));

    // Clicar na musica que ja toca apenas a mantem em foco. Uma fila diferente
    // que ja esteja armada deve permanecer intacta.
    if (isCurrentExplicitChild) {
      g_nativeForceStateBuild.store(true);
      return true;
    }

    const bool sameQueuedTarget = !g_nativeQueuedSongId.empty() && g_nativeQueuedManual &&
      (g_nativeQueuedSongId == idValue ||
       (std::fabs(g_nativeQueuedStart - startPos) <= 0.002 &&
        std::fabs(g_nativeQueuedEnd - endPos) <= 0.002));

    if (commandManualQueue && sameQueuedTarget) {
      nativeClearQueuedSongLocked();
    } else {
      g_nativeQueuedSongId = idValue;
      g_nativeQueuedPlaylistSongId = idValue;
      g_nativeQueuedRegionNumber = 0;
      g_nativeQueuedStart = startPos;
      g_nativeQueuedEnd = endPos;
      g_nativeQueuedPlaylistIndex = playlistOrder;
      g_nativeQueuedManual = commandManualQueue;
      g_nativeQueuedKind = type == "queue_region_song" ? "regions" : "playlist";
      g_nativeQueuedSeekSignature.clear();
      nativeClearAutoBlocoTargetLocked();
      nativeCancelQueueHandoffProtectionLocked();
      if (commandManualQueue) g_nativeLastAutoQueueForPlayingId.clear();
      else g_nativeLastAutoQueueForPlayingId = g_nativeCurrentPlayingId;
    }

    nativeBumpSharedRevisionLocked(fromLua ? "lua" : "director");
    g_nativeForceStateBuild.store(true);
    return true;
  }

  const NativeSongWindow* song = (fromLua && hasExplicitBounds) ? nativeFindSongForCommand(idValue, startPos, endPos) : nullptr;
  if (!song) song = nativeFindActivePlaylistSongByOrderLocked(playlistOrder);
  if (!song) song = nativeFindSongForCommand(idValue, startPos, endPos);
  if (!song || !nativeSongIsPlayable(*song)) return true;
  if (nativeQueuedChildConflictsWithPlayingFamilyLocked(*song)) {
    g_nativeForceStateBuild.store(true);
    return true;
  }

  const std::string commandQueueKind = type == "queue_region_song" ? "regions" : "playlist";
  if (nativeSongIsCurrentPlayingLocked(*song)) {
    nativeClearQueuedSongLocked();
  } else if (commandManualQueue && !g_nativeQueuedSongId.empty() && g_nativeQueuedManual &&
    ((g_nativeQueuedPlaylistIndex > 0 && playlistOrder == g_nativeQueuedPlaylistIndex) ||
      nativeSongIdMatches(*song, g_nativeQueuedSongId) ||
      nativeSongBoundsMatch(*song, g_nativeQueuedStart, g_nativeQueuedEnd))) {
    nativeClearQueuedSongLocked();
  } else {
    // O Lua escolhe o alvo. Auto vindo do Lua entra como fila automatica;
    // clique manual continua entrando como fila manual.
    nativeSetQueuedSongLocked(*song, commandManualQueue, commandQueueKind);
  }
  nativeBumpSharedRevisionLocked(fromLua ? "lua" : "director");
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeApplyAutoCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  const bool isAuto = (type == "autoplay_toggle" || type == "auto_toggle" || type == "director_auto_toggle" || type == "autoplay_set" || type == "auto_set");
  const bool isAtBl = (type == "auto_bloco_toggle" || type == "at_bl_toggle" || type == "atbl_toggle" || type == "director_at_bl_toggle" || type == "auto_bloco_set" || type == "at_bl_set");
  const bool isAutoStop = (type == "auto_stop_toggle" || type == "autostop_toggle" || type == "director_auto_stop_toggle" ||
    type == "auto_stop_set" || type == "autostop_set" || type == "set_auto_stop" || type == "set_autostop" ||
    type == "toggle_auto_stop" || type == "toggle_autostop" || type == "auto_stop" || type == "autostop");
  if (!isAuto && !isAtBl && !isAutoStop) return false;

  std::string desired = nativeJsonExtractString(commandBody, "desiredState");
  if (isAutoStop) {
    std::string specific = nativeJsonExtractString(commandBody, "desiredAutoStop");
    if (specific.empty()) specific = nativeJsonExtractString(commandBody, "autoStopEnabled");
    if (specific.empty()) specific = nativeJsonExtractString(commandBody, "autostopEnabled");
    if (specific.empty()) specific = nativeJsonExtractString(commandBody, "enabled");
    if (!specific.empty()) desired = specific;
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    nativeLoadAutomationSettingsOnceLocked();
    const bool next = desired.empty() ? !g_nativeAutoStopEnabled : nativeBoolFromText(desired, g_nativeAutoStopEnabled);
    g_nativeAutoStopEnabled = next;
    if (!g_nativeAutoStopEnabled) g_nativeAutoStopLastStoppedSignature.clear();
    nativeSaveAutoStopEnabledLocked();
    nativeBumpSharedRevisionLocked("director");
    g_nativeForceStateBuild.store(true);
    return true;
  }

  if (isAuto) {
    std::string specific = nativeJsonExtractString(commandBody, "desiredAutoplay");
    if (!specific.empty()) desired = specific;
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const bool next = desired.empty() ? !g_nativeAutoplayEnabled : nativeBoolFromText(desired, g_nativeAutoplayEnabled);
    g_nativeAutoplayEnabled = next;
    if (!g_nativeAutoplayEnabled) {
      nativeClearAutoBlocoTargetLocked();
      if (!g_nativeQueuedManual) nativeClearQueuedSongLocked();
    }
    nativeSaveAutoplayEnabledLocked();
    nativeSaveAutoBlocoEnabledLocked();
    nativeBumpSharedRevisionLocked("director");
    g_nativeForceStateBuild.store(true);
    return true;
  }

  std::string specific = nativeJsonExtractString(commandBody, "desiredAutoBloco");
  if (specific.empty()) specific = nativeJsonExtractString(commandBody, "desiredAtBl");
  if (specific.empty()) specific = nativeJsonExtractString(commandBody, "desiredATBL");
  if (!specific.empty()) desired = specific;
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  const bool next = desired.empty() ? !g_nativeAutoBlocoEnabled : nativeBoolFromText(desired, g_nativeAutoBlocoEnabled);
  g_nativeAutoBlocoEnabled = next;
  if (!g_nativeAutoplayEnabled || !g_nativeAutoBlocoEnabled) nativeClearAutoBlocoTargetLocked();
  nativeSaveAutoBlocoEnabledLocked();
  nativeBumpSharedRevisionLocked("director");
  g_nativeForceStateBuild.store(true);
  return true;
}


static int nativeClampPreviewMode(int value)
{
  if (value < 1 || value > 3) return 0;
  return value;
}

static bool nativeApplyPreviewCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "preview_set" && type != "preview_toggle" && type != "preview_clear") return false;

  if (type == "preview_clear") {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativePreviewMode = 0;
    nativeSavePreviewModeLocked();
    g_nativeForceStateBuild.store(true);
    return true;
  }

  std::string slotValue = nativeJsonExtractString(commandBody, "previewIndex");
  if (slotValue.empty()) slotValue = nativeJsonExtractString(commandBody, "previewMode");
  if (slotValue.empty()) slotValue = nativeJsonExtractString(commandBody, "previewSlot");
  if (slotValue.empty()) slotValue = nativeJsonExtractString(commandBody, "slot");
  if (slotValue.empty()) slotValue = nativeJsonExtractString(commandBody, "index");
  const int slot = nativeClampPreviewMode(nativeLooksNumeric(slotValue) ? std::atoi(slotValue.c_str()) : 0);

  std::string desired = nativeJsonExtractString(commandBody, "desiredState");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "enabled");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "active");

  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (type == "preview_toggle") {
    g_nativePreviewMode = (slot > 0 && g_nativePreviewMode != slot) ? slot : 0;
  } else {
    const bool wantOn = desired.empty() ? (slot > 0) : nativeBoolFromText(desired, slot > 0);
    g_nativePreviewMode = (wantOn && slot > 0) ? slot : 0;
  }
  nativeSavePreviewModeLocked();
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeApplyLiveMarkCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (type == "live_reset_item" || type == "director_live_reset_item") {
    std::string id = nativeJsonExtractString(commandBody, "songId");
    if (id.empty()) id = nativeJsonExtractString(commandBody, "targetId");
    if (id.empty()) id = nativeJsonExtractString(commandBody, "id");
    const std::string startText = nativeJsonExtractString(commandBody, "startPos");
    const std::string endText = nativeJsonExtractString(commandBody, "endPos");
    const double start = nativeLooksNumeric(startText) ? std::atof(startText.c_str()) : 0.0;
    const double end = nativeLooksNumeric(endText) ? std::atof(endText.c_str()) : 0.0;
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (!id.empty()) g_nativeLiveExecutedItems.erase(id);
    if (end > start + 0.0005) {
      for (const auto& song : g_nativeSongWindows) {
        if (!song.isBlock && song.start >= start - 0.0005 && song.end <= end + 0.0005) g_nativeLiveExecutedItems.erase(song.id);
      }
    }
    g_nativeForceStateBuild.store(true);
    return true;
  }
  if (type != "live_set" && type != "live_toggle" && type != "director_live_set" && type != "director_live_toggle") return false;

  std::string desired = nativeJsonExtractString(commandBody, "enabled");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "live");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "desiredState");

  std::lock_guard<std::mutex> lock(g_nativeMutex);
  const bool next = (type == "live_toggle" || type == "director_live_toggle" || desired.empty())
    ? !g_nativeLiveMarkEnabled
    : nativeBoolFromText(desired, g_nativeLiveMarkEnabled);
  if (next != g_nativeLiveMarkEnabled) {
    g_nativeLiveMarkEnabled = next;
    g_nativeLiveExecutedItems.clear();
    nativeResetLiveTrackerLocked();
  }
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeApplyLoopCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "loop_toggle" && type != "loop_set" && type != "director_loop_toggle") return false;

  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return true;

  std::string desired = nativeJsonExtractString(commandBody, "desiredState");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "enabled");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "active");
  const bool current = nativeIsRepeatEnabled(project);
  const bool next = (type == "loop_toggle" || type == "director_loop_toggle" || desired.empty()) ? !current : nativeBoolFromText(desired, current);

  std::string startValue = nativeJsonExtractString(commandBody, "loopStartPos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "startPos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "start_pos");
  std::string endValue = nativeJsonExtractString(commandBody, "loopEndPos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "endPos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "end_pos");
  double startPos = nativeLooksNumeric(startValue) ? std::atof(startValue.c_str()) : -1.0;
  double endPos = nativeLooksNumeric(endValue) ? std::atof(endValue.c_str()) : -1.0;
  if (next && !(endPos > startPos && startPos >= 0.0)) {
    const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
    const double referencePos = (playState != 0 && GetPlayPositionEx_ptr)
      ? GetPlayPositionEx_ptr(project)
      : (GetCursorPositionEx_ptr ? GetCursorPositionEx_ptr(project) : 0.0);
    if (!nativeFindTransitionLoopRange(project, referencePos, startPos, endPos)) {
      nativeClearLoopTimeRange(project);
      nativeSetRepeatEnabled(project, false);
      g_nativeForceStateBuild.store(true);
      return true;
    }
  }
  if (next) nativeSetLoopTimeRange(project, startPos, endPos);
  else nativeClearLoopTimeRange(project);
  nativeSetRepeatEnabled(project, next);
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeApplyMultiLoopCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (!nativeStartsWith(type, "multiloop") && !nativeStartsWith(type, "multi_loop")) return false;
  nativeLoadMultiLoopState();
  if (type == "multiloop_focus" || type == "multiloop_open" || type == "multi_loop_focus" || type == "multi_loop_open") {
    std::string id = nativeJsonExtractString(commandBody, "songId");
    if (id.empty()) id = nativeJsonExtractString(commandBody, "targetId");
    std::string key = nativeJsonExtractString(commandBody, "songKey");
    if (key.empty()) key = nativeJsonExtractString(commandBody, "premixKey");
    std::string startText = nativeJsonExtractString(commandBody, "startPos");
    std::string endText = nativeJsonExtractString(commandBody, "endPos");
    const double start = nativeLooksNumeric(startText) ? std::atof(startText.c_str()) : 0.0;
    const double end = nativeLooksNumeric(endText) ? std::atof(endText.c_str()) : 0.0;
    // Canonicaliza inclusive clientes antigos que ainda enviem premixKey.
    // Multiloops e Lua persistem pelo numero estavel da regiao.
    for (const auto& song : g_nativeSongWindows) {
      if (song.isBlock) continue;
      if ((!id.empty() && song.id == id) || (end > start && std::fabs(song.start - start) < 0.002 && std::fabs(song.end - end) < 0.002)) {
        const std::string stableKey = nativeMultiLoopSongKey(song);
        if (!stableKey.empty()) key = stableKey;
        if (id.empty()) id = song.id;
        break;
      }
    }
    g_nativeMultiLoopFocusId = id;
    g_nativeMultiLoopFocusKey = key;
    g_nativeMultiLoopFocusName = nativeJsonExtractString(commandBody, "songName");
    g_nativeMultiLoopFocusStart = start;
    g_nativeMultiLoopFocusEnd = end;
    ++g_nativeMultiLoopPairCacheRevision;
    g_nativeForceStateBuild.store(true);
    return true;
  }
  if (type == "multiloop_close" || type == "multi_loop_close") {
    g_nativeMultiLoopFocusId.clear(); g_nativeMultiLoopFocusKey.clear(); g_nativeMultiLoopFocusName.clear();
    g_nativeMultiLoopFocusStart = 0.0; g_nativeMultiLoopFocusEnd = 0.0;
    g_nativeForceStateBuild.store(true);
    return true;
  }
  if (type == "multiloop_bypass_toggle" || type == "multi_loop_bypass_toggle" ||
      type == "multiloop_bypass_set" || type == "multi_loop_bypass_set") {
    if (type == "multiloop_bypass_toggle" || type == "multi_loop_bypass_toggle") {
      g_nativeMultiLoopBypassRequest.store(2);
    } else {
      std::string desiredText = nativeJsonExtractString(commandBody, "enabled");
      if (desiredText.empty()) desiredText = nativeJsonExtractString(commandBody, "desiredState");
      g_nativeMultiLoopBypassRequest.store(nativeBoolFromText(desiredText, false) ? 1 : 0);
    }
    g_nativeForceStateBuild.store(true);
    return true;
  }
  std::string key = nativeJsonExtractString(commandBody, "songKey");
  if (!g_nativeMultiLoopFocusKey.empty()) {
    key = g_nativeMultiLoopFocusKey;
  } else {
    // Compatibilidade com clientes ainda abertos usando premixKey
    // (region|12|...): converte para a mesma chave persistente do Lua.
    const size_t firstPipe = key.find('|');
    const size_t secondPipe = firstPipe == std::string::npos ? std::string::npos : key.find('|', firstPipe + 1);
    if (firstPipe != std::string::npos) {
      const std::string number = key.substr(firstPipe + 1, secondPipe == std::string::npos ? std::string::npos : secondPipe - firstPipe - 1);
      if (nativeLooksNumeric(number)) key = number;
    }
  }
  if (key.empty()) return true;
  int slot = 1;
  std::string slotText = nativeJsonExtractString(commandBody, "slot");
  if (nativeLooksNumeric(slotText)) slot = std::atoi(slotText.c_str());
  slot = std::max(1, std::min(2, slot)) - 1;
  NativeMultiLoopSongState& state = g_nativeMultiLoopStates[key];
  std::string desiredText = nativeJsonExtractString(commandBody, "enabled");
  if (desiredText.empty()) desiredText = nativeJsonExtractString(commandBody, "desiredState");
  bool changed = false;
  if (type == "multiloop_slot_toggle" || type == "multiloop_loop_toggle" || type == "multi_loop_slot_toggle" || type == "multiloop_slot_set") {
    const bool next = type == "multiloop_slot_set" && !desiredText.empty() ? nativeBoolFromText(desiredText, state.loop[slot]) : !state.loop[slot];
    if (next) {
      char path[2048] = ""; ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));
      const NativeMultiLoopPair pair = nativeFindMultiLoopPair(project, key, g_nativeMultiLoopFocusStart, g_nativeMultiLoopFocusEnd, slot);
      if (!pair.valid) { g_nativeForceStateBuild.store(true); return true; }
    }
    state.loop[slot] = next;
    if (!next) state.ms[slot] = false;
    if (!next && g_nativeMultiLoopActivePair.valid && g_nativeMultiLoopActivePair.songKey == key && g_nativeMultiLoopActivePair.slot == slot) {
      char path[2048] = ""; ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));
      nativeSetRepeatEnabled(project, false);
      nativeClearLoopTimeRange(project);
      for (const auto& row : g_nativeMultiLoopRestore) if (row.track && SetMediaTrackInfo_Value_ptr) {
        SetMediaTrackInfo_Value_ptr(row.track, "B_MUTE", row.mute);
        SetMediaTrackInfo_Value_ptr(row.track, "I_SOLO", row.solo);
      }
      g_nativeMultiLoopRestore.clear();
      g_nativeMultiLoopDisarmedPairKey = g_nativeMultiLoopActivePair.key;
      if (!g_nativeMultiLoopFadeTracks.empty()) {
        const double playPos = GetPlayPositionEx_ptr ? GetPlayPositionEx_ptr(project) : g_nativeMultiLoopActivePair.start;
        g_nativeMultiLoopFadeOutActive = false;
        g_nativeMultiLoopFadeInActive = true;
        g_nativeMultiLoopFadeStart = playPos;
        g_nativeMultiLoopFadeEnd = std::max(playPos + 0.08, g_nativeMultiLoopActivePair.end);
      }
      g_nativeMultiLoopActivePair = NativeMultiLoopPair();
      if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
      if (UpdateArrange_ptr) UpdateArrange_ptr();
    }
    changed = true;
  } else if (type == "multiloop_ms_toggle" || type == "multi_loop_ms_toggle" || type == "multiloop_ms_set") {
    if (state.loop[slot]) {
      const bool next = type == "multiloop_ms_set" && !desiredText.empty() ? nativeBoolFromText(desiredText, state.ms[slot]) : !state.ms[slot];
      state.ms[slot] = next; changed = true;
    }
  } else if (type == "multiloop_fade_adjust" || type == "multiloop_fade_set") {
    if (type == "multiloop_fade_set") {
      std::string value = nativeJsonExtractString(commandBody, "seconds");
      if (value.empty()) value = nativeJsonExtractString(commandBody, "durationSec");
      state.fadeSec[slot] = std::max(1, std::min(5, std::atoi(value.c_str())));
    } else {
      const std::string deltaText = nativeJsonExtractString(commandBody, "delta");
      state.fadeSec[slot] = std::max(1, std::min(5, state.fadeSec[slot] + std::atoi(deltaText.c_str())));
    }
    changed = true;
  } else if (type == "multiloop_track_toggle" || type == "multi_loop_track_toggle" ||
             type == "multiloop_track_set" || type == "multi_loop_track_set") {
    std::string guid = nativeJsonExtractString(commandBody, "trackId");
    if (guid.empty()) guid = nativeJsonExtractString(commandBody, "guid");
    const std::string mode = nativeLower(nativeJsonExtractString(commandBody, "mode"));
    if (!guid.empty() && (mode == "auto" || mode == "mute" || mode == "solo")) {
      NativeMultiLoopTrackPreset& row = state.tracks[slot][guid];
      const bool hasDesired = !desiredText.empty();
      if (mode == "auto") { row.autoFader = hasDesired ? nativeBoolFromText(desiredText, row.autoFader) : !row.autoFader; if (row.autoFader) row.mute = false; }
      if (mode == "mute") { row.mute = hasDesired ? nativeBoolFromText(desiredText, row.mute) : !row.mute; if (row.mute) { row.autoFader = false; row.solo = false; } }
      if (mode == "solo") { row.solo = hasDesired ? nativeBoolFromText(desiredText, row.solo) : !row.solo; if (row.solo) row.mute = false; }
      if (!row.autoFader && !row.mute && !row.solo && !row.fadeLimitSet) state.tracks[slot].erase(guid);
      changed = true;
    }
  } else if (type == "multiloop_track_limit_set" || type == "multi_loop_track_limit_set") {
    std::string guid = nativeJsonExtractString(commandBody, "trackId");
    if (guid.empty()) guid = nativeJsonExtractString(commandBody, "guid");
    std::string dbText = nativeJsonExtractString(commandBody, "limitDb");
    if (dbText.empty()) dbText = nativeJsonExtractString(commandBody, "reductionDb");
    if (!guid.empty() && !dbText.empty()) {
      NativeMultiLoopTrackPreset& row = state.tracks[slot][guid];
      row.fadeLimitSet = true;
      row.fadeLimitDb = std::max(-90.0, std::min(150.0, std::atof(dbText.c_str())));
      // Com o loop ativo, o slider funciona como monitoracao ao vivo do
      // volume minimo. Mantemos o volume original salvo para a restauracao.
      if (row.autoFader && g_nativeMultiLoopActivePair.valid &&
          g_nativeMultiLoopActivePair.songKey == key && g_nativeMultiLoopActivePair.slot == slot) {
        auto saved = std::find_if(g_nativeMultiLoopFadeTracks.begin(), g_nativeMultiLoopFadeTracks.end(),
          [&](const NativeMultiLoopTrackRestore& item) { return item.guid == guid; });
        if (saved != g_nativeMultiLoopFadeTracks.end()) {
          char path[2048] = "";
          ReaProject* project = getCurrentProject(path, static_cast<int>(sizeof(path)));
          MediaTrack* track = nativeFindTrackByGuid(project, guid);
          if (track && SetMediaTrackInfo_Value_ptr) {
            saved->track = track;
            saved->targetVolume = nativeMultiLoopFadeTargetVolume(saved->volume, row);
            SetMediaTrackInfo_Value_ptr(track, "D_VOL", saved->targetVolume);
            if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
            if (UpdateArrange_ptr) UpdateArrange_ptr();
          }
        }
      }
      changed = true;
    }
  }
  if (changed) nativeSaveMultiLoopState();
  g_nativeForceStateBuild.store(true);
  return true;
}

static MediaItem* nativeFindPremixMediaItemById(ReaProject* project, const std::string& rawId)
{
  const std::string id = nativeTrim(rawId);
  if (!project || id.empty() || !CountTracks_ptr || !GetTrack_ptr || !GetTrackNumMediaItems_ptr || !GetTrackMediaItem_ptr) return nullptr;
  const int trackCount = CountTracks_ptr(project);
  for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
    MediaTrack* track = GetTrack_ptr(project, trackIndex);
    if (!track) continue;
    const int itemCount = GetTrackNumMediaItems_ptr(track);
    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      MediaItem* item = GetTrackMediaItem_ptr(track, itemIndex);
      if (!item) continue;
      const std::string fallbackId = std::string("premix_item_") + std::to_string(trackIndex + 1) + "_" + std::to_string(itemIndex + 1);
      if (nativeMediaItemGuid(item, fallbackId) == id) return item;
    }
  }
  return nullptr;
}

static bool nativeApplyPremixCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (!nativeStartsWith(type, "premix")) return false;

  if (type == "premix_focus_song" || type == "premix_item_open" || type == "premix_open_song" || type == "premix_open") {
    std::string songId = nativeJsonExtractString(commandBody, "songId");
    if (songId.empty()) songId = nativeJsonExtractString(commandBody, "selectedRegionId");
    if (songId.empty()) songId = nativeJsonExtractString(commandBody, "targetId");
    if (songId.empty()) songId = nativeJsonExtractString(commandBody, "id");
    std::string startValue = nativeJsonExtractString(commandBody, "startPos");
    if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "selectedStartPos");
    if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "start_pos");
    std::string endValue = nativeJsonExtractString(commandBody, "endPos");
    if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "selectedEndPos");
    if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "end_pos");
    if (!songId.empty()) {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      g_nativePremixSelectedSongId = songId;
      g_nativePremixSelectedSongStart = nativeLooksNumeric(startValue) ? std::atof(startValue.c_str()) : 0.0;
      g_nativePremixSelectedSongEnd = nativeLooksNumeric(endValue) ? std::atof(endValue.c_str()) : 0.0;
    }
    g_nativeForceStateBuild.store(true);
    return true;
  }

  if (type == "premix_close" || type == "premix_screen_close") {
    g_nativeForceStateBuild.store(true);
    return true;
  }

  if (type != "premix_set_volume" && type != "premix_item_set_volume" &&
      type != "premix_toggle_mute" && type != "premix_item_toggle_mute") {
    // Mantém compatibilidade com comandos antigos de Premix ainda não usados nesta tela.
    g_nativeForceStateBuild.store(true);
    return true;
  }

  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return true;

  std::string targetId = nativeJsonExtractString(commandBody, "itemId");
  if (targetId.empty()) targetId = nativeJsonExtractString(commandBody, "mediaItemId");
  if (targetId.empty()) targetId = nativeJsonExtractString(commandBody, "targetId");
  if (targetId.empty()) targetId = nativeJsonExtractString(commandBody, "trackId");
  if (targetId.empty()) targetId = nativeJsonExtractString(commandBody, "guid");

  bool changed = false;
  MediaItem* mediaItem = nativeFindPremixMediaItemById(project, targetId);
  if (mediaItem && GetMediaItemInfo_Value_ptr && SetMediaItemInfo_Value_ptr) {
    if (type == "premix_set_volume" || type == "premix_item_set_volume") {
      std::string ratioText = nativeJsonExtractString(commandBody, "ratio");
      if (ratioText.empty()) ratioText = nativeJsonExtractString(commandBody, "volumeRatio");
      if (ratioText.empty()) ratioText = nativeJsonExtractString(commandBody, "value");
      const double ratio = ratioText.empty() ? 0.76 : std::atof(ratioText.c_str());
      changed = SetMediaItemInfo_Value_ptr(mediaItem, "D_VOL", nativePremixRatioToVolume(ratio));
    } else {
      const double current = GetMediaItemInfo_Value_ptr(mediaItem, "B_MUTE");
      std::string desired = nativeJsonExtractString(commandBody, "desiredMute");
      if (desired.empty()) desired = nativeJsonExtractString(commandBody, "muted");
      if (desired.empty()) desired = nativeJsonExtractString(commandBody, "enabled");
      const bool next = desired.empty() ? !(current > 0.5) : nativeBoolFromText(desired, current > 0.5);
      changed = SetMediaItemInfo_Value_ptr(mediaItem, "B_MUTE", next ? 1.0 : 0.0);
    }
  } else {
    // Compatibilidade com a tela Premix antiga baseada em tracks.
    MediaTrack* track = nativeFindTrackById(project, targetId);
    if (track && GetMediaTrackInfo_Value_ptr && SetMediaTrackInfo_Value_ptr) {
      if (type == "premix_set_volume" || type == "premix_item_set_volume") {
        std::string ratioText = nativeJsonExtractString(commandBody, "ratio");
        if (ratioText.empty()) ratioText = nativeJsonExtractString(commandBody, "volumeRatio");
        if (ratioText.empty()) ratioText = nativeJsonExtractString(commandBody, "value");
        changed = SetMediaTrackInfo_Value_ptr(track, "D_VOL", nativeRatioToVolume(ratioText.empty() ? 0.76 : std::atof(ratioText.c_str())));
      } else {
        const double current = GetMediaTrackInfo_Value_ptr(track, "B_MUTE");
        changed = SetMediaTrackInfo_Value_ptr(track, "B_MUTE", current > 0.5 ? 0.0 : 1.0);
      }
    }
  }

  if (changed && UpdateArrange_ptr) UpdateArrange_ptr();
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeExtractTimerSeconds(const std::string& commandBody, double& out)
{
  const char* keys[] = {
    "timerTargetSec", "targetSec", "timerCountdownStartSec", "countdownSec",
    "timerDisplaySec", "displaySec", "timerAccumulatedSec", "accumulatedSec",
    "seconds", "value"
  };
  for (const char* key : keys) {
    const std::string raw = nativeJsonExtractString(commandBody, key);
    if (nativeParseDouble(raw, out)) {
      if (out < 0.0) out = 0.0;
      return true;
    }
  }
  return false;
}

static void nativeTimerStartLocked()
{
  if (g_nativeTimerMode == "countdown" && g_nativeTimerResetToZero) {
    g_nativeTimerBaseSec = g_nativeTimerTargetSec;
  }
  g_nativeTimerResetToZero = false;
  g_nativeTimerRunning = true;
  g_nativeTimerStartedAtSteady = std::chrono::steady_clock::now();
  g_nativeTimerStartedAtSystem = std::chrono::system_clock::now();
}

static void nativeTimerStopLocked()
{
  g_nativeTimerBaseSec = nativeTimerDisplaySecLocked();
  g_nativeTimerResetToZero = false;
  g_nativeTimerRunning = false;
  g_nativeTimerStartedAtSteady = std::chrono::steady_clock::time_point();
  g_nativeTimerStartedAtSystem = std::chrono::system_clock::time_point();
}

static void nativeTimerResetLocked()
{
  g_nativeTimerRunning = false;
  g_nativeTimerStartedAtSteady = std::chrono::steady_clock::time_point();
  g_nativeTimerStartedAtSystem = std::chrono::system_clock::time_point();
  g_nativeTimerBaseSec = 0.0;
  g_nativeTimerResetToZero = true;
}

static bool nativeApplyTimerCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "timer_toggle" && type != "timer_start" && type != "timer_stop" &&
      type != "timer_stop_reset" && type != "timer_reset" && type != "timer_set_mode" &&
      type != "timer_config" && type != "timer_set_target") {
    return false;
  }

  std::string modeValue = nativeJsonExtractString(commandBody, "timerMode");
  if (modeValue.empty()) modeValue = nativeJsonExtractString(commandBody, "mode");
  if (modeValue.empty()) modeValue = nativeJsonExtractString(commandBody, "timerType");
  if (modeValue.empty()) modeValue = nativeJsonExtractString(commandBody, "typeValue");

  double seconds = 0.0;
  const bool hasSeconds = nativeExtractTimerSeconds(commandBody, seconds);

  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (!modeValue.empty()) {
    const std::string nextMode = nativeNormalizeTimerMode(modeValue);
    if (nextMode != g_nativeTimerMode) {
      const double currentDisplay = nativeTimerDisplaySecLocked();
      g_nativeTimerMode = nextMode;
      if (g_nativeTimerMode == "countdown") {
        g_nativeTimerBaseSec = hasSeconds ? seconds : (g_nativeTimerTargetSec > 0.0 ? g_nativeTimerTargetSec : currentDisplay);
        g_nativeTimerTargetSec = g_nativeTimerBaseSec;
        g_nativeTimerResetToZero = false;
      } else if (g_nativeTimerMode == "progressive") {
        g_nativeTimerBaseSec = 0.0;
        g_nativeTimerResetToZero = true;
      } else {
        g_nativeTimerBaseSec = 0.0;
        g_nativeTimerResetToZero = false;
      }
      if (g_nativeTimerRunning) {
        g_nativeTimerStartedAtSteady = std::chrono::steady_clock::now();
        g_nativeTimerStartedAtSystem = std::chrono::system_clock::now();
      }
    }
  }

  if (hasSeconds) {
    if (g_nativeTimerMode == "countdown") {
      g_nativeTimerTargetSec = seconds;
      if (!g_nativeTimerRunning || type == "timer_config" || type == "timer_set_target") {
        g_nativeTimerBaseSec = seconds;
        g_nativeTimerResetToZero = false;
        if (g_nativeTimerRunning) {
          g_nativeTimerStartedAtSteady = std::chrono::steady_clock::now();
          g_nativeTimerStartedAtSystem = std::chrono::system_clock::now();
        }
      }
    } else if (!g_nativeTimerRunning) {
      g_nativeTimerBaseSec = seconds;
      g_nativeTimerResetToZero = (seconds <= 0.0);
    }
  }

  if (type == "timer_start") {
    if (!g_nativeTimerRunning) nativeTimerStartLocked();
  } else if (type == "timer_stop") {
    if (g_nativeTimerRunning) nativeTimerStopLocked();
  } else if (type == "timer_toggle") {
    if (g_nativeTimerRunning) nativeTimerStopLocked();
    else nativeTimerStartLocked();
  } else if (type == "timer_reset" || type == "timer_stop_reset") {
    nativeTimerResetLocked();
  }

  nativePublishTimerStateLocked();
  g_nativeForceStateBuild.store(true);
  return true;
}


static bool nativeShouldMirrorCommandToLua(const std::string& commandType)
{
  return commandType == "autoplay_toggle" || commandType == "auto_toggle" || commandType == "director_auto_toggle" || commandType == "autoplay_set" || commandType == "auto_set" ||
         commandType == "auto_bloco_toggle" || commandType == "at_bl_toggle" || commandType == "atbl_toggle" || commandType == "director_at_bl_toggle" || commandType == "auto_bloco_set" || commandType == "at_bl_set" ||
         commandType == "auto_stop_toggle" || commandType == "autostop_toggle" || commandType == "director_auto_stop_toggle" || commandType == "auto_stop_set" || commandType == "autostop_set" || commandType == "set_auto_stop" || commandType == "set_autostop" || commandType == "toggle_auto_stop" || commandType == "toggle_autostop" || commandType == "auto_stop" || commandType == "autostop" ||
         commandType == "preview_set" || commandType == "preview_toggle" || commandType == "preview_clear" ||
         commandType == "multiloop_focus" || commandType == "multiloop_open" || commandType == "multiloop_close" || commandType == "multiloop_slot_set" || commandType == "multiloop_ms_set" || commandType == "multiloop_fade_adjust" || commandType == "multiloop_track_toggle" || commandType == "multiloop_track_set" || commandType == "multiloop_track_limit_set" || commandType == "multiloop_bypass_toggle" || commandType == "multiloop_bypass_set" ||
         commandType == "loop_toggle" || commandType == "loop_set" || commandType == "director_loop_toggle" ||
         commandType == "set_page" || commandType == "set_active_playlist";
}

static bool nativeApplyDirectorListScrollCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (type != "director_list_scroll" && type != "app_list_scroll") return false;

  std::string page = nativeLower(nativeJsonExtractString(commandBody, "page"));
  if (page.empty()) page = nativeLower(nativeJsonExtractString(commandBody, "activeTab"));
  if (page != "playlist" && page != "regions") return true;

  const std::string ratioText = nativeJsonExtractString(commandBody, "scrollRatio");
  double ratio = ratioText.empty() ? 0.0 : std::atof(ratioText.c_str());
  if (!std::isfinite(ratio)) ratio = 0.0;
  ratio = std::max(0.0, std::min(1.0, ratio));

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    double& savedRatio = page == "regions"
      ? g_nativeDirectorRegionsScrollRatio
      : g_nativeDirectorPlaylistScrollRatio;
    if (std::fabs(savedRatio - ratio) < 0.00005) return true;
    savedRatio = ratio;
    ++g_nativeDirectorListScrollRevision;
  }
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeApplyDirectorFamilyDrawersCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (type != "director_family_drawers_sync" && type != "director_drawers_sync") return false;

  const std::string packedIds = nativeJsonExtractString(commandBody, "openDrawerIds");
  std::map<std::string, bool> next;
  const std::vector<std::string> ids = nativeSplit(packedIds, '|');
  for (size_t i = 0; i < ids.size(); ++i) {
    const std::string id = nativeTrim(ids[i]);
    if (!id.empty()) next[id] = true;
  }

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (next == g_nativeDirectorOpenFamilyDrawers) return true;
    g_nativeDirectorOpenFamilyDrawers = std::move(next);
    g_nativeFamilyDrawersOwner = "director";
    ++g_nativeFamilyDrawersRevision;
    ++g_nativeDirectorListScrollRevision;
    nativeBumpSharedRevisionLocked("director");
  }
  g_nativeForceStateBuild.store(true);
  return true;
}

static void nativeMirrorCommandToLuaIfNeeded(const std::string& commandType, const std::string& commandBody)
{
  if (!nativeShouldMirrorCommandToLua(commandType)) return;
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  if (g_nativeCommandQueue.size() > 200) g_nativeCommandQueue.pop_front();
  g_nativeCommandQueue.push_back(commandBody);
  ++g_nativeCommandSequence;
  g_nativeForceStateBuild.store(true);
}

static bool nativeResolveProjectMarkerPosition(
  ReaProject* project,
  int markerNumber,
  int markerEnumIndex,
  double& positionOut)
{
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return false;

  auto readMarkerAtEnumIndex = [&](int enumIndex, bool requireNumberMatch) -> bool {
    if (enumIndex < 0) return false;
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* name = "";
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, enumIndex, &isRegion, &pos, &end, &name, &number, &color)) return false;
    if (isRegion) return false;
    if (requireNumberMatch && markerNumber > 0 && number != markerNumber) return false;
    positionOut = pos;
    return std::isfinite(positionOut);
  };

  // O índice enum identifica exatamente o marker usado para montar o cabeçalho.
  // Se a ordem do projeto mudou, o número do marker continua sendo o fallback estável.
  if (readMarkerAtEnumIndex(markerEnumIndex, true)) return true;

  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;
  if (markerNumber > 0) {
    for (int i = 0; i < total; ++i) {
      bool isRegion = false;
      double pos = 0.0;
      double end = 0.0;
      const char* name = "";
      int number = 0;
      int color = 0;
      if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &name, &number, &color)) continue;
      if (!isRegion && number == markerNumber) {
        positionOut = pos;
        return std::isfinite(positionOut);
      }
    }
  }

  // Último fallback: usa o índice sem exigir o número, caso o usuário tenha renumerado markers.
  return readMarkerAtEnumIndex(markerEnumIndex, false);
}


static void nativeProcessPendingSelectionOnMainThread()
{
  NativePendingSelectionCommand command;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (!g_nativePendingSelection.pending) return;
    command = g_nativePendingSelection;
    g_nativePendingSelection = NativePendingSelectionCommand{};
  }

  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return;

  const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
  const bool transportPlaying = ((playState & 1) == 1) || ((playState & 4) == 4);
  const bool cursorOnly = command.exactPosition && !command.seekToMarker && command.noSeek;
  double targetPos = command.startPos;
  bool hasTargetPos = command.hasExplicitStart;
  if (command.type == "edit_cursor_move" && command.hasBounds) {
    targetPos = std::max(command.minPos, std::min(command.maxPos, targetPos));
  }
  if (command.exactPosition && command.seekToMarker) {
    double markerPos = 0.0;
    if (nativeResolveProjectMarkerPosition(project, command.markerNumber, command.markerEnumIndex, markerPos)) {
      targetPos = markerPos;
      hasTargetPos = true;
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const NativeSongWindow* song = nativeFindSongForCommand(command.id, command.startPos, command.endPos);
    if (song) {
      g_nativeSelectedId = song->id;
      g_nativeSelectedTab = (command.type == "select_region") ? "regions" : "playlist";
      g_nativeSelectedStart = song->start;
      g_nativeSelectedEnd = song->end;
      if (!command.exactPosition) {
        targetPos = song->start;
        hasTargetPos = true;
      }
    } else if (!command.id.empty()) {
      g_nativeSelectedId = command.id;
      g_nativeSelectedTab = (command.type == "select_region") ? "regions" : "playlist";
      g_nativeSelectedStart = command.startPos;
      g_nativeSelectedEnd = command.endPos;
    }
  }

  // O modal e cursor-only: parado move somente o cursor de edicao; tocando ignora.
  // Toda chamada ao REAPER acontece aqui, na thread principal registrada pelo timer.
  if (!(cursorOnly && transportPlaying) && hasTargetPos && targetPos >= 0.0) {
    if (SetEditCurPos2_ptr) SetEditCurPos2_ptr(project, targetPos, true, false);
    else if (SetEditCurPos_ptr) SetEditCurPos_ptr(targetPos, true, false);
  }
  if (UpdateArrange_ptr) UpdateArrange_ptr();
  g_nativeForceStateBuild.store(true);
}

static bool nativeApplySelectionCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "edit_cursor_move" && type != "select_playlist_song" && type != "select_region" && type != "clear_selection" && type != "clear_selected_song" && type != "set_page") return false;

  if (type == "set_page") {
    std::string page = nativeJsonExtractString(commandBody, "page");
    if (page.empty()) page = nativeJsonExtractString(commandBody, "activeTab");
    page = nativeLower(nativeTrim(page));
    if (page == "playlist" || page == "regions") {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      g_nativeDirectorActivePage = page;
      ++g_nativeDirectorListScrollRevision;
      if (!g_nativeSelectedId.empty() && !g_nativeSelectedTab.empty() && g_nativeSelectedTab != page) {
        g_nativeSelectedId.clear();
        g_nativeSelectedTab.clear();
        g_nativeSelectedStart = 0.0;
        g_nativeSelectedEnd = 0.0;
      }
      g_nativeForceStateBuild.store(true);
    }
    return true;
  }

  if (type == "clear_selection" || type == "clear_selected_song") {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativePendingSelection = NativePendingSelectionCommand{};
    g_nativeSelectedId.clear();
    g_nativeSelectedTab.clear();
    g_nativeSelectedStart = 0.0;
    g_nativeSelectedEnd = 0.0;
    g_nativeForceStateBuild.store(true);
    return true;
  }

  NativePendingSelectionCommand command;
  command.pending = true;
  command.type = type;
  command.id = nativeJsonExtractString(commandBody, "targetId");
  if (command.id.empty()) command.id = nativeJsonExtractString(commandBody, "songId");
  if (command.id.empty()) command.id = nativeJsonExtractString(commandBody, "id");

  std::string startValue = nativeJsonExtractString(commandBody, "position");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "startPos");
  if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "start_pos");
  std::string endValue = nativeJsonExtractString(commandBody, "endPos");
  if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "end_pos");
  command.hasExplicitStart = nativeLooksNumeric(startValue);
  command.startPos = command.hasExplicitStart ? std::atof(startValue.c_str()) : 0.0;
  command.endPos = nativeLooksNumeric(endValue) ? std::atof(endValue.c_str()) : 0.0;

  const std::string minValue = nativeJsonExtractString(commandBody, "minPos");
  const std::string maxValue = nativeJsonExtractString(commandBody, "maxPos");
  command.hasBounds = nativeLooksNumeric(minValue) && nativeLooksNumeric(maxValue) && std::atof(maxValue.c_str()) >= std::atof(minValue.c_str());
  if (command.hasBounds) {
    command.minPos = std::atof(minValue.c_str());
    command.maxPos = std::atof(maxValue.c_str());
  }

  std::string exactPositionValue = nativeJsonExtractString(commandBody, "exactPosition");
  if (exactPositionValue.empty()) exactPositionValue = nativeJsonExtractString(commandBody, "useExplicitPosition");
  command.exactPosition = nativeBoolFromText(exactPositionValue, false);

  const std::string seekTargetValue = nativeLower(nativeTrim(nativeJsonExtractString(commandBody, "seekTarget")));
  command.seekToMarker = nativeBoolFromText(nativeJsonExtractString(commandBody, "seekToMarker"), false) || seekTargetValue == "marker";
  command.noSeek = nativeBoolFromText(nativeJsonExtractString(commandBody, "noSeek"), false);
  if (type == "edit_cursor_move") {
    const std::string seqValue = nativeJsonExtractString(commandBody, "cursorMoveSeq");
    const uint64_t seq = nativeLooksNumeric(seqValue) ? static_cast<uint64_t>(std::strtoull(seqValue.c_str(), nullptr, 10)) : 0;
    const uint64_t previousSeq = g_lastCursorMoveSeq.load();
    if (seq > 0 && seq < previousSeq) return true;
    if (seq > 0) g_lastCursorMoveSeq.store(seq);
    command.exactPosition = true;
    command.noSeek = true;
    command.seekToMarker = false;
  }

  std::string markerNumberValue = nativeJsonExtractString(commandBody, "markerNumber");
  if (markerNumberValue.empty()) markerNumberValue = nativeJsonExtractString(commandBody, "sourceNumber");
  command.markerNumber = nativeLooksNumeric(markerNumberValue) ? std::atoi(markerNumberValue.c_str()) : 0;
  const std::string markerEnumIndexValue = nativeJsonExtractString(commandBody, "markerEnumIndex");
  command.markerEnumIndex = nativeLooksNumeric(markerEnumIndexValue) ? std::atoi(markerEnumIndexValue.c_str()) : -1;

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    // Uma unica solicitacao pendente. O ultimo toque substitui o anterior.
    g_nativePendingSelection = command;
  }
  g_nativeForceStateBuild.store(true);
  return true;
}



static void nativeRestoreManualStopFadeoutTracksLocked()
{
  if (!SetMediaTrackInfo_Value_ptr) return;
  for (const auto& item : g_nativeManualStopFadeoutRuntime.tracks) {
    if (item.track) SetMediaTrackInfo_Value_ptr(item.track, "D_VOL", item.before);
  }
  if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);
  if (UpdateArrange_ptr) UpdateArrange_ptr();
}

static bool nativeTryStartManualStopFadeout(ReaProject* project, const std::string& pendingStopCommand)
{
  if (g_nativeManualStopFadeoutBypass || !project || !GetPlayStateEx_ptr || !CountTracks_ptr || !GetTrack_ptr ||
      !GetMediaTrackInfo_Value_ptr || !SetMediaTrackInfo_Value_ptr) return false;
  const int playState = GetPlayStateEx_ptr(project);
  const bool playing = ((playState & 1) == 1) || ((playState & 4) == 4);
  if (!playing || !nativeBoolFromText(nativeReadLuaWindowExtState(kManualStopFadeoutEnabledKey), false)) return false;

  std::lock_guard<std::mutex> lock(g_nativeManualStopFadeoutMutex);
  if (g_nativeManualStopFadeoutRuntime.active) {
    // Igual ao Lua: um segundo Stop enquanto o fade ainda esta descendo cancela a parada.
    nativeRestoreManualStopFadeoutTracksLocked();
    g_nativeManualStopFadeoutRuntime = NativeManualStopFadeoutRuntime{};
    g_nativeForceStateBuild.store(true);
    return true;
  }
  if (g_nativeManualStopFadeoutRuntime.restorePending) return true;

  const std::vector<std::string> selectedIds = nativeManualStopFadeoutTrackIds();
  if (selectedIds.empty()) return false;
  std::vector<NativeManualStopFadeoutTrack> selectedTracks;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;
    const std::string id = nativeTrackGuid(track, i);
    if (std::find(selectedIds.begin(), selectedIds.end(), id) == selectedIds.end()) continue;
    NativeManualStopFadeoutTrack item;
    item.track = track;
    item.before = GetMediaTrackInfo_Value_ptr(track, "D_VOL");
    selectedTracks.push_back(item);
  }
  if (selectedTracks.empty()) return false;

  g_nativeManualStopFadeoutRuntime = NativeManualStopFadeoutRuntime{};
  g_nativeManualStopFadeoutRuntime.active = true;
  g_nativeManualStopFadeoutRuntime.project = project;
  g_nativeManualStopFadeoutRuntime.startedAt = std::chrono::steady_clock::now();
  g_nativeManualStopFadeoutRuntime.durationSec = static_cast<double>(
    nativeManualStopFadeoutClampDuration(nativeReadLuaWindowExtState(kManualStopFadeoutDurationKey)));
  g_nativeManualStopFadeoutRuntime.pendingStopCommand = pendingStopCommand;
  g_nativeManualStopFadeoutRuntime.tracks = std::move(selectedTracks);
  g_nativeForceStateBuild.store(true);
  return true;
}

static void nativeBreakManualStopFadeout()
{
  std::lock_guard<std::mutex> lock(g_nativeManualStopFadeoutMutex);
  if (g_nativeManualStopFadeoutRuntime.active || g_nativeManualStopFadeoutRuntime.restorePending) {
    nativeRestoreManualStopFadeoutTracksLocked();
    g_nativeManualStopFadeoutRuntime = NativeManualStopFadeoutRuntime{};
    g_nativeForceStateBuild.store(true);
  }
}

static bool nativeApplyStopPauseModeCommand(const std::string& commandBody)
{
  const std::string type = nativeLower(nativeJsonExtractString(commandBody, "type"));
  if (type != "stop_pause_mode_set" && type != "stop_pause_mode_toggle" && type != "edit_mode_stop_pause_set") return false;

  nativeRefreshStopPauseModeFromExtState();
  bool enabled = !g_stopPauseModeEnabled.load();
  if (type != "stop_pause_mode_toggle") {
    std::string value = nativeJsonExtractString(commandBody, "enabled");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "stopPauseModeEnabled");
    if (value.empty()) value = nativeJsonExtractString(commandBody, "desiredState");
    enabled = nativeBoolFromText(value, enabled);
  }
  nativeWriteStopPauseModeEnabled(enabled);
  return true;
}

static bool nativeApplyTransportCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "play_start" && type != "play" && type != "play_stop" && type != "stop" &&
      type != "play_stop_no_seek" && type != "transport_stop_no_seek" &&
      type != "play_toggle" && type != "director_play_button" && type != "play_button" &&
      type != "director_play_no_seek" && type != "director_stop_no_seek" && type != "director_stop_break" && type != "stop_break" && type != "director_pause") {
    return false;
  }
  if (!Main_OnCommand_ptr) return false;

  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
  const bool isPlaying = ((playState & 1) == 1) || ((playState & 4) == 4);

  if (type == "director_pause") {
    if (isPlaying) Main_OnCommand_ptr(1008, 0); // Transport: Pause
    g_nativeForceStateBuild.store(true);
    return true;
  }

  std::string desired = nativeJsonExtractString(commandBody, "desiredPlaying");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "requestedPlaybackState");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "desiredState");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "forcePlay");
  if (desired.empty()) desired = nativeJsonExtractString(commandBody, "forceStop");
  desired = nativeTrim(desired);
  std::transform(desired.begin(), desired.end(), desired.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

  bool wantsPlay = false;
  bool wantsStop = false;
  if (type == "play_start" || type == "play" || type == "director_play_no_seek") wantsPlay = true;
  if (type == "play_stop" || type == "stop" || type == "director_stop_no_seek" || type == "director_stop_break" || type == "stop_break" ||
      type == "play_stop_no_seek" || type == "transport_stop_no_seek") wantsStop = true;
  if (desired == "true" || desired == "1" || desired == "play" || desired == "playing" || desired == "on") wantsPlay = true;
  if (desired == "false" || desired == "0" || desired == "stop" || desired == "stopped" || desired == "off") wantsStop = true;
  if (type == "play_toggle" || type == "director_play_button" || type == "play_button") {
    if (!wantsPlay && !wantsStop) {
      wantsPlay = !isPlaying;
      wantsStop = isPlaying;
    }
  }

  if (wantsStop && !wantsPlay) {
    // STOP BREAK pode chegar pelo atalho nativo ou pelo app. Além do tipo,
    // honra as flags do payload para que nenhuma normalização intermediária
    // transforme a parada imediata em Manual Stop/Faderout.
    const bool stopBreak = type == "director_stop_break" || type == "stop_break" ||
      nativeBoolFromText(nativeJsonExtractString(commandBody, "ignoreFadeout"), false) ||
      nativeBoolFromText(nativeJsonExtractString(commandBody, "stopBreak"), false);
    if (stopBreak) {
      if (!isPlaying) return true;
      nativeBreakManualStopFadeout();
    } else if (nativeTryStartManualStopFadeout(project, commandBody)) return true;
    std::string stopNoSeekValue = nativeJsonExtractString(commandBody, "noSeek");
    std::string stopTransportOnlyValue = nativeJsonExtractString(commandBody, "transportOnly");
    std::string stopPreserveCursorValue = nativeJsonExtractString(commandBody, "preserveCursor");
    std::string stopNoPositionValue = nativeJsonExtractString(commandBody, "noPosition");
    std::string stopPreventFallbackZeroValue = nativeJsonExtractString(commandBody, "preventFallbackZero");
    const bool stopNoSeek = (type == "director_stop_no_seek" || type == "director_stop_break" || type == "stop_break" || type == "play_stop_no_seek" || type == "transport_stop_no_seek" ||
      nativeBoolFromText(stopNoSeekValue, false) || nativeBoolFromText(stopTransportOnlyValue, false) ||
      nativeBoolFromText(stopPreserveCursorValue, false) || nativeBoolFromText(stopNoPositionValue, false) ||
      nativeBoolFromText(stopPreventFallbackZeroValue, false));
    if (stopNoSeek) {
      std::string explicitStopId = nativeJsonExtractString(commandBody, "stopSelectionTargetId");
      if (explicitStopId.empty()) explicitStopId = nativeJsonExtractString(commandBody, "queuedSelectionId");
      if (explicitStopId.empty()) explicitStopId = nativeJsonExtractString(commandBody, "nextSelectionId");
      if (explicitStopId.empty()) explicitStopId = nativeJsonExtractString(commandBody, "targetId");
      if (explicitStopId.empty()) explicitStopId = nativeJsonExtractString(commandBody, "songId");
      if (explicitStopId.empty()) explicitStopId = nativeJsonExtractString(commandBody, "selectedPlaylistSongId");
      if (explicitStopId.empty()) explicitStopId = nativeJsonExtractString(commandBody, "selectedRegionId");
      std::string explicitStopTab = nativeJsonExtractString(commandBody, "stopSelectionTargetTab");
      if (explicitStopTab.empty()) explicitStopTab = nativeJsonExtractString(commandBody, "activeTab");
      if (explicitStopTab.empty()) explicitStopTab = nativeJsonExtractString(commandBody, "page");
      if (explicitStopTab != "regions") explicitStopTab = "playlist";
      std::string explicitStartValue = nativeJsonExtractString(commandBody, "stopSelectionStartPos");
      if (explicitStartValue.empty()) explicitStartValue = nativeJsonExtractString(commandBody, "selectedStartPos");
      if (explicitStartValue.empty()) explicitStartValue = nativeJsonExtractString(commandBody, "startPos");
      std::string explicitEndValue = nativeJsonExtractString(commandBody, "stopSelectionEndPos");
      if (explicitEndValue.empty()) explicitEndValue = nativeJsonExtractString(commandBody, "selectedEndPos");
      if (explicitEndValue.empty()) explicitEndValue = nativeJsonExtractString(commandBody, "endPos");
      const double explicitStartPos = nativeLooksNumeric(explicitStartValue) ? std::atof(explicitStartValue.c_str()) : 0.0;
      const double explicitEndPos = nativeLooksNumeric(explicitEndValue) ? std::atof(explicitEndValue.c_str()) : 0.0;
      g_transportCommandBypass = true;
      const bool ok = nativeStopTransportAndPrepareExplicitSelection(project, explicitStopId, explicitStopTab, explicitStartPos, explicitEndPos, true);
      g_transportCommandBypass = false;
      if (stopBreak && ok) {
        // O Stop Break termina primeiro o transporte e so depois desarma o loop.
        // Assim o timer nativo do Multiloops nao consegue rearma-lo no intervalo
        // em que o REAPER ainda aparecia como tocando.
        nativeSetRepeatEnabled(project, false);
        nativeClearLoopTimeRange(project);
        nativeRestoreMultiLoopTracks();
        nativeRestoreMultiLoopBypassVolumes(true);
        g_nativeMultiLoopActivePair = NativeMultiLoopPair();
        g_nativeMultiLoopDisarmedPairKey.clear();
        g_nativeMultiLoopBypassActive = false;
        g_nativeMultiLoopBypassSongKey.clear();
        g_nativeMultiLoopBypassRequest.store(-1);
      }
      return ok;
    }
    const double editCursorBeforeStop = (stopNoSeek && GetCursorPositionEx_ptr) ? GetCursorPositionEx_ptr(project) : -1.0;

    // FIX102: Stop no-seek vindo do Diretor/Bridge é transporte puro.
    // A extensão não pode transformar ausência de alvo em id do comando nem em posição 0.
    // O comando "id" do envelope HTTP é UUID do comando, não id de música.
    // Também preserva o cursor de edição ao redor do Main_OnCommand(1016), porque o Stop
    // do REAPER pode voltar o cursor para o início da reprodução dependendo das prefs.
    // FIX77: Stop no Diretor precisa deixar a seleção pronta para o próximo Play.
    // Se existe fila manual/auto, a seleção nativa vai para a música da fila antes
    // de limpar a fila. Sem fila, preserva a música parada como seleção.
    std::string stopSelectionId = nativeJsonExtractString(commandBody, "stopSelectionTargetId");
    if (stopSelectionId.empty()) stopSelectionId = nativeJsonExtractString(commandBody, "queuedSelectionId");
    if (stopSelectionId.empty()) stopSelectionId = nativeJsonExtractString(commandBody, "nextSelectionId");
    std::string stopSelectionTab = nativeJsonExtractString(commandBody, "stopSelectionTargetTab");
    if (stopSelectionTab.empty()) stopSelectionTab = nativeJsonExtractString(commandBody, "activeTab");
    std::string stopStartValue = nativeJsonExtractString(commandBody, "stopSelectionStartPos");
    if (stopStartValue.empty()) stopStartValue = nativeJsonExtractString(commandBody, "selectedStartPos");
    std::string stopEndValue = nativeJsonExtractString(commandBody, "stopSelectionEndPos");
    if (stopEndValue.empty()) stopEndValue = nativeJsonExtractString(commandBody, "selectedEndPos");
    double stopStartPos = nativeLooksNumeric(stopStartValue) ? std::atof(stopStartValue.c_str()) : 0.0;
    double stopEndPos = nativeLooksNumeric(stopEndValue) ? std::atof(stopEndValue.c_str()) : 0.0;

    // FIX110: Stop usa a fila como destino antes de limpar.
    // Se havia fila manual ou Auto, ela vira a selecao/cursor pronto para Play.
    // Só fica na musica atual quando nao havia nada aguardando.
    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      if (stopSelectionId.empty()) {
        std::string snapId;
        std::string snapTab;
        double snapStart = 0.0;
        double snapEnd = 0.0;
        if (nativeSnapshotStopTargetLocked(snapId, snapTab, snapStart, snapEnd)) {
          stopSelectionId = snapId;
          stopSelectionTab = snapTab;
          stopStartPos = snapStart;
          stopEndPos = snapEnd;
        }
      }
    }
    if (stopSelectionId.empty() && !stopNoSeek) {
      stopSelectionId = nativeJsonExtractString(commandBody, "targetId");
      if (stopSelectionId.empty()) stopSelectionId = nativeJsonExtractString(commandBody, "songId");
      if (stopSelectionId.empty()) stopSelectionId = nativeJsonExtractString(commandBody, "selectedPlaylistSongId");
      if (stopSelectionId.empty()) stopSelectionId = nativeJsonExtractString(commandBody, "selectedRegionId");
      // Não usar "id" aqui: em comando nativo ele é o id do comando, não da música.
    }
    if (stopSelectionTab != "regions") stopSelectionTab = "playlist";

    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      nativeCancelQueueHandoffProtectionLocked();
    }

    Main_OnCommand_ptr(1016, 0); // Transport: Stop
    if (stopNoSeek && SetEditCurPos2_ptr && editCursorBeforeStop >= 0.0) {
      SetEditCurPos2_ptr(project, editCursorBeforeStop, false, false);
    }

    double finalSelectionStart = stopStartPos;
    double finalSelectionEnd = stopEndPos;
    bool hasFinalSelection = false;
    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      if (!stopSelectionId.empty()) {
        const NativeSongWindow* selectedSong = nativeFindSongForCommand(stopSelectionId, stopStartPos, stopEndPos);
        if (selectedSong) {
          g_nativeSelectedId = selectedSong->id;
          g_nativeSelectedTab = stopSelectionTab;
          g_nativeSelectedStart = selectedSong->start;
          g_nativeSelectedEnd = selectedSong->end;
          finalSelectionStart = selectedSong->start;
          finalSelectionEnd = selectedSong->end;
          hasFinalSelection = true;
        } else if (!stopNoSeek || stopStartPos > 0.0 || stopEndPos > 0.0) {
          g_nativeSelectedId = stopSelectionId;
          g_nativeSelectedTab = stopSelectionTab;
          g_nativeSelectedStart = stopStartPos;
          g_nativeSelectedEnd = stopEndPos;
          finalSelectionStart = stopStartPos;
          finalSelectionEnd = stopEndPos;
          hasFinalSelection = true;
        }
      }
      nativeClearAllQueueStateLocked();
    }

    if (!stopNoSeek && hasFinalSelection && SetEditCurPos2_ptr && finalSelectionStart >= 0.0) {
      SetEditCurPos2_ptr(project, finalSelectionStart, true, false);
    }
    if (UpdateArrange_ptr) UpdateArrange_ptr();
    g_nativeForceStateBuild.store(true);
    return true;
  }

  if (wantsPlay && !wantsStop) {
    std::string noSeekValue = nativeJsonExtractString(commandBody, "noSeek");
    std::string transportOnlyValue = nativeJsonExtractString(commandBody, "transportOnly");
    std::transform(noSeekValue.begin(), noSeekValue.end(), noSeekValue.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    std::transform(transportOnlyValue.begin(), transportOnlyValue.end(), transportOnlyValue.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    const bool noSeek = (noSeekValue == "true" || noSeekValue == "1" || transportOnlyValue == "true" || transportOnlyValue == "1" || type == "director_play_no_seek");
    std::string exactPositionValue = nativeJsonExtractString(commandBody, "exactPosition");
    if (exactPositionValue.empty()) exactPositionValue = nativeJsonExtractString(commandBody, "useExplicitPosition");
    const bool exactPosition = nativeBoolFromText(exactPositionValue, false);
    std::string seekToMarkerValue = nativeJsonExtractString(commandBody, "seekToMarker");
    const std::string seekTargetValue = nativeLower(nativeTrim(nativeJsonExtractString(commandBody, "seekTarget")));
    const bool seekToMarker = nativeBoolFromText(seekToMarkerValue, false) || seekTargetValue == "marker";
    std::string markerNumberValue = nativeJsonExtractString(commandBody, "markerNumber");
    if (markerNumberValue.empty()) markerNumberValue = nativeJsonExtractString(commandBody, "sourceNumber");
    const int markerNumber = nativeLooksNumeric(markerNumberValue) ? std::atoi(markerNumberValue.c_str()) : 0;
    const std::string markerEnumIndexValue = nativeJsonExtractString(commandBody, "markerEnumIndex");
    const int markerEnumIndex = nativeLooksNumeric(markerEnumIndexValue) ? std::atoi(markerEnumIndexValue.c_str()) : -1;
    std::string idValue = nativeJsonExtractString(commandBody, "targetId");
    if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "songId");
    if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "selectedPlaylistSongId");
    if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "selectedRegionId");
    if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "id");
    std::string startValue = nativeJsonExtractString(commandBody, "startPos");
    if (startValue.empty()) startValue = nativeJsonExtractString(commandBody, "start_pos");
    std::string endValue = nativeJsonExtractString(commandBody, "endPos");
    if (endValue.empty()) endValue = nativeJsonExtractString(commandBody, "end_pos");
    const bool hasExplicitStart = nativeLooksNumeric(startValue);
    double startPos = hasExplicitStart ? std::atof(startValue.c_str()) : 0.0;
    const double endPos = nativeLooksNumeric(endValue) ? std::atof(endValue.c_str()) : 0.0;
    bool hasResolvedStart = hasExplicitStart;
    if (exactPosition && seekToMarker) {
      double markerPos = 0.0;
      if (nativeResolveProjectMarkerPosition(project, markerNumber, markerEnumIndex, markerPos)) {
        startPos = markerPos;
        hasResolvedStart = true;
      }
    }

    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      const NativeSongWindow* song = exactPosition ? nullptr : nativeFindSongForCommand(idValue, startPos, endPos);
      if (song) {
        startPos = song->start;
        g_nativeSelectedId = song->id;
        g_nativeSelectedStart = song->start;
        g_nativeSelectedEnd = song->end;
      } else if (!idValue.empty()) {
        g_nativeSelectedId = idValue;
        g_nativeSelectedStart = startPos;
        g_nativeSelectedEnd = endPos;
      }
    }

    int playPlaylistOrder = 0;
    if (!exactPosition) {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      const NativeSongWindow* song = nativeFindSongForCommand(idValue, startPos, endPos);
      if (song) playPlaylistOrder = song->playlistOrder;
    }
    if (playPlaylistOrder > 0) {
      VS_Hook_Native_ArmAutoQueueFromPlaylistOrder(playPlaylistOrder);
    }

    if (!noSeek && SetEditCurPos2_ptr && project && hasResolvedStart && startPos >= 0.0) {
      SetEditCurPos2_ptr(project, startPos, true, false);
    }
    Main_OnCommand_ptr(1007, 0); // Transport: Play
    if (UpdateArrange_ptr) UpdateArrange_ptr();
    g_nativeForceStateBuild.store(true);
    return true;
  }

  return false;
}


static bool nativeParseMarkerNumberFromId(const std::string& rawId, int& numberOut)
{
  std::string id = nativeTrim(rawId);
  if (id.empty()) return false;
  if ((id[0] == 'm' || id[0] == 'M') && id.size() > 1) id = id.substr(1);
  if (!nativeLooksNumeric(id)) return false;
  numberOut = std::atoi(id.c_str());
  return numberOut != 0;
}

static bool nativeFindMarkerByCommandId(ReaProject* project, const std::string& rawId, double& posOut, std::string& idOut)
{
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return false;
  int wantedNumber = 0;
  const bool hasWantedNumber = nativeParseMarkerNumberFromId(rawId, wantedNumber);
  const std::string wantedId = nativeTrim(rawId);
  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;
  double bestPos = 0.0;
  std::string bestId;
  bool found = false;
  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* name = nullptr;
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &name, &number, &color)) continue;
    if (isRegion) continue;
    const std::string markerId = std::string("m") + std::to_string(number);
    if ((hasWantedNumber && number == wantedNumber) || (!wantedId.empty() && markerId == wantedId)) {
      bestPos = pos;
      bestId = markerId;
      found = true;
      break;
    }
  }
  if (!found) return false;
  posOut = bestPos;
  idOut = bestId;
  return true;
}

static void nativeUpdateManualStopFadeoutOnMainThread()
{
  std::string stopCommand;
  bool finishNow = false;
  {
    std::lock_guard<std::mutex> lock(g_nativeManualStopFadeoutMutex);
    NativeManualStopFadeoutRuntime& runtime = g_nativeManualStopFadeoutRuntime;
    if (!runtime.active && !runtime.restorePending) return;
    const auto now = std::chrono::steady_clock::now();

    if (runtime.restorePending) {
      if (now >= runtime.restoreAt) {
        nativeRestoreManualStopFadeoutTracksLocked();
        runtime = NativeManualStopFadeoutRuntime{};
        g_nativeForceStateBuild.store(true);
      }
      return;
    }

    const int playState = (GetPlayStateEx_ptr && runtime.project) ? GetPlayStateEx_ptr(runtime.project) : 0;
    const bool playing = ((playState & 1) == 1) || ((playState & 4) == 4);
    if (!playing) {
      runtime.active = false;
      runtime.restorePending = true;
      runtime.restoreAt = now + std::chrono::milliseconds(400);
      return;
    }

    const double duration = std::max(0.05, runtime.durationSec);
    const double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - runtime.startedAt).count() / 1000.0;
    const double ratio = std::max(0.0, std::min(1.0, elapsed / duration));
    runtime.progress = ratio;
    const double remain = 1.0 - ratio;
    if (SetMediaTrackInfo_Value_ptr) {
      for (const auto& item : runtime.tracks) {
        if (item.track) SetMediaTrackInfo_Value_ptr(item.track, "D_VOL", std::max(0.0, item.before * remain));
      }
    }
    if (TrackList_AdjustWindows_ptr) TrackList_AdjustWindows_ptr(false);

    if (ratio >= 1.0) {
      stopCommand = runtime.pendingStopCommand;
      runtime.active = false;
      runtime.restorePending = true;
      runtime.restoreAt = now + std::chrono::milliseconds(400);
      finishNow = true;
    }
  }

  if (finishNow && !stopCommand.empty()) {
    g_nativeManualStopFadeoutBypass = true;
    nativeApplyTransportCommand(stopCommand);
    g_nativeManualStopFadeoutBypass = false;
    g_nativeForceStateBuild.store(true);
  }
}

static void nativeProcessGlobalStopBreakOnMainThread()
{
  if (!g_globalStopBreakRequested.exchange(false)) return;
  if (!nativeIsRuntimeControlActive()) return;
  nativeApplyTransportCommand(
    "{\"type\":\"director_stop_break\",\"payload\":{\"noSeek\":true,\"preserveCursor\":true,\"transportOnly\":true,\"ignoreFadeout\":true,\"stopBreak\":true}}"
  );
}

static void nativeProcessGlobalPauseOnMainThread()
{
  if (!g_globalPauseRequested.exchange(false)) return;
  if (!nativeIsRuntimeControlActive() || !Main_OnCommand_ptr) return;
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
  // Esta rotina pertence somente ao Alt/Option+Espaco. O Modo Stop/Pause tem
  // sua rotina separada abaixo e nunca passa pela logica do Lua.
  if (playState != 0) {
    Main_OnCommand_ptr(1008, 0);
    g_nativeForceStateBuild.store(true);
  }
}

static void nativeProcessGlobalStopPauseOnMainThread()
{
  if (!g_globalStopPauseRequested.exchange(false)) return;
  if (!nativeIsRuntimeControlActive() || !Main_OnCommand_ptr) return;
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;

  // A extensao e a unica dona da acao. O Lua apenas publica se o modo esta ON.
  // Parado inicia Play; tocando ou pausado alterna Pause/Continuar sem Stop.
  Main_OnCommand_ptr(playState == 0 ? 1007 : 1008, 0);
  g_nativeForceStateBuild.store(true);
}

static bool nativeFindNextMarkerAfterPlayCursor(ReaProject* project, double& posOut, std::string& idOut)
{
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return false;
  const double playPos = GetPlayPositionEx_ptr ? GetPlayPositionEx_ptr(project) : 0.0;
  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;
  bool found = false;
  double bestPos = 0.0;
  int bestNumber = 0;
  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* name = nullptr;
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &name, &number, &color)) continue;
    if (isRegion) continue;
    if (pos < playPos - 0.0005) continue;
    if (!found || pos < bestPos) {
      found = true;
      bestPos = pos;
      bestNumber = number;
    }
  }
  if (!found) return false;
  posOut = bestPos;
  idOut = std::string("m") + std::to_string(bestNumber);
  return true;
}

static bool nativeIsPartsMarkerName(const std::string& markerName)
{
  const std::string name = nativeTrim(markerName);
  return name.rfind("$", 0) == 0 || name.rfind("*1", 0) == 0 || name.rfind("*2", 0) == 0;
}

static bool nativeFindNextPartsMarkerInWindow(ReaProject* project, double playPos, double windowStart, double windowEnd, double& posOut, std::string& idOut)
{
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return false;
  if (windowEnd <= windowStart + 0.0005) return false;
  int markerCount = 0;
  int regionCount = 0;
  int total = CountProjectMarkers_ptr(project, &markerCount, &regionCount);
  if (total <= 0) total = markerCount + regionCount;

  bool found = false;
  double bestPos = 0.0;
  int bestNumber = 0;
  for (int i = 0; i < total; ++i) {
    bool isRegion = false;
    double pos = 0.0;
    double end = 0.0;
    const char* name = nullptr;
    int number = 0;
    int color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &isRegion, &pos, &end, &name, &number, &color)) continue;
    if (isRegion) continue;
    if (!nativeIsPartsMarkerName(name ? name : "")) continue;
    if (pos <= playPos + 0.0005) continue;
    if (pos < windowStart - 0.0005 || pos >= windowEnd - 0.0005) continue;
    if (!found || pos < bestPos) {
      found = true;
      bestPos = pos;
      bestNumber = number;
    }
  }
  if (!found) return false;
  posOut = bestPos;
  idOut = std::string("m") + std::to_string(bestNumber);
  return true;
}


static void nativeMoveEditCursorAndSeek(ReaProject* project, double pos, bool seekPlayback)
{
  if (!SetEditCurPos2_ptr || pos < 0.0) return;
  // Primeiro move de fato o cursor de edição. Em alguns setups, chamar apenas com seek=true
  // faz o playback buscar, mas a linha visual do cursor de edição não acompanha.
  SetEditCurPos2_ptr(project, pos, true, false);
  if (seekPlayback) {
    SetEditCurPos2_ptr(project, pos, true, true);
  }
  if (UpdateArrange_ptr) UpdateArrange_ptr();
}

static bool nativeApplyMarkerCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "marker_select" && type != "select_marker" && type != "marker_go" && type != "trigger_marker" &&
      type != "marker_cancel" && type != "escape_key" && type != "esc" && type != "key_escape") {
    return false;
  }
  if (!EnumProjects_ptr) return false;
  char pathBuf[2048] = "";
  ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
  if (!project) return false;

  if (type == "marker_cancel" || type == "escape_key" || type == "esc" || type == "key_escape") {
    const bool clearOnly = nativeBoolFromText(nativeJsonExtractString(commandBody, "clearOnly"), false) ||
      nativeBoolFromText(nativeJsonExtractString(commandBody, "stopCleanup"), false) ||
      nativeBoolFromText(nativeJsonExtractString(commandBody, "noSeek"), false) ||
      nativeBoolFromText(nativeJsonExtractString(commandBody, "preserveCursor"), false);
    if (clearOnly) {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      g_nativeArmedMarkerId.clear();
      g_nativeSelectedMarkerId.clear();
      g_nativeSelectedMarkerPos = 0.0;
      g_nativeArmedMarkerSetAt = std::chrono::steady_clock::time_point();
      g_nativeArmedMarkerStartPlayPos = 0.0;
      g_nativeForceStateBuild.store(true);
      return true;
    }

    double targetPos = -1.0;
    std::string targetMarkerId;
    const double playPos = GetPlayPositionEx_ptr ? GetPlayPositionEx_ptr(project) : 0.0;
    std::string markersJson;
    std::vector<NativeSongWindow> songs = nativeCollectProjectSongs(project, markersJson);
    std::string playingName;
    double songStart = 0.0;
    double songEnd = 0.0;
    std::string playingId = nativeFindPlayingId(songs, playPos, playingName, songStart, songEnd);
    if (playingId.empty()) {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      songStart = g_nativeCurrentSongStart;
      songEnd = g_nativeCurrentSongEnd;
    }
    if (songEnd > songStart + 0.0005) {
      if (nativeFindNextPartsMarkerInWindow(project, playPos, songStart, songEnd, targetPos, targetMarkerId)) {
        // Usa o proximo marker de Parts que ainda esta a frente do play cursor.
      } else {
        targetPos = songEnd;
      }
    } else {
      nativeFindNextMarkerAfterPlayCursor(project, targetPos, targetMarkerId);
    }
    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      g_nativeArmedMarkerId.clear();
      g_nativeSelectedMarkerId.clear();
      g_nativeSelectedMarkerPos = 0.0;
      g_nativeArmedMarkerSetAt = std::chrono::steady_clock::time_point();
      g_nativeArmedMarkerStartPlayPos = 0.0;
    }
    if (targetPos >= 0.0) nativeMoveEditCursorAndSeek(project, targetPos, true);
    else if (UpdateArrange_ptr) UpdateArrange_ptr();
    g_nativeForceStateBuild.store(true);
    return true;
  }

  std::string idValue = nativeJsonExtractString(commandBody, "markerId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "selectedMarkerId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "targetId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "id");
  double markerPos = 0.0;
  std::string markerId;
  if (!nativeFindMarkerByCommandId(project, idValue, markerPos, markerId)) return false;

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeSelectedMarkerId = markerId;
    g_nativeSelectedMarkerPos = markerPos;
    if (type == "marker_go" || type == "trigger_marker") {
      const int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
      const bool playingNow = (playState & 1) == 1 || (playState & 4) == 4;
      if (playingNow) {
        g_nativeArmedMarkerId = markerId;
        g_nativeArmedMarkerSetAt = std::chrono::steady_clock::now();
        g_nativeArmedMarkerStartPlayPos = GetPlayPositionEx_ptr ? GetPlayPositionEx_ptr(project) : 0.0;
      } else {
        g_nativeArmedMarkerId.clear();
        g_nativeArmedMarkerSetAt = std::chrono::steady_clock::time_point();
        g_nativeArmedMarkerStartPlayPos = 0.0;
      }
    }
    if (type == "marker_select" || type == "select_marker") { g_nativeArmedMarkerId.clear(); g_nativeArmedMarkerSetAt = std::chrono::steady_clock::time_point(); g_nativeArmedMarkerStartPlayPos = 0.0; }
  }

  // FIX47: primeiro toque no App Diretor apenas seleciona o marker (amarelo).
  // Só marker_go/trigger_marker confirma o engatilhamento e move o cursor de edição.
  if (type == "marker_go" || type == "trigger_marker") {
    nativeMoveEditCursorAndSeek(project, markerPos, true);
  } else if (UpdateArrange_ptr) {
    UpdateArrange_ptr();
  }
  g_nativeForceStateBuild.store(true);
  return true;
}

static bool nativeSelectProjectFromCommand(const std::string& commandBody)
{
  const std::string type = nativeJsonExtractString(commandBody, "type");
  if (type != "set_project_tab" && type != "select_project_tab" && type != "switch_project_tab" && type != "project_tab_select") return false;
  if (!EnumProjects_ptr || !SelectProjectInstance_ptr) return false;
  std::string idxValue = nativeJsonExtractString(commandBody, "projectTabIndex");
  if (idxValue.empty()) idxValue = nativeJsonExtractString(commandBody, "tabIndex");
  if (idxValue.empty()) idxValue = nativeJsonExtractString(commandBody, "projectIndex");
  if (idxValue.empty()) idxValue = nativeJsonExtractString(commandBody, "index");
  std::string idValue = nativeJsonExtractString(commandBody, "projectId");
  if (idValue.empty()) idValue = nativeJsonExtractString(commandBody, "id");
  std::string nameValue = nativeJsonExtractString(commandBody, "projectName");
  std::string pathValue = normalizeSlashes(nativeJsonExtractString(commandBody, "projectPath"));

  ReaProject* target = nullptr;
  if (!idxValue.empty() && nativeLooksNumeric(idxValue)) {
    char pathBuf[2048] = "";
    target = EnumProjects_ptr(std::atoi(idxValue.c_str()), pathBuf, static_cast<int>(sizeof(pathBuf)));
  }
  if (!target) {
    for (int i = 0; i < 64; ++i) {
      char pathBuf[2048] = "";
      ReaProject* p = EnumProjects_ptr(i, pathBuf, static_cast<int>(sizeof(pathBuf)));
      if (!p) break;
      const std::string pid = std::to_string(reinterpret_cast<std::uintptr_t>(p));
      const std::string ppath = normalizeSlashes(pathBuf);
      const std::string pname = nativeBasenameNoRpp(ppath);
      if ((!idValue.empty() && idValue == pid) || (!pathValue.empty() && pathValue == ppath) || (!nameValue.empty() && nameValue == pname)) {
        target = p;
        break;
      }
    }
  }
  if (!target) return false;
  SelectProjectInstance_ptr(target);
  g_nativeForceStateBuild.store(true);
  return true;
}


static std::string nativeRequestTarget(const std::string& req)
{
  const size_t sp1 = req.find(' ');
  if (sp1 == std::string::npos) return "/";
  const size_t sp2 = req.find(' ', sp1 + 1);
  if (sp2 == std::string::npos) return "/";
  return req.substr(sp1 + 1, sp2 - sp1 - 1);
}

static int nativeHexValue(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static std::string nativeUrlDecode(const std::string& value)
{
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (c == '%' && i + 2 < value.size()) {
      const int hi = nativeHexValue(value[i + 1]);
      const int lo = nativeHexValue(value[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(c == '+' ? ' ' : c);
  }
  return out;
}

static std::string nativeQueryParam(const std::string& target, const std::string& key)
{
  const size_t q = target.find('?');
  if (q == std::string::npos) return std::string();
  const std::string query = target.substr(q + 1);
  size_t pos = 0;
  while (pos <= query.size()) {
    size_t amp = query.find('&', pos);
    if (amp == std::string::npos) amp = query.size();
    const std::string part = query.substr(pos, amp - pos);
    const size_t eq = part.find('=');
    const std::string k = nativeUrlDecode(eq == std::string::npos ? part : part.substr(0, eq));
    if (k == key) return nativeUrlDecode(eq == std::string::npos ? std::string() : part.substr(eq + 1));
    if (amp >= query.size()) break;
    pos = amp + 1;
  }
  return std::string();
}

static std::string nativeMimeTypeForPath(const std::string& path)
{
  const std::string ext = nativeFileExtensionLower(path);
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "webp") return "image/webp";
  if (ext == "gif") return "image/gif";
  if (ext == "bmp") return "image/bmp";
  if (ext == "svg") return "image/svg+xml";
  if (ext == "mp4" || ext == "m4v") return "video/mp4";
  if (ext == "mov") return "video/quicktime";
  if (ext == "webm") return "video/webm";
  if (ext == "mkv") return "video/x-matroska";
  if (ext == "avi") return "video/x-msvideo";
  return "application/octet-stream";
}

static bool nativeReadBinaryFile(const std::string& path, std::string& out)
{
  out.clear();
  if (path.empty()) return false;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  if (size < 0) { std::fclose(f); return false; }
  std::fseek(f, 0, SEEK_SET);
  out.resize(static_cast<size_t>(size));
  if (size > 0) {
    const size_t got = std::fread(&out[0], 1, static_cast<size_t>(size), f);
    if (got != static_cast<size_t>(size)) { std::fclose(f); out.clear(); return false; }
  }
  std::fclose(f);
  return true;
}

static std::string nativeHttpBinaryResponse(int status, const std::string& body, const std::string& contentType)
{
  std::ostringstream res;
  res << "HTTP/1.1 " << status << (status == 200 ? " OK" : " ERROR") << "\r\n";
  res << "Content-Type: " << contentType << "\r\n";
  res << "Access-Control-Allow-Origin: *\r\n";
  res << "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n";
  res << "Access-Control-Allow-Headers: Content-Type,Range\r\n";
  res << "Accept-Ranges: bytes\r\n";
  res << "Cache-Control: no-store\r\n";
  res << "Connection: close\r\n";
  res << "Content-Length: " << body.size() << "\r\n\r\n";
  std::string header = res.str();
  header.append(body);
  return header;
}

static std::string nativeHttpBinaryRangeResponse(const std::string& body, const std::string& contentType, size_t totalSize, size_t start, size_t end)
{
  std::ostringstream res;
  res << "HTTP/1.1 206 Partial Content\r\n";
  res << "Content-Type: " << contentType << "\r\n";
  res << "Access-Control-Allow-Origin: *\r\n";
  res << "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n";
  res << "Access-Control-Allow-Headers: Content-Type,Range\r\n";
  res << "Accept-Ranges: bytes\r\n";
  res << "Content-Range: bytes " << start << "-" << end << "/" << totalSize << "\r\n";
  res << "Cache-Control: no-store\r\n";
  res << "Connection: close\r\n";
  res << "Content-Length: " << body.size() << "\r\n\r\n";
  std::string header = res.str();
  header.append(body);
  return header;
}

static bool nativeParseByteRange(const std::string& req, size_t totalSize, size_t& startOut, size_t& endOut)
{
  if (totalSize == 0) return false;
  const std::string lowerReq = nativeLower(req);
  size_t p = lowerReq.find("range:");
  if (p == std::string::npos) return false;
  size_t lineEnd = lowerReq.find('\n', p);
  std::string line = lowerReq.substr(p, lineEnd == std::string::npos ? std::string::npos : lineEnd - p);
  size_t b = line.find("bytes=");
  if (b == std::string::npos) return false;
  b += 6;
  size_t dash = line.find('-', b);
  if (dash == std::string::npos) return false;
  const std::string a = nativeTrim(line.substr(b, dash - b));
  size_t epos = dash + 1;
  size_t comma = line.find(',', epos);
  const std::string z = nativeTrim(line.substr(epos, comma == std::string::npos ? std::string::npos : comma - epos));
  if (a.empty()) return false;
  size_t start = static_cast<size_t>(std::strtoull(a.c_str(), nullptr, 10));
  size_t end = z.empty() ? (totalSize - 1) : static_cast<size_t>(std::strtoull(z.c_str(), nullptr, 10));
  if (start >= totalSize) return false;
  if (end >= totalSize) end = totalSize - 1;
  if (end < start) return false;
  startOut = start;
  endOut = end;
  return true;
}

static bool nativeGetBinaryFileSize(const std::string& path, size_t& sizeOut)
{
  sizeOut = 0;
  if (path.empty()) return false;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
#ifdef _WIN32
  if (_fseeki64(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
  const __int64 size = _ftelli64(f);
#else
  if (fseeko(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
  const off_t size = ftello(f);
#endif
  std::fclose(f);
  if (size < 0) return false;
  sizeOut = static_cast<size_t>(size);
  return true;
}

static bool nativeReadBinaryFileRange(const std::string& path, size_t start, size_t length, std::string& out)
{
  out.clear();
  if (path.empty()) return false;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
#ifdef _WIN32
  if (_fseeki64(f, static_cast<__int64>(start), SEEK_SET) != 0) {
#else
  if (fseeko(f, static_cast<off_t>(start), SEEK_SET) != 0) {
#endif
    std::fclose(f);
    return false;
  }
  out.resize(length);
  if (length > 0) {
    const size_t got = std::fread(&out[0], 1, length, f);
    if (got != length) {
      std::fclose(f);
      out.clear();
      return false;
    }
  }
  std::fclose(f);
  return true;
}

static std::string nativeBuildTpMediaResponse(const std::string& req)
{
  const std::string target = nativeRequestTarget(req);
  std::string mediaPath = nativeQueryParam(target, "path");
  if (mediaPath.empty()) mediaPath = nativeQueryParam(target, "file");
  if (mediaPath.empty()) {
    return nativeHttpResponse(400, "{\"ok\":false,\"error\":\"missing_media_path\"}");
  }
  mediaPath = nativeTrim(mediaPath);
  size_t totalSize = 0;
  if (!nativeGetBinaryFileSize(mediaPath, totalSize)) {
    return nativeHttpResponse(404, "{\"ok\":false,\"error\":\"media_not_found\"}");
  }
  const std::string mime = nativeMimeTypeForPath(mediaPath);
  size_t start = 0;
  size_t end = 0;
  std::string data;
  if (nativeParseByteRange(req, totalSize, start, end)) {
    if (!nativeReadBinaryFileRange(mediaPath, start, end - start + 1, data)) {
      return nativeHttpResponse(500, "{\"ok\":false,\"error\":\"media_read_failed\"}");
    }
    return nativeHttpBinaryRangeResponse(data, mime, totalSize, start, end);
  }
  if (!nativeReadBinaryFile(mediaPath, data)) {
    return nativeHttpResponse(500, "{\"ok\":false,\"error\":\"media_read_failed\"}");
  }
  return nativeHttpBinaryResponse(200, data, mime);
}

static void nativeQueueHttpCommand(const std::string& commandBody)
{
  if (commandBody.empty()) return;
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  // Limite defensivo: em uso normal o timer esvazia esta fila no ciclo
  // seguinte. O teto impede um cliente abandonado de consumir memoria sem fim.
  if (g_nativeHttpCommandQueue.size() >= 1024) {
    g_nativeHttpCommandQueue.pop_front();
  }
  g_nativeHttpCommandQueue.push_back(commandBody);
}

// Executado exclusivamente pelo startupTimer, portanto na thread principal do
// REAPER. Nenhuma funcao nativeApply* abaixo pode voltar para a thread HTTP.
static void nativeApplyHttpCommandOnMainThread(const std::string& commandBody)
{
  if (commandBody.empty()) return;

  const std::string commandType = nativeJsonExtractString(commandBody, "type");
  const std::string commandRole = nativeLower(nativeJsonExtractString(commandBody, "role"));
  const std::string commandClientRole = nativeLower(nativeJsonExtractString(commandBody, "clientRole"));
  const std::string commandAppRole = nativeLower(nativeJsonExtractString(commandBody, "appRole"));
  const bool commandIsDirectorRole = commandRole == "director" || commandRole == "diretor" ||
    commandClientRole == "director" || commandClientRole == "diretor" ||
    commandAppRole == "director" || commandAppRole == "diretor";
  const bool lightweightDirectorHeartbeat = commandType == "director_active" || commandType == "director_heartbeat" ||
    ((commandType == "app_heartbeat" || commandType == "heartbeat") && commandIsDirectorRole);

  if (commandType == "director_enter" || commandType == "director_active" || commandType == "director_heartbeat" ||
      ((commandType == "app_heartbeat" || commandType == "heartbeat") && commandIsDirectorRole)) {
    // Somente um novo login explicito devolve o controle ao App Diretor.
    // Heartbeat de uma sessao expulsa pelo PC nao pode reabrir APP ATIVO.
    if (commandType == "director_enter") {
      g_state.pcAccessOverride = false;
      // A entrada do app nunca pode herdar o mixer do ultimo snapshot. O
      // usuario pode ter alterado mute/solo/volume enquanto nenhum app ou Lua
      // estava ativo.
      g_nativeForceSnapshotBuild.store(true);
      g_nativeForceStateBuild.store(true);
      if (SetExtState_ptr) {
        SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_REQUEST_V1", "", false);
        SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_REQUEST_AT_V1", "", false);
        SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_COMMAND_V1", "", false);
        SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_LOGOUT_TARGET_V1", "", false);
      }

      // Uma nova entrada no Diretor com o transporte parado nao herda fila
      // antiga. Esta consulta agora acontece na thread principal.
      char projectPathBuf[2048] = "";
      ReaProject* project = getCurrentProject(projectPathBuf, static_cast<int>(sizeof(projectPathBuf)));
      const int directorEnterPlayState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(project) : 0;
      const bool directorEnterPlaying = ((directorEnterPlayState & 1) == 1) || ((directorEnterPlayState & 4) == 4);
      if (!directorEnterPlaying) {
        std::lock_guard<std::mutex> lock(g_nativeMutex);
        nativeClearAllQueueStateLocked();
        nativeBumpSharedRevisionLocked("director");
      }
    }
    if (!g_state.pcAccessOverride) {
      {
        std::lock_guard<std::mutex> lock(g_nativeMutex);
        g_nativeLastDirectorHeartbeat = std::chrono::steady_clock::now();
      }
      if (SetExtState_ptr) {
        SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_V1", "1", false);
        SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_AT_SEC_V1", "0", false);
      }
      requestAppActiveScreenForDirector();
    } else if (SetExtState_ptr) {
      SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_V1", "0", false);
      SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_AT_SEC_V1", "0", false);
    }
  }

  if (commandType == "director_force_logout_ack" || commandType == "director_logout_ack" ||
      commandType == "director_logout" || commandType == "app_logout") {
    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      g_nativeLastDirectorHeartbeat = std::chrono::steady_clock::time_point();
    }
    g_state.appActiveScreenOpen = false;
    if (SetExtState_ptr) {
      SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_V1", "0", false);
      SetExtState_ptr(kNativeExtStateSection, "DIRECTOR_ACTIVE_AT_SEC_V1", "0", false);
    }
  }

  const bool handledByNative = nativeApplyDirectorListScrollCommand(commandBody) ||
    nativeApplyDirectorFamilyDrawersCommand(commandBody) ||
    nativeApplyStopPauseModeCommand(commandBody) ||
    nativeApplyManualStopFadeoutCommand(commandBody) ||
    nativeApplyTunerCommand(commandBody) ||
    nativeApplyMixerCommand(commandBody) ||
    nativeApplyQueueCommand(commandBody) ||
    nativeApplyAutoCommand(commandBody) ||
    nativeApplyPreviewCommand(commandBody) ||
    nativeApplyLiveMarkCommand(commandBody) ||
    nativeApplyLoopCommand(commandBody) ||
    nativeApplyMultiLoopCommand(commandBody) ||
    nativeApplyPremixCommand(commandBody) ||
    nativeApplyTimerCommand(commandBody) ||
    nativeApplyPlaylistNumberSortCommand(commandBody) ||
    nativeApplyPlaylistCommand(commandBody) ||
    nativeApplySelectionCommand(commandBody) ||
    nativeApplyTransportCommand(commandBody) ||
    nativeApplyMarkerCommand(commandBody) ||
    nativeSelectProjectFromCommand(commandBody);

  if (handledByNative) nativeMirrorCommandToLuaIfNeeded(commandType, commandBody);

  if (!handledByNative && !lightweightDirectorHeartbeat) {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    if (g_nativeCommandQueue.size() > 200) g_nativeCommandQueue.pop_front();
    g_nativeCommandQueue.push_back(commandBody);
    g_nativeCommandHistory.push_back(commandBody);
    if (g_nativeCommandHistory.size() > 120) {
      g_nativeCommandHistory.erase(
        g_nativeCommandHistory.begin(),
        g_nativeCommandHistory.begin() + static_cast<long>(g_nativeCommandHistory.size() - 120));
    }
    ++g_nativeCommandSequence;
  }
  if (!lightweightDirectorHeartbeat) g_nativeForceStateBuild.store(true);
}

static void nativeProcessHttpCommandsOnMainThread()
{
  std::deque<std::string> pending;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    const size_t count = std::min<size_t>(g_nativeHttpCommandQueue.size(), 128);
    for (size_t i = 0; i < count; ++i) {
      pending.push_back(std::move(g_nativeHttpCommandQueue.front()));
      g_nativeHttpCommandQueue.pop_front();
    }
  }
  for (const std::string& commandBody : pending) {
    nativeApplyHttpCommandOnMainThread(commandBody);
  }
}

static void nativeHandleClient(native_socket_t client)
{
  const std::string req = nativeReadHttpRequest(client);
  if (req.empty()) { nativeCloseSocket(client); return; }
  const std::string path = nativeRequestPath(req);
  std::string body;

  if (nativeStartsWith(req, "OPTIONS ")) {
    body = nativeHttpResponse(200, "{}", "application/json; charset=utf-8");
  } else if (path == "/media" || path == "/tp-media" || path == "/teleprompt-media") {
    body = nativeBuildTpMediaResponse(req);
  } else if (path == "/state" || path == "/state.json" || path == "/snapshot" || path == "/projects" || path == "/projects.json") {
    std::string snapshot;
    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      snapshot = g_nativeStateJson.empty()
        ? std::string("{\"ok\":false,\"connected\":false}")
        : g_nativeStateJson;
    }
    body = nativeHttpResponse(200, snapshot);
  } else if (path == "/meters" || path == "/meters.json") {
    // Apenas assina a coleta. A API do REAPER continua sendo acessada somente
    // pela thread principal em nativeUpdateTrackMetersOnMainThread().
    g_nativeMetersLastRequestMs.store(nativeSteadyNowMs(), std::memory_order_relaxed);
    std::string meters;
    {
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      meters = g_nativeMetersJson;
    }
    body = nativeHttpResponse(200, meters);
  } else if (path == "/command" && nativeRequestIsPost(req)) {
    const std::string commandBody = nativeTrim(nativeRequestBody(req));
    if (!commandBody.empty()) nativeQueueHttpCommand(commandBody);
    body = nativeHttpResponse(200, "{\"ok\":true,\"nativeBridge\":true,\"queued\":true}");
  } else if (path == "/health" || path == "/ping") {
    body = nativeHttpResponse(200, "{\"ok\":true,\"nativeBridge\":true}");
  } else {
    body = nativeHttpResponse(404, "{\"ok\":false,\"error\":\"not_found\"}");
  }
#ifdef _WIN32
  send(client, body.c_str(), static_cast<int>(body.size()), 0);
#else
  send(client, body.c_str(), body.size(), 0);
#endif
  nativeCloseSocket(client);
}

static void nativeBridgeServerThread()
{
#ifdef _WIN32
  WSADATA wsaData{};
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) g_nativeWinsockStarted = true;
#endif
  native_socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == kInvalidNativeSocket) return;

  int opt = 1;
#ifdef _WIN32
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(kNativeBridgePort));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    nativeCloseSocket(serverSocket);
    return;
  }
  if (listen(serverSocket, 16) != 0) {
    nativeCloseSocket(serverSocket);
    return;
  }
  g_nativeServerSocket = serverSocket;

  while (g_nativeRunning.load()) {
    sockaddr_in clientAddr{};
#ifdef _WIN32
    int clientLen = sizeof(clientAddr);
#else
    socklen_t clientLen = sizeof(clientAddr);
#endif
    native_socket_t client = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (client == kInvalidNativeSocket) continue;
    nativeHandleClient(client);
  }
  nativeCloseSocket(serverSocket);
  g_nativeServerSocket = kInvalidNativeSocket;
#ifdef _WIN32
  if (g_nativeWinsockStarted) {
    WSACleanup();
    g_nativeWinsockStarted = false;
  }
#endif
}

static void startNativeBridgeServer()
{
  if (g_nativeRunning.load()) return;
  g_nativeRunning = true;
  try {
    g_nativeThread = std::thread(nativeBridgeServerThread);
  } catch (...) {
    g_nativeRunning = false;
  }
}

static void stopNativeBridgeServer()
{
  if (!g_nativeRunning.load()) return;
  g_nativeRunning = false;
  // Acorda accept() conectando no proprio servidor.
  native_socket_t wake = socket(AF_INET, SOCK_STREAM, 0);
  if (wake != kInvalidNativeSocket) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(kNativeBridgePort));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    connect(wake, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    nativeCloseSocket(wake);
  }
  if (g_nativeThread.joinable()) g_nativeThread.join();
}

static bool g_nativeRuntimeWasActive = false;

static void nativePollAppActiveEnterKey()
{
  const bool keyDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
  if (!nativeAppActivePanelIsOpen()) {
    g_nativeAppActiveEnterWasDown = keyDown;
    return;
  }

#ifdef _WIN32
  // GetAsyncKeyState e global ao sistema. Sem esta verificacao, um Enter usado
  // no Prompt, navegador ou qualquer outro programa podia abrir a confirmacao
  // do APP ATIVO que estava apenas ao fundo. Aceita somente quando o foco esta
  // no REAPER ou na propria janela nativa (solta ou dentro do docker).
  const HWND foreground = GetForegroundWindow();
  const HWND reaperWindow = GetMainHwnd_ptr ? GetMainHwnd_ptr() : nullptr;
  const HWND panelWindow = g_nativeAppActivePanelHwnd;
  const HWND foregroundRoot = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;
  const HWND reaperRoot = reaperWindow ? GetAncestor(reaperWindow, GA_ROOT) : nullptr;
  const HWND panelRoot = panelWindow ? GetAncestor(panelWindow, GA_ROOT) : nullptr;
  const bool reaperFocused = foreground && reaperWindow &&
    (foreground == reaperWindow || foregroundRoot == reaperRoot || IsChild(reaperWindow, foreground));
  const bool panelFocused = foreground && panelWindow &&
    (foreground == panelWindow || foregroundRoot == panelRoot || IsChild(panelWindow, foreground));
  if (!reaperFocused && !panelFocused) {
    // Mantem o latch coerente: voltar ao REAPER com Enter ainda pressionado nao
    // pode transformar a tecla usada em outro programa em um novo toque.
    g_nativeAppActiveEnterWasDown = keyDown;
    return;
  }
#endif

  if (!keyDown) {
    g_nativeAppActiveEnterWasDown = false;
    return;
  }

  // Fallback para dockers/sistemas em que o evento de teclado nao chega ao
  // accelerator. Um toque fisico ainda produz uma unica acao.
  if (!g_nativeAppActiveEnterWasDown) {
    g_nativeAppActiveEnterWasDown = true;
    SendMessage(g_nativeAppActivePanelHwnd, WM_KEYDOWN, VK_RETURN, 0);
  }
}

static void nativeReleaseRuntimeControlOnMainThread()
{
  g_globalStopBreakRequested.store(false);
  g_globalPauseRequested.store(false);
  g_globalStopPauseRequested.store(false);
  g_nativeMultiLoopBypassRequest.store(-1);
  nativeBreakManualStopFadeout();

  const bool ownedLoop = g_nativeMultiLoopActivePair.valid;
  if (ownedLoop || !g_nativeMultiLoopRestore.empty() || !g_nativeMultiLoopFadeTracks.empty()) {
    nativeRestoreMultiLoopTracks();
  }
  nativeRestoreMultiLoopBypassVolumes(true);

  if (ownedLoop) {
    char pathBuf[2048] = "";
    ReaProject* project = getCurrentProject(pathBuf, static_cast<int>(sizeof(pathBuf)));
    nativeSetRepeatEnabled(project, false);
    nativeClearLoopTimeRange(project);
  }

  g_nativeMultiLoopActivePair = NativeMultiLoopPair();
  g_nativeMultiLoopDisarmedPairKey.clear();
  g_nativeMultiLoopBypassActive = false;
  g_nativeMultiLoopBypassSongKey.clear();
  g_nativeMultiLoopBypassVolumeBaseline.clear();
  nativeCancelQueueHandoffProtection();
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativePendingSelection = NativePendingSelectionCommand{};
  }
}

static void startupTimer()
{
  static bool standbyDiscoveryPublished = false;
  static std::string standbyDiscoveryProjectSignature;

  // A leitura do heartbeat e a unica verificacao permanente. Sem App Diretor
  // nem Lua ativo, nenhum atalho, transporte, loop, fila ou varredura pesada do
  // projeto e executado pela extensao. A descoberta leve de abas fica ativa.
  nativeRefreshLuaControlHeartbeatFromExtState();
  nativePollAppActiveEnterKey();
  // A fila HTTP precisa ser consumida antes de calcular runtimeActive: o
  // director_enter que acorda a extensao tambem chega por essa fila.
  nativeProcessHttpCommandsOnMainThread();
  nativeUpdateTrackMetersOnMainThread();
  const bool runtimeActive = nativeIsRuntimeControlActive();
  if (runtimeActive) {
    const bool runtimeJustActivated = !g_nativeRuntimeWasActive;
    standbyDiscoveryPublished = false;
    nativeRefreshStopPauseModeFromExtState();
    g_nativeRuntimeWasActive = true;
    if (runtimeJustActivated) {
      // Tambem cobre clientes antigos que acordam a extensao apenas com
      // heartbeat, sem enviar director_enter.
      g_nativeForceSnapshotBuild.store(true);
      g_nativeForceStateBuild.store(true);
    }
    nativeProcessGlobalStopBreakOnMainThread();
    nativeProcessGlobalPauseOnMainThread();
    nativeProcessGlobalStopPauseOnMainThread();
    nativeUpdateManualStopFadeoutOnMainThread();
    nativeProcessPendingSelectionOnMainThread();
    nativeProcessEnsureReaPitchRequestOnMainThread();
    nativeBridgeTick();
    nativeRefreshAppActivePanelModel();
    nativeProcessMultiLoopsOnMainThread();
    processPcResumeRequestFromAppActiveOnMainThread();
    processAppActiveScreenRequestsOnMainThread();
  } else if (g_nativeRuntimeWasActive) {
    g_nativeRuntimeWasActive = false;
    nativeReleaseRuntimeControlOnMainThread();
  }
  processPendingScriptSwitchOnMainThread();
  ++g_state.startupTimerTicks;

  const std::string projectSignature = getCurrentProjectSignature();
  if (projectSignature != g_state.activeProjectSignature) {
    g_state.activeProjectSignature = projectSignature;
    g_state.projectStableTicks = 0;
  } else {
    ++g_state.projectStableTicks;
  }

  // Publica o projeto mesmo antes de qualquer Lua abrir. Enumera somente as
  // abas do REAPER e os dados de login; a lista completa e os controles seguem
  // dormindo ate o Diretor ou um Lua enviar heartbeat.
  if (!runtimeActive && g_state.projectStableTicks >= 1 &&
      (!standbyDiscoveryPublished || standbyDiscoveryProjectSignature != projectSignature)) {
    nativePublishStandbyDiscoveryState();
    standbyDiscoveryProjectSignature = projectSignature;
    standbyDiscoveryPublished = true;
  }

  // Espera alguns ciclos para o REAPER terminar de montar a interface e carregar
  // os ProjExtState salvos no RPP.
  if (g_state.startupTimerTicks < 30) return;

  normalizeAutoOpenConflict();

  const std::string projectMode = getProjectAutoOpenMode();

  if (!projectMode.empty() && g_state.projectStableTicks >= 6) {
    const std::string projectRunKey = projectSignature + "|" + projectMode;
    if (g_state.autoOpenedProjectSignature != projectRunKey) {
      g_state.autoOpenedProjectSignature = projectRunKey;
      g_state.didGlobalStartupAutoOpen = true;
      runScriptByAutoOpenMode(projectMode);
      return;
    }
  }

  // Auto-inicio global: roda apenas uma vez na abertura do REAPER e nao compete
  // com configuracao especifica do projeto.
  if (!g_state.didGlobalStartupAutoOpen && projectMode.empty()) {
    g_state.didGlobalStartupAutoOpen = true;
    const std::string autoMode = getAutoOpenMode();
    if (!autoMode.empty()) {
      runScriptByAutoOpenMode(autoMode);
    }
  }
}

static bool loadApi(reaper_plugin_info_t* rec)
{
  if (!rec || !rec->GetFunc) return false;

  // Mesmo padrao da SWS: pega plugin_register via GetFunc primeiro.
  // Em algumas combinacoes de REAPER/SDK, depender somente de rec->Register
  // pode fazer a extensao retornar 0 e nem aparecer no menu Extensions.
  plugin_register_ptr = reinterpret_cast<plugin_register_t>(rec->GetFunc("plugin_register"));
  if (!plugin_register_ptr) {
    plugin_register_ptr = rec->Register;
  }

  plugin_getapi_ptr = reinterpret_cast<plugin_getapi_t>(rec->GetFunc("plugin_getapi"));
  ShowMessageBox_ptr = reinterpret_cast<ShowMessageBox_t>(rec->GetFunc("ShowMessageBox"));
  GetMainHwnd_ptr = reinterpret_cast<GetMainHwnd_t>(rec->GetFunc("GetMainHwnd"));
  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  GetAppVersion_ptr = reinterpret_cast<GetAppVersion_t>(rec->GetFunc("GetAppVersion"));
  Main_OnCommand_ptr = reinterpret_cast<Main_OnCommand_t>(rec->GetFunc("Main_OnCommand"));
  GetToggleCommandState_ptr = reinterpret_cast<GetToggleCommandState_t>(rec->GetFunc("GetToggleCommandState"));
  GetToggleCommandStateEx_ptr = reinterpret_cast<GetToggleCommandStateEx_t>(rec->GetFunc("GetToggleCommandStateEx"));
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));
  NamedCommandLookup_ptr = reinterpret_cast<NamedCommandLookup_t>(rec->GetFunc("NamedCommandLookup"));
  ReverseNamedCommandLookup_ptr = reinterpret_cast<ReverseNamedCommandLookup_t>(rec->GetFunc("ReverseNamedCommandLookup"));
  GetExtState_ptr = reinterpret_cast<GetExtState_t>(rec->GetFunc("GetExtState"));
  SetExtState_ptr = reinterpret_cast<SetExtState_t>(rec->GetFunc("SetExtState"));
  readRememberedActiveScriptMode();
  EnumProjects_ptr = reinterpret_cast<EnumProjects_t>(rec->GetFunc("EnumProjects"));
  GetProjExtState_ptr = reinterpret_cast<GetProjExtState_t>(rec->GetFunc("GetProjExtState"));
  SetProjExtState_ptr = reinterpret_cast<SetProjExtState_t>(rec->GetFunc("SetProjExtState"));
  MarkProjectDirty_ptr = reinterpret_cast<MarkProjectDirty_t>(rec->GetFunc("MarkProjectDirty"));
  CountProjectMarkers_ptr = reinterpret_cast<CountProjectMarkers_t>(rec->GetFunc("CountProjectMarkers"));
  EnumProjectMarkers3_ptr = reinterpret_cast<EnumProjectMarkers3_t>(rec->GetFunc("EnumProjectMarkers3"));
  GetRegionOrMarker_ptr = reinterpret_cast<GetRegionOrMarker_t>(rec->GetFunc("GetRegionOrMarker"));
  GetRegionOrMarkerInfo_Value_ptr = reinterpret_cast<GetRegionOrMarkerInfo_Value_t>(rec->GetFunc("GetRegionOrMarkerInfo_Value"));
  GetProjectStateChangeCount_ptr = reinterpret_cast<GetProjectStateChangeCount_t>(rec->GetFunc("GetProjectStateChangeCount"));
  GetPlayStateEx_ptr = reinterpret_cast<GetPlayStateEx_t>(rec->GetFunc("GetPlayStateEx"));
  GetPlayPositionEx_ptr = reinterpret_cast<GetPlayPositionEx_t>(rec->GetFunc("GetPlayPositionEx"));
  GetCursorPositionEx_ptr = reinterpret_cast<GetCursorPositionEx_t>(rec->GetFunc("GetCursorPositionEx"));
  GetSet_LoopTimeRange2_ptr = reinterpret_cast<GetSet_LoopTimeRange2_t>(rec->GetFunc("GetSet_LoopTimeRange2"));
  GetSetRepeatEx_ptr = reinterpret_cast<GetSetRepeatEx_t>(rec->GetFunc("GetSetRepeatEx"));
  DockIsChildOfDock_ptr = reinterpret_cast<DockIsChildOfDock_t>(rec->GetFunc("DockIsChildOfDock"));
  DockWindowActivate_ptr = reinterpret_cast<DockWindowActivate_t>(rec->GetFunc("DockWindowActivate"));
  DockWindowAdd_ptr = reinterpret_cast<DockWindowAdd_t>(rec->GetFunc("DockWindowAdd"));
  DockWindowRefreshForHWND_ptr = reinterpret_cast<DockWindowRefreshForHWND_t>(rec->GetFunc("DockWindowRefreshForHWND"));
  DockWindowRemove_ptr = reinterpret_cast<DockWindowRemove_t>(rec->GetFunc("DockWindowRemove"));
  CountTracks_ptr = reinterpret_cast<CountTracks_t>(rec->GetFunc("CountTracks"));
  GetTrack_ptr = reinterpret_cast<GetTrack_t>(rec->GetFunc("GetTrack"));
  GetMasterTrack_ptr = reinterpret_cast<GetMasterTrack_t>(rec->GetFunc("GetMasterTrack"));
  GetTrackName_ptr = reinterpret_cast<GetTrackName_t>(rec->GetFunc("GetTrackName"));
  GetTrackNumMediaItems_ptr = reinterpret_cast<GetTrackNumMediaItems_t>(rec->GetFunc("GetTrackNumMediaItems"));
  GetTrackMediaItem_ptr = reinterpret_cast<GetTrackMediaItem_t>(rec->GetFunc("GetTrackMediaItem"));
  GetMediaItemInfo_Value_ptr = reinterpret_cast<GetMediaItemInfo_Value_t>(rec->GetFunc("GetMediaItemInfo_Value"));
  SetMediaItemInfo_Value_ptr = reinterpret_cast<SetMediaItemInfo_Value_t>(rec->GetFunc("SetMediaItemInfo_Value"));
  GetSetMediaItemInfo_String_ptr = reinterpret_cast<GetSetMediaItemInfo_String_t>(rec->GetFunc("GetSetMediaItemInfo_String"));
  GetActiveTake_ptr = reinterpret_cast<GetActiveTake_t>(rec->GetFunc("GetActiveTake"));
  GetTakeName_ptr = reinterpret_cast<GetTakeName_t>(rec->GetFunc("GetTakeName"));
  GetSetMediaItemTakeInfo_String_ptr = reinterpret_cast<GetSetMediaItemTakeInfo_String_t>(rec->GetFunc("GetSetMediaItemTakeInfo_String"));
  GetMediaItemTakeInfo_Value_ptr = reinterpret_cast<GetMediaItemTakeInfo_Value_t>(rec->GetFunc("GetMediaItemTakeInfo_Value"));
  GetMediaItemTake_Source_ptr = reinterpret_cast<GetMediaItemTake_Source_t>(rec->GetFunc("GetMediaItemTake_Source"));
  GetMediaSourceFileName_ptr = reinterpret_cast<GetMediaSourceFileName_t>(rec->GetFunc("GetMediaSourceFileName"));
  TakeFX_GetCount_ptr = reinterpret_cast<TakeFX_GetCount_t>(rec->GetFunc("TakeFX_GetCount"));
  TakeFX_GetEnabled_ptr = reinterpret_cast<TakeFX_GetEnabled_t>(rec->GetFunc("TakeFX_GetEnabled"));
  TakeFX_SetEnabled_ptr = reinterpret_cast<TakeFX_SetEnabled_t>(rec->GetFunc("TakeFX_SetEnabled"));
  GetMediaTrackInfo_Value_ptr = reinterpret_cast<GetMediaTrackInfo_Value_t>(rec->GetFunc("GetMediaTrackInfo_Value"));
  SetMediaTrackInfo_Value_ptr = reinterpret_cast<SetMediaTrackInfo_Value_t>(rec->GetFunc("SetMediaTrackInfo_Value"));
  Track_GetPeakInfo_ptr = reinterpret_cast<Track_GetPeakInfo_t>(rec->GetFunc("Track_GetPeakInfo"));
  SelectProjectInstance_ptr = reinterpret_cast<SelectProjectInstance_t>(rec->GetFunc("SelectProjectInstance"));
  TrackList_AdjustWindows_ptr = reinterpret_cast<TrackList_AdjustWindows_t>(rec->GetFunc("TrackList_AdjustWindows"));
  UpdateArrange_ptr = reinterpret_cast<UpdateArrange_t>(rec->GetFunc("UpdateArrange"));
  SetEditCurPos_ptr = reinterpret_cast<SetEditCurPos_t>(rec->GetFunc("SetEditCurPos"));
  SetEditCurPos2_ptr = reinterpret_cast<SetEditCurPos2_t>(rec->GetFunc("SetEditCurPos2"));
  GetTrackGUID_ptr = reinterpret_cast<GetTrackGUID_t>(rec->GetFunc("GetTrackGUID"));
  guidToString_ptr = reinterpret_cast<guidToString_t>(rec->GetFunc("guidToString"));
  TrackFX_GetCount_ptr = reinterpret_cast<TrackFX_GetCount_t>(rec->GetFunc("TrackFX_GetCount"));
  TrackFX_GetFXName_ptr = reinterpret_cast<TrackFX_GetFXName_t>(rec->GetFunc("TrackFX_GetFXName"));
  TrackFX_GetNumParams_ptr = reinterpret_cast<TrackFX_GetNumParams_t>(rec->GetFunc("TrackFX_GetNumParams"));
  TrackFX_GetParamName_ptr = reinterpret_cast<TrackFX_GetParamName_t>(rec->GetFunc("TrackFX_GetParamName"));
  TrackFX_FormatParamValueNormalized_ptr = reinterpret_cast<TrackFX_FormatParamValueNormalized_t>(rec->GetFunc("TrackFX_FormatParamValueNormalized"));
  TrackFX_GetParamNormalized_ptr = reinterpret_cast<TrackFX_GetParamNormalized_t>(rec->GetFunc("TrackFX_GetParamNormalized"));
  TrackFX_SetParamNormalized_ptr = reinterpret_cast<TrackFX_SetParamNormalized_t>(rec->GetFunc("TrackFX_SetParamNormalized"));
  TrackFX_AddByName_ptr = reinterpret_cast<TrackFX_AddByName_t>(rec->GetFunc("TrackFX_AddByName"));
  TrackFX_CopyToTrack_ptr = reinterpret_cast<TrackFX_CopyToTrack_t>(rec->GetFunc("TrackFX_CopyToTrack"));

  if (!plugin_register_ptr) {
    showDiagnostic("VS Hook Loader nao carregou: plugin_register indisponivel.");
    return false;
  }

  return true;
}

static void initialize()
{
  if (g_state.initialized) return;

  bool hasRegisteredAction = false;

  for (AutoOpenEntry& entry : g_autoOpenEntries) {
    entry.commandId = plugin_register_ptr("custom_action", (void*)&entry.action);
    if (entry.commandId != 0) {
      hasRegisteredAction = true;
    }
  }

  for (AutoOpenEntry& entry : g_projectAutoOpenEntries) {
    entry.commandId = plugin_register_ptr("custom_action", (void*)&entry.action);
    if (entry.commandId != 0) {
      hasRegisteredAction = true;
    }
  }

  unregisterLegacyAppActiveLuaScript();

  for (ScriptEntry& script : g_scripts) {
    const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
    if (mode == "app_active") continue;
    script.commandId = plugin_register_ptr("custom_action", (void*)&script.action);
    if (script.commandId != 0) {
      hasRegisteredAction = true;
    }
  }

  // Registra scripts ocultos no Action List quando o arquivo ja existe. APP ATIVO
  // agora e uma janela nativa da extensao e nao depende mais de ReaScript.
  for (ScriptEntry& script : g_scripts) {
    const std::string mode = script.autoOpenMode ? script.autoOpenMode : "";
    if (!script.showInExtensionsMenu && mode != "app_active") {
      registerScriptInActionListIfPresent(script);
    }
  }

  registerClipboardApi();
  registerNativeBridgeApi();
  startNativeBridgeServer();

  if (plugin_register_ptr("accelerator", reinterpret_cast<void*>(&g_vshookGlobalHotkeyAccelerator))) {
    g_state.acceleratorRegistered = true;
  }

  if (hasRegisteredAction) {
    if (plugin_register_ptr("hookcommand", reinterpret_cast<void*>(&hookCommand))) {
      g_state.commandHookRegistered = true;
    }

    if (plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2))) {
      g_state.commandHook2Registered = true;
    }

    if (plugin_register_ptr("toggleaction", reinterpret_cast<void*>(&toggleActionState))) {
      g_state.toggleActionRegistered = true;
    }
  } else {
    showDiagnostic("VS Hook Loader carregou, mas nao conseguiu registrar as actions no REAPER.");
  }

  if (plugin_register_ptr("hookcustommenu", reinterpret_cast<void*>(&menuHook))) {
    g_state.menuHookRegistered = true;
  }

  if (AddExtensionsMainMenu_ptr) {
    AddExtensionsMainMenu_ptr();
  }

  // O timer fica ativo de forma leve para suportar auto-inicio por projeto
  // quando o usuario troca/abre projetos depois do REAPER ja estar rodando.
  g_state.startupTimerTicks = 0;
  g_state.projectStableTicks = 0;
  g_state.didGlobalStartupAutoOpen = false;
  g_state.activeProjectSignature.clear();
  g_state.autoOpenedProjectSignature.clear();
  rememberActiveScriptMode("");
  if (plugin_register_ptr("timer", reinterpret_cast<void*>(&startupTimer))) {
    g_state.timerRegistered = true;
  } else if (!getAutoOpenMode().empty()) {
    runScriptByAutoOpenMode(getAutoOpenMode());
  }

  g_state.initialized = true;
}

static void shutdown()
{
  if (!plugin_register_ptr) return;

  nativeCloseAppActivePanel();

  if (g_state.timerRegistered) {
    plugin_register_ptr("-timer", reinterpret_cast<void*>(&startupTimer));
    g_state.timerRegistered = false;
  }

  stopNativeBridgeServer();
  unregisterNativeBridgeApi();
  unregisterClipboardApi();

  if (g_state.acceleratorRegistered) {
    plugin_register_ptr("-accelerator", reinterpret_cast<void*>(&g_vshookGlobalHotkeyAccelerator));
    g_state.acceleratorRegistered = false;
  }

  if (g_state.menuHookRegistered) {
    plugin_register_ptr("-hookcustommenu", reinterpret_cast<void*>(&menuHook));
    g_state.menuHookRegistered = false;
  }

  if (g_state.toggleActionRegistered) {
    plugin_register_ptr("-toggleaction", reinterpret_cast<void*>(&toggleActionState));
    g_state.toggleActionRegistered = false;
  }

  if (g_state.commandHook2Registered) {
    plugin_register_ptr("-hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHook2Registered = false;
  }

  if (g_state.commandHookRegistered) {
    plugin_register_ptr("-hookcommand", reinterpret_cast<void*>(&hookCommand));
    g_state.commandHookRegistered = false;
  }

  for (ScriptEntry& script : g_scripts) {
    if (script.commandId != 0) {
      plugin_register_ptr("-custom_action", (void*)&script.action);
      script.commandId = 0;
    }
  }

  for (AutoOpenEntry& entry : g_autoOpenEntries) {
    if (entry.commandId != 0) {
      plugin_register_ptr("-custom_action", (void*)&entry.action);
      entry.commandId = 0;
    }
  }

  for (AutoOpenEntry& entry : g_projectAutoOpenEntries) {
    if (entry.commandId != 0) {
      plugin_register_ptr("-custom_action", (void*)&entry.action);
      entry.commandId = 0;
    }
  }

  g_state.activeProjectSignature.clear();
  g_state.autoOpenedProjectSignature.clear();
  rememberActiveScriptMode("");
  g_state.modeBeforeDirectorApp.clear();
  g_appActiveOpenRequested.store(false);
  g_state.appActiveScreenOpen = false;
  g_state.pcAccessOverride = false;
  g_state.initialized = false;
}

} // namespace vshook

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
  REAPER_PLUGIN_HINSTANCE instance,
  reaper_plugin_info_t* rec)
{
  vshook::g_pluginInstance = instance;

  if (!rec) {
    vshook::shutdown();
    return 0;
  }

  // Nao bloqueia por caller_version: alguns REAPERs reportam versao diferente
  // e isso fazia a extensao retornar 0 silenciosamente, sem criar o menu Extensions.
  if (!vshook::loadApi(rec)) return 0;

  vshook::initialize();
  return 1;
}

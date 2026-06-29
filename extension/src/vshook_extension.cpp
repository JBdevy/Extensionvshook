#define SWELL_PROVIDED_BY_APP
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

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using plugin_getapi_t = void* (*)(const char*);
using ShowMessageBox_t = int (*)(const char*, const char*, int);
using GetResourcePath_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using GetToggleCommandState_t = int (*)(int);
using GetToggleCommandStateEx_t = int (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);
using GetExtState_t = const char* (*)(const char*, const char*);
using SetExtState_t = void (*)(const char*, const char*, const char*, bool);
using EnumProjects_t = ReaProject* (*)(int, char*, int);
using GetProjExtState_t = int (*)(ReaProject*, const char*, const char*, char*, int);
using SetProjExtState_t = int (*)(ReaProject*, const char*, const char*, const char*);
using CountProjectMarkers_t = int (*)(ReaProject*, int*, int*);
using EnumProjectMarkers3_t = int (*)(ReaProject*, int, bool*, double*, double*, const char**, int*, int*);
using GetPlayStateEx_t = int (*)(ReaProject*);
using GetPlayPositionEx_t = double (*)(ReaProject*);

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
using TakeFX_GetCount_t = int (*)(MediaItem_Take*);
using TakeFX_GetEnabled_t = bool (*)(MediaItem_Take*, int);
using TakeFX_SetEnabled_t = void (*)(MediaItem_Take*, int, bool);
using GetMediaTrackInfo_Value_t = double (*)(MediaTrack*, const char*);
using GetTrackGUID_t = GUID* (*)(MediaTrack*);
using guidToString_t = void (*)(const GUID*, char*);

static plugin_register_t plugin_register_ptr = nullptr;
static plugin_getapi_t plugin_getapi_ptr = nullptr;
static ShowMessageBox_t ShowMessageBox_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static GetToggleCommandState_t GetToggleCommandState_ptr = nullptr;
static GetToggleCommandStateEx_t GetToggleCommandStateEx_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;
static GetExtState_t GetExtState_ptr = nullptr;
static SetExtState_t SetExtState_ptr = nullptr;
static EnumProjects_t EnumProjects_ptr = nullptr;
static GetProjExtState_t GetProjExtState_ptr = nullptr;
static SetProjExtState_t SetProjExtState_ptr = nullptr;
static CountProjectMarkers_t CountProjectMarkers_ptr = nullptr;
static EnumProjectMarkers3_t EnumProjectMarkers3_ptr = nullptr;
static GetPlayStateEx_t GetPlayStateEx_ptr = nullptr;
static GetPlayPositionEx_t GetPlayPositionEx_ptr = nullptr;

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
static TakeFX_GetCount_t TakeFX_GetCount_ptr = nullptr;
static TakeFX_GetEnabled_t TakeFX_GetEnabled_ptr = nullptr;
static TakeFX_SetEnabled_t TakeFX_SetEnabled_ptr = nullptr;
static GetMediaTrackInfo_Value_t GetMediaTrackInfo_Value_ptr = nullptr;
static GetTrackGUID_t GetTrackGUID_ptr = nullptr;
static guidToString_t guidToString_ptr = nullptr;

static const char* kExtStateSection = "VS_HOOK_LOADER";
static const char* kAutoOpenModeKey = "AUTO_OPEN_VSHOOK_MODE";
static const char* kLegacyAutoOpenKey = "AUTO_OPEN_VSHOOK";
static const char* kProjectAutoOpenModeKey = "PROJECT_AUTO_OPEN_VSHOOK_MODE";

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
    { 0, "VSHOOKRUN", "VS Hook Pro", nullptr },
    "VS Hook Pro",
    "VS Hook Pro.lua",
    "pro",
    true
  },
  {
    { 0, "VSHOOKRUNBASIC", "VS Hook Basic", nullptr },
    "VS Hook Basic",
    "VS Hook Basic.lua",
    "basic",
    true
  }
};

struct State {
  bool initialized = false;
  bool commandHookRegistered = false;
  bool commandHook2Registered = false;
  bool toggleActionRegistered = false;
  bool menuHookRegistered = false;
  bool timerRegistered = false;
  bool apiRegistered = false;
  int startupTimerTicks = 0;
  int projectStableTicks = 0;
  bool didGlobalStartupAutoOpen = false;
  std::string activeProjectSignature;
  std::string autoOpenedProjectSignature;
  std::string lastLaunchedMode;
} g_state;

static AutoOpenEntry g_autoOpenEntries[] = {
  {
    { 0, "VSHOOKAUTOPRO", "Iniciar VS Hook Pro junto com o REAPER", nullptr },
    "Iniciar VS Hook Pro junto com o REAPER",
    "pro"
  },
  {
    { 0, "VSHOOKAUTOBASIC", "Iniciar VS Hook Basic junto com o REAPER", nullptr },
    "Iniciar VS Hook Basic junto com o REAPER",
    "basic"
  }
};

static AutoOpenEntry g_projectAutoOpenEntries[] = {
  {
    { 0, "VSHOOKPROJECTAUTOPRO", "Iniciar VS Hook Pro junto com ESTE projeto", nullptr },
    "Iniciar VS Hook Pro junto com ESTE projeto",
    "pro"
  },
  {
    { 0, "VSHOOKPROJECTAUTOBASIC", "Iniciar VS Hook Basic junto com ESTE projeto", nullptr },
    "Iniciar VS Hook Basic junto com ESTE projeto",
    "basic"
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

static bool VS_Hook_SetClipboard(const char* text)
{
  const std::string value = text ? text : "";

#ifdef _WIN32
  const std::wstring wide = utf8ToWide(value);
  const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!mem) return false;

  void* locked = GlobalLock(mem);
  if (!locked) {
    GlobalFree(mem);
    return false;
  }

  std::memcpy(locked, wide.c_str(), bytes);
  GlobalUnlock(mem);

  if (!OpenClipboard(nullptr)) {
    GlobalFree(mem);
    return false;
  }

  EmptyClipboard();
  if (!SetClipboardData(CF_UNICODETEXT, mem)) {
    CloseClipboard();
    GlobalFree(mem);
    return false;
  }

  CloseClipboard();
  return true;
#else
  const size_t bytes = value.size() + 1;
  HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<int>(bytes));
  if (!mem) return false;

  void* locked = GlobalLock(mem);
  if (!locked) {
    GlobalFree(mem);
    return false;
  }

  std::memcpy(locked, value.c_str(), bytes);
  GlobalUnlock(mem);

  if (!OpenClipboard(nullptr)) {
    GlobalFree(mem);
    return false;
  }

  EmptyClipboard();
  SetClipboardData(CF_TEXT, mem);
  CloseClipboard();
  return true;
#endif
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
  "bool\0const char*\0text\0Write text into the system clipboard using the VS Hook extension.";

static char g_apiDefGetClipboard[] =
  "bool\0char*,int\0textOutNeedBig,textOutNeedBig_sz\0Read text from the system clipboard using the VS Hook extension.";

static bool registerClipboardApi()
{
  if (!plugin_register_ptr) return false;

  bool ok = true;
  ok = (plugin_register_ptr("API_VS_Hook_SetClipboard", reinterpret_cast<void*>(&VS_Hook_SetClipboard)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_SetClipboard", reinterpret_cast<void*>(g_apiDefSetClipboard)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_GetClipboard", reinterpret_cast<void*>(&VS_Hook_GetClipboard)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_GetClipboard", reinterpret_cast<void*>(g_apiDefGetClipboard)) != 0) && ok;

  g_state.apiRegistered = ok;
  return ok;
}

static void unregisterClipboardApi()
{
  if (!plugin_register_ptr || !g_state.apiRegistered) return;

  plugin_register_ptr("-APIdef_VS_Hook_GetClipboard", reinterpret_cast<void*>(g_apiDefGetClipboard));
  plugin_register_ptr("-API_VS_Hook_GetClipboard", reinterpret_cast<void*>(&VS_Hook_GetClipboard));
  plugin_register_ptr("-APIdef_VS_Hook_SetClipboard", reinterpret_cast<void*>(g_apiDefSetClipboard));
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

static bool ensureScriptRegistered(ScriptEntry& script)
{
  if (script.scriptCommandId != 0) {
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

  const int commandId = AddRemoveReaScript_ptr(true, 0, script.scriptPath.c_str(), true);
  if (commandId == 0) {
    showDiagnostic(std::string("REAPER nao conseguiu registrar o script:\n") + script.scriptPath);
    return false;
  }

  script.scriptCommandId = commandId;
  return true;
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

static bool isScriptRunning(ScriptEntry& script)
{
  if (!ensureScriptRegistered(script)) return false;
  return getScriptToggleState(script) == 1;
}

static void closeOtherScripts(ScriptEntry& target)
{
  if (!Main_OnCommand_ptr) return;

  for (ScriptEntry& other : g_scripts) {
    if (&other == &target) continue;
    if (!ensureScriptRegistered(other)) continue;

    const int otherState = getScriptToggleState(other);

    // Preferimos o estado nativo do REAPER quando ele funciona.
    // Em alguns setups o defer script nao informa toggle=1 de forma confiavel;
    // por isso tambem usamos o ultimo modo aberto pela propria extensao nesta sessao.
    const bool shouldClose =
      (otherState == 1) ||
      (!g_state.lastLaunchedMode.empty() && other.autoOpenMode && g_state.lastLaunchedMode == other.autoOpenMode);

    if (shouldClose && other.scriptCommandId != 0) {
      Main_OnCommand_ptr(other.scriptCommandId, 0);
    }
  }
}

static void runScript(ScriptEntry& script, bool toggleIfAlreadyOpen = true)
{
  if (!ensureScriptRegistered(script)) {
    return;
  }

  closeOtherScripts(script);

  if (!Main_OnCommand_ptr || script.scriptCommandId == 0) return;

  const int nativeStateBefore = getScriptToggleState(script);
  const bool rememberedAsOpen =
    script.autoOpenMode && !g_state.lastLaunchedMode.empty() && g_state.lastLaunchedMode == script.autoOpenMode;
  const bool consideredOpen = (nativeStateBefore == 1) || rememberedAsOpen;

  if (!toggleIfAlreadyOpen && nativeStateBefore == 1) {
    if (script.autoOpenMode) g_state.lastLaunchedMode = script.autoOpenMode;
    return;
  }

  Main_OnCommand_ptr(script.scriptCommandId, 0);

  if (script.autoOpenMode) {
    // Clique manual no mesmo modo funciona como toggle: se ele ja era o ultimo
    // aberto, considera que o usuario quis fechar e limpa a memoria.
    // Na troca Pro -> Basic / Basic -> Pro, closeOtherScripts ja fechou o outro
    // e aqui registramos o novo modo ativo.
    if (toggleIfAlreadyOpen && consideredOpen) {
      g_state.lastLaunchedMode.clear();
    } else {
      g_state.lastLaunchedMode = script.autoOpenMode;
    }
  }
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
  // "Abrir VS Hook junto com o REAPER" ativo, a extensao nova assume Pro.
  const char* legacy = GetExtState_ptr(kExtStateSection, kLegacyAutoOpenKey);
  if (legacy && std::strcmp(legacy, "1") == 0) {
    return "pro";
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

  // Desliga a chave antiga para ela nao religar o Pro depois que o usuario
  // escolher Basic ou desativar o auto-inicio.
  SetExtState_ptr(kExtStateSection, kLegacyAutoOpenKey, "0", true);
}

static void setAutoOpenMode(const char* mode)
{
  setAutoOpenModeRaw(mode, true);
}

static bool isAutoOpenModeValue(const std::string& mode)
{
  return mode == "pro" || mode == "basic";
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
  oss << reinterpret_cast<std::uintptr_t>(project) << "|" << normalizeSlashes(path ? path : "");
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

  for (ScriptEntry& script : g_scripts) {
    if (script.commandId != 0 && commandId == script.commandId) {
      return isScriptRunning(script) ? 1 : 0;
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
    for (ScriptEntry& script : g_scripts) {
      setMenuCommandChecked(hMenu, script.commandId, script.commandId != 0 && isScriptRunning(script));
    }
    return;
  }

  if (flag != 0) return;

  // Insere de tras para frente porque cada item entra na posicao 0.
  // Ordem final no menu:
  // VS Hook Pro / VS Hook Basic
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

  for (int i = static_cast<int>(sizeof(g_scripts) / sizeof(g_scripts[0])) - 1; i >= 0; --i) {
    if (!g_scripts[i].showInExtensionsMenu) continue;
    if (g_scripts[i].commandId == 0) continue;

    insertMenuString(hMenu, g_scripts[i].displayName, g_scripts[i].commandId, isScriptRunning(g_scripts[i]));
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
static const int kNativeBridgeLiveIntervalMs = 250;
static const int kNativeBridgeSnapshotIntervalMs = 1500;
static const char* kPlaylistExtSection = "CHATGPT_REGION_PLAYLIST";
static const char* kPlaylistExtKey = "PLAYLISTS_DB_V3";
static const char* kNativeExtStateSection = "VS_HOOK_NATIVE_BRIDGE";
static const char* kNativeCommandsExtKey = "COMMANDS_JSON_V1";
static const char* kNativeLuaLiveExtKey = "LUA_LIVE_JSON_V1";

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
static std::vector<std::string> g_nativeCommandHistory;
static uint64_t g_nativeCommandSequence = 0;
static std::string g_nativePremixSelectedSongId;
static std::chrono::steady_clock::time_point g_nativeLastDirectorHeartbeat;
static std::string g_nativePublishedCommandsSignature;
static std::chrono::steady_clock::time_point g_nativeLastLiveBuild;
static std::chrono::steady_clock::time_point g_nativeLastSnapshotBuild;
static bool g_nativeWinsockStarted = false;

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
  std::string parentId;
  std::string parentName;
  std::string blockColorKey;
  std::string blockColorHex;
  std::string inheritedBlockColorHex;
};

static std::vector<NativeSongWindow> g_nativeSongWindows;
static bool g_nativeCurrentTransportPlaying = false;
static std::string g_nativeCurrentPlayingId;
static double g_nativeCurrentPlayPosition = 0.0;

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
  oss << "\"isHashParent\":" << (item.isHashParent ? "true" : "false") << ",";
  oss << "\"isHashChild\":" << (item.isHashChild ? "true" : "false") << ",";
  oss << "\"isFamilyItem\":" << ((item.isHashParent || item.isHashChild) ? "true" : "false") << ",";
  oss << "\"familyRole\":" << nativeJsonString(item.isHashParent ? "parent" : (item.isHashChild ? "child" : "none")) << ",";
  oss << "\"parentId\":" << nativeJsonString(item.parentId) << ",";
  oss << "\"parentName\":" << nativeJsonString(item.parentName) << ",";
  oss << "\"blockColorKey\":" << nativeJsonString(item.blockColorKey) << ",";
  oss << "\"blockColorHex\":" << nativeJsonString(item.blockColorHex) << ",";
  oss << "\"bridgeBlockColorHex\":" << nativeJsonString(item.blockColorHex) << ",";
  oss << "\"inheritedBlockColorHex\":" << nativeJsonString(item.inheritedBlockColorHex) << ",";
  oss << "\"textColorHex\":" << nativeJsonString(item.inheritedBlockColorHex) << ",";
  oss << "\"finalTextColorHex\":" << nativeJsonString(item.inheritedBlockColorHex) << ",";
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
    const std::string projectPath = normalizeSlashes(pathBuf ? pathBuf : "");
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

static std::vector<NativeSongWindow> nativeCollectProjectSongs(ReaProject* project, std::string& markersJsonOut)
{
  std::vector<NativeSongWindow> songs;
  struct MarkerEntry { bool region=false; double start=0.0; double end=0.0; std::string name; int number=0; int color=0; };
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

  for (const auto& r : regions) {
    const std::string clean = nativeCleanSongName(r.name);
    if (clean.empty()) continue;
    const bool isHashParent = nativeTrim(r.name).rfind("--", 0) == 0;
    NativeSongWindow parent;
    parent.id = std::to_string(r.number);
    parent.name = clean;
    parent.type = isHashParent ? "hash_parent" : "song";
    parent.start = r.start;
    parent.end = r.end;
    parent.sourceNumber = r.number;
    parent.isHashParent = isHashParent;
    songs.push_back(parent);

    if (isHashParent) {
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
        if (!child.name.empty()) songs.push_back(child);
      }
    }
  }

  std::sort(songs.begin(), songs.end(), [](const NativeSongWindow& a, const NativeSongWindow& b){
    if (a.start == b.start) return a.end < b.end;
    return a.start < b.start;
  });
  return songs;
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
    oss << nativeSongToJson(s, index++);
  }
  oss << "]";
  return oss.str();
}

static std::string nativeBuildPlaylistsJson(ReaProject* project, const std::vector<NativeSongWindow>& projectSongs, int& activePlaylistIndexOut, std::string& activePlaylistNameOut)
{
  activePlaylistIndexOut = 1;
  activePlaylistNameOut = "Músicas";
  const std::string savedPlaylistName = nativeTrim(nativeGetProjExtStateString(project, kPlaylistExtSection, "LAST_PLAYLIST_NAME_V1"));
  const std::string data = nativeGetProjExtStateString(project, kPlaylistExtSection, kPlaylistExtKey);
  if (data.empty()) {
    std::ostringstream fallback;
    fallback << "[{\"id\":\"1\",\"name\":\"Músicas\",\"active\":true,\"current\":true,\"songs\":" << nativeBuildRegionsJson(projectSongs) << "}]";
    return fallback.str();
  }

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
      if (activeThis) { activePlaylistIndexOut = playlistIndex; activePlaylistNameOut = name; }
      out << "{\"id\":" << nativeJsonString(std::to_string(playlistIndex)) << ",\"name\":" << nativeJsonString(name) << ",\"active\":" << (activeThis ? "true" : "false") << ",\"current\":" << (activeThis ? "true" : "false") << ",\"songs\":[";
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

      if (itemIndex > 0) out << ",";
      out << nativeSongToJson(item, itemIndex + 1);
      ++itemIndex;
    } else if (tag == "END" && inPlaylist) {
      out << "]}";
      inPlaylist = false;
    }
  }
  if (inPlaylist) out << "]}";
  out << "]";
  return out.str();
}

static std::string nativeFindPlayingId(const std::vector<NativeSongWindow>& songs, double pos, std::string& nameOut, double& startOut, double& endOut)
{
  std::string id;
  double bestLen = 1e99;
  for (const auto& s : songs) {
    if (s.isBlock) continue;
    if (pos >= s.start - 0.0005 && pos < s.end - 0.0005) {
      const double len = std::max(0.0, s.end - s.start);
      // Prioriza filhos/trechos menores quando parent e child cobrem o mesmo ponto.
      if (len < bestLen) {
        bestLen = len;
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

static std::string nativeBuildMixerTracksJson(ReaProject* project, bool groupsOnly)
{
  std::ostringstream oss;
  oss << "[";
  if (!CountTracks_ptr || !GetTrack_ptr) { oss << "]"; return oss.str(); }
  bool first = true;
  const int count = CountTracks_ptr(project);
  for (int i = 0; i < count; ++i) {
    MediaTrack* track = GetTrack_ptr(project, i);
    if (!track) continue;
    const double folderDepth = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "I_FOLDERDEPTH") : 0.0;
    if (groupsOnly && static_cast<int>(folderDepth) <= 0) continue;
    const std::string guid = nativeTrackGuid(track, i);
    const std::string name = nativeTrackName(track, i);
    const double vol = GetMediaTrackInfo_Value_ptr ? GetMediaTrackInfo_Value_ptr(track, "D_VOL") : 1.0;
    const bool mute = GetMediaTrackInfo_Value_ptr ? (GetMediaTrackInfo_Value_ptr(track, "B_MUTE") > 0.5) : false;
    const bool solo = GetMediaTrackInfo_Value_ptr ? (GetMediaTrackInfo_Value_ptr(track, "I_SOLO") > 0.5) : false;
    if (!first) oss << ",";
    first = false;
    oss << "{";
    oss << "\"id\":" << nativeJsonString(guid) << ",";
    oss << "\"guid\":" << nativeJsonString(guid) << ",";
    oss << "\"index\":" << (i + 1) << ",";
    oss << "\"name\":" << nativeJsonString(name) << ",";
    oss << "\"label\":" << nativeJsonString(name) << ",";
    oss << "\"volume\":" << nativeNumber(vol) << ",";
    oss << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(vol), 6) << ",";
    oss << "\"db\":" << nativeNumber(nativeVolumeToDb(vol), 3) << ",";
    oss << "\"displayScale\":\"db\",";
    oss << "\"mute\":" << (mute ? "true" : "false") << ",";
    oss << "\"solo\":" << (solo ? "true" : "false") << ",";
    oss << "\"folderDepth\":" << static_cast<int>(folderDepth);
    oss << "}";
  }
  oss << "]";
  return oss.str();
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

static std::string nativeBuildMixerJson(ReaProject* project)
{
  std::ostringstream oss;
  const std::string tracks = nativeBuildMixerTracksJson(project, false);
  const std::string groups = nativeBuildMixerTracksJson(project, true);
  const std::string master = nativeBuildMixerMasterJson(project);
  oss << "{\"tracks\":" << tracks << ",\"groups\":" << groups << ",\"master\":" << master << "}";
  return oss.str();
}

static std::string nativeBuildPremixSongsJson(const std::vector<NativeSongWindow>& songs)
{
  std::ostringstream oss;
  oss << "[";
  bool first = true;
  int index = 1;
  for (const auto& s : songs) {
    if (s.isBlock || s.isHashParent) continue;
    if (!first) oss << ",";
    first = false;
    oss << nativeSongToJson(s, index++);
  }
  oss << "]";
  return oss.str();
}

static std::string nativeBuildPremixTracksJson(ReaProject* project, const NativeSongWindow* selected)
{
  std::ostringstream oss;
  oss << "[";
  if (!selected || !CountTracks_ptr || !GetTrack_ptr || !GetTrackNumMediaItems_ptr || !GetTrackMediaItem_ptr || !GetMediaItemInfo_Value_ptr) {
    oss << "]"; return oss.str();
  }

  struct PremixItemRow {
    std::string guid;
    std::string name;
    std::string trackName;
    double volume = 1.0;
    bool muted = false;
    int fxCount = 0;
    bool fxEnabled = false;
    bool drawer = false;
  };

  auto collectRows = [&](bool overlapFallback) {
    std::vector<PremixItemRow> rows;
    const int trackCount = CountTracks_ptr(project);
    for (int ti = 0; ti < trackCount; ++ti) {
      MediaTrack* track = GetTrack_ptr(project, ti);
      if (!track) continue;
      const std::string trackName = nativeTrackName(track, ti);
      const int itemCount = GetTrackNumMediaItems_ptr(track);
      for (int ii = 0; ii < itemCount; ++ii) {
        MediaItem* item = GetTrackMediaItem_ptr(track, ii);
        const bool belongs = overlapFallback
          ? nativeItemOverlapsRange(item, selected->start, selected->end)
          : nativeItemStartsInRange(item, selected->start, selected->end);
        if (!belongs) continue;
        PremixItemRow row;
        row.guid = nativeMediaItemGuid(item, std::string("item_") + std::to_string(ti + 1) + "_" + std::to_string(ii + 1));
        row.name = nativeTakeName(item, trackName);
        row.trackName = trackName;
        row.volume = GetMediaItemInfo_Value_ptr(item, "D_VOL");
        row.muted = GetMediaItemInfo_Value_ptr(item, "B_MUTE") > 0.5;
        row.fxCount = nativeTakeFxCount(item);
        row.fxEnabled = nativeTakeAnyFxEnabled(item);
        row.drawer = false;
        rows.push_back(row);
      }
    }
    return rows;
  };

  std::vector<PremixItemRow> rows = collectRows(false);
  if (rows.empty()) {
    // Fallback: se nenhum item começar dentro do trecho, mostra os itens que atravessam o trecho.
    // Isso evita Premix vazio em projetos onde os áudios começam antes do marker/região.
    rows = collectRows(true);
  }

  bool first = true;
  int outIndex = 1;
  for (const auto& row : rows) {
    if (!first) oss << ",";
    first = false;
    oss << "{";
    oss << "\"id\":" << nativeJsonString(row.guid) << ",";
    oss << "\"guid\":" << nativeJsonString(row.guid) << ",";
    oss << "\"itemId\":" << nativeJsonString(row.guid) << ",";
    oss << "\"songId\":" << nativeJsonString(selected->id) << ",";
    oss << "\"selectedSongId\":" << nativeJsonString(selected->id) << ",";
    oss << "\"index\":" << outIndex++ << ",";
    oss << "\"name\":" << nativeJsonString(row.name) << ",";
    oss << "\"label\":" << nativeJsonString(row.name) << ",";
    oss << "\"trackName\":" << nativeJsonString(row.trackName) << ",";
    oss << "\"volume\":" << nativeNumber(row.volume) << ",";
    oss << "\"volumeRatio\":" << nativeNumber(nativeVolumeToRatio(row.volume), 6) << ",";
    oss << "\"liveVolumeRatio\":" << nativeNumber(nativeVolumeToRatio(row.volume), 6) << ",";
    oss << "\"db\":" << nativeNumber(nativeVolumeToDb(row.volume), 3) << ",";
    oss << "\"displayScale\":\"db\",";
    oss << "\"mute\":" << (row.muted ? "true" : "false") << ",";
    oss << "\"muted\":" << (row.muted ? "true" : "false") << ",";
    oss << "\"hasFx\":" << (row.fxCount > 0 ? "true" : "false") << ",";
    oss << "\"fxCount\":" << row.fxCount << ",";
    oss << "\"fxEnabled\":" << (row.fxEnabled ? "true" : "false") << ",";
    oss << "\"fxOn\":" << (row.fxEnabled ? "true" : "false");
    oss << "}";
  }
  oss << "]";
  return oss.str();
}

static std::string nativeBuildPremixTracksBySongIdJson(ReaProject* project, const std::vector<NativeSongWindow>& songs)
{
  std::ostringstream oss;
  oss << "{";
  bool first = true;
  for (const auto& s : songs) {
    if (s.isBlock || s.isHashParent) continue;
    if (!first) oss << ",";
    first = false;
    oss << nativeJsonString(s.id) << ":" << nativeBuildPremixTracksJson(project, &s);
  }
  oss << "}";
  return oss.str();
}

static std::string nativeBuildPremixJson(ReaProject* project, const std::vector<NativeSongWindow>& songs)
{
  std::string selectedId;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    selectedId = g_nativePremixSelectedSongId;
  }
  const NativeSongWindow* selected = nativeFindSongById(songs, selectedId);
  std::ostringstream oss;
  oss << "{";
  oss << "\"mode\":\"item\",\"itemMode\":true,\"globalEnabled\":false,\"bypassEnabled\":false,";
  oss << "\"selectedSongId\":" << nativeJsonString(selected ? selected->id : selectedId) << ",";
  oss << "\"selectedPremixEnabled\":true,\"selectedCanEdit\":true,";
  oss << "\"songs\":" << nativeBuildPremixSongsJson(songs) << ",";
  oss << "\"tracks\":" << nativeBuildPremixTracksJson(project, selected) << ",";
  oss << "\"tracksBySongId\":" << nativeBuildPremixTracksBySongIdJson(project, songs) << ",";
  oss << "\"tracksByRegionId\":" << nativeBuildPremixTracksBySongIdJson(project, songs) << ",";
  oss << "\"groups\":[]";
  oss << "}";
  return oss.str();
}

static void nativeRebuildState(bool forceSnapshot)
{
  if (!EnumProjects_ptr) return;
  char activePathBuf[2048] = "";
  ReaProject* activeProject = getCurrentProject(activePathBuf, static_cast<int>(sizeof(activePathBuf)));
  std::string activeProjectName;
  std::string activeProjectPath;
  int activeProjectIndex = 0;
  const std::string projectsJson = nativeBuildProjectsJson(activeProject, activeProjectName, activeProjectPath, activeProjectIndex);
  if (activeProjectName.empty()) activeProjectName = nativeBasenameNoRpp(activePathBuf ? activePathBuf : "");
  if (activeProjectPath.empty()) activeProjectPath = normalizeSlashes(activePathBuf ? activePathBuf : "");

  std::string markersJson;
  std::vector<NativeSongWindow> songs = nativeCollectProjectSongs(activeProject, markersJson);

  int playState = GetPlayStateEx_ptr ? GetPlayStateEx_ptr(activeProject) : 0;
  const bool playing = (playState & 1) == 1 || (playState & 4) == 4;
  const double playPos = GetPlayPositionEx_ptr ? GetPlayPositionEx_ptr(activeProject) : 0.0;
  std::string playingName;
  double songStart = 0.0;
  double songEnd = 0.0;
  const std::string playingId = playing ? nativeFindPlayingId(songs, playPos, playingName, songStart, songEnd) : "";
  const double duration = std::max(0.0, songEnd - songStart);
  const double elapsed = playingId.empty() ? 0.0 : std::max(0.0, std::min(duration, playPos - songStart));
  const double remaining = playingId.empty() ? 0.0 : std::max(0.0, duration - elapsed);
  const double progress = duration > 0.0 ? std::min(1.0, std::max(0.0, elapsed / duration)) : 0.0;

  g_nativeCurrentTransportPlaying = playing;
  g_nativeCurrentPlayingId = playingId;
  g_nativeCurrentPlayPosition = playPos;

  const std::string regionsJson = nativeBuildRegionsJson(songs);
  int activePlaylistIndex = 1;
  std::string activePlaylistName = "Músicas";
  const std::string playlistsJson = nativeBuildPlaylistsJson(activeProject, songs, activePlaylistIndex, activePlaylistName);
  const std::string mixerJson = nativeBuildMixerJson(activeProject);
  const std::string premixJson = nativeBuildPremixJson(activeProject, songs);
  const std::string nowIso = nativeIsoNow();
  const bool appActive = g_nativeLastDirectorHeartbeat.time_since_epoch().count() != 0 &&
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - g_nativeLastDirectorHeartbeat).count() < 12;

  std::string luaLiveRaw;
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    luaLiveRaw = g_nativeLuaLiveFragment;
  }
  if (GetExtState_ptr) {
    const char* extLuaLive = GetExtState_ptr(kNativeExtStateSection, kNativeLuaLiveExtKey);
    if (extLuaLive && *extLuaLive) luaLiveRaw = extLuaLive;
  }
  std::string luaFragment = nativeStripJsonObjectBraces(luaLiveRaw);

  std::ostringstream json;
  json << "{";
  json << "\"ok\":true,";
  json << "\"nativeBridge\":true,";
  json << "\"bridgeVersion\":2,";
  json << "\"connected\":true,";
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
  json << "\"mixerTracks\":" << nativeBuildMixerTracksJson(activeProject, false) << ",";
  json << "\"mixerGroups\":" << nativeBuildMixerTracksJson(activeProject, true) << ",";
  json << "\"mixerMaster\":" << nativeBuildMixerMasterJson(activeProject) << ",";
  json << "\"currentPlaylistName\":" << nativeJsonString(activePlaylistName) << ",";
  json << "\"activePlaylistName\":" << nativeJsonString(activePlaylistName) << ",";
  json << "\"activePlaylistId\":" << nativeJsonString(std::to_string(activePlaylistIndex)) << ",";
  json << "\"currentPlaylistIndex\":" << activePlaylistIndex << ",";
  json << "\"playing\":" << (playing && !playingId.empty() ? "true" : "false") << ",";
  json << "\"isPlaying\":" << (playing && !playingId.empty() ? "true" : "false") << ",";
  json << "\"transportPlaying\":" << (playing ? "true" : "false") << ",";
  json << "\"playingId\":" << (playingId.empty() ? "null" : nativeJsonString(playingId)) << ",";
  json << "\"playingSongId\":" << (playingId.empty() ? "null" : nativeJsonString(playingId)) << ",";
  json << "\"currentSongId\":" << (playingId.empty() ? "null" : nativeJsonString(playingId)) << ",";
  json << "\"currentSongName\":" << nativeJsonString(playingName) << ",";
  json << "\"playingSongName\":" << nativeJsonString(playingName) << ",";
  json << "\"songName\":" << nativeJsonString(playingName) << ",";
  json << "\"musicName\":" << nativeJsonString(playingName) << ",";
  json << "\"playPosition\":" << nativeNumber(playPos) << ",";
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
  json << "\"currentSongProgress\":" << nativeNumber(progress, 6);
  if (!luaFragment.empty()) json << "," << luaFragment;
  // Native premix vem por ultimo para sobrepor qualquer fragmento antigo do Lua.
  json << ",\"premix\":" << premixJson;
  json << "}";

  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeStateJson = json.str();
    g_nativeSongWindows = std::move(songs);
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
    for (size_t i = 0; i < g_nativeCommandHistory.size(); ++i) {
      if (i) oss << ",";
      oss << g_nativeCommandHistory[i];
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
  const bool liveDue = g_nativeLastLiveBuild.time_since_epoch().count() == 0 ||
    std::chrono::duration_cast<std::chrono::milliseconds>(now - g_nativeLastLiveBuild).count() >= kNativeBridgeLiveIntervalMs;
  const bool snapshotDue = g_nativeLastSnapshotBuild.time_since_epoch().count() == 0 ||
    std::chrono::duration_cast<std::chrono::milliseconds>(now - g_nativeLastSnapshotBuild).count() >= kNativeBridgeSnapshotIntervalMs;
  if (!liveDue && !snapshotDue) return;
  g_nativeLastLiveBuild = now;
  if (snapshotDue) g_nativeLastSnapshotBuild = now;
  nativeRebuildState(snapshotDue);
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

static char g_apiDefNativePullCommand[] =
  "bool\0char*,int\0commandJsonOutNeedBig,commandJsonOutNeedBig_sz\0Pull one pending VS Hook command from the native bridge.";
static char g_apiDefNativeSetLuaState[] =
  "bool\0const char*\0jsonFragment\0Send small Lua-only live state fields to the VS Hook native bridge.";
static char g_apiDefNativeGetState[] =
  "bool\0char*,int\0stateJsonOutNeedBig,stateJsonOutNeedBig_sz\0Read the current VS Hook native bridge state.";

static bool registerNativeBridgeApi()
{
  if (!plugin_register_ptr) return false;
  bool ok = true;
  ok = (plugin_register_ptr("API_VS_Hook_Native_PullCommand", reinterpret_cast<void*>(&VS_Hook_Native_PullCommand)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_PullCommand", reinterpret_cast<void*>(g_apiDefNativePullCommand)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_SetLuaState", reinterpret_cast<void*>(&VS_Hook_Native_SetLuaState)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_SetLuaState", reinterpret_cast<void*>(g_apiDefNativeSetLuaState)) != 0) && ok;
  ok = (plugin_register_ptr("API_VS_Hook_Native_GetState", reinterpret_cast<void*>(&VS_Hook_Native_GetState)) != 0) && ok;
  ok = (plugin_register_ptr("APIdef_VS_Hook_Native_GetState", reinterpret_cast<void*>(g_apiDefNativeGetState)) != 0) && ok;
  return ok;
}

static void unregisterNativeBridgeApi()
{
  if (!plugin_register_ptr) return;
  plugin_register_ptr("-APIdef_VS_Hook_Native_GetState", reinterpret_cast<void*>(g_apiDefNativeGetState));
  plugin_register_ptr("-API_VS_Hook_Native_GetState", reinterpret_cast<void*>(&VS_Hook_Native_GetState));
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

static void nativeHandleClient(native_socket_t client)
{
  const std::string req = nativeReadHttpRequest(client);
  if (req.empty()) { nativeCloseSocket(client); return; }
  const std::string path = nativeRequestPath(req);
  std::string body;

  if (nativeStartsWith(req, "OPTIONS ")) {
    body = nativeHttpResponse(200, "{}", "application/json; charset=utf-8");
  } else if (path == "/state" || path == "/state.json" || path == "/snapshot" || path == "/projects" || path == "/projects.json") {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    body = nativeHttpResponse(200, g_nativeStateJson.empty() ? std::string("{\"ok\":false,\"connected\":false}") : g_nativeStateJson);
  } else if (path == "/command" && nativeRequestIsPost(req)) {
    const std::string commandBody = nativeTrim(nativeRequestBody(req));
    if (!commandBody.empty()) {
      const std::string commandType = nativeJsonExtractString(commandBody, "type");
      if (commandType == "app_heartbeat") {
        g_nativeLastDirectorHeartbeat = std::chrono::steady_clock::now();
      }
      if (commandType == "premix_item_focus_song" || commandType == "premix_focus_song" || commandType == "premix_item_set_volume" || commandType == "premix_item_toggle_mute" || commandType == "premix_item_toggle_fx") {
        std::string selected = nativeJsonExtractString(commandBody, "selectedRegionId");
        if (selected.empty()) selected = nativeJsonExtractString(commandBody, "songId");
        if (selected.empty()) selected = nativeJsonExtractString(commandBody, "regionId");
        if (selected.empty()) selected = nativeJsonExtractString(commandBody, "id");
        if (!selected.empty()) {
          std::lock_guard<std::mutex> lock(g_nativeMutex);
          g_nativePremixSelectedSongId = selected;
        }
      }
      std::lock_guard<std::mutex> lock(g_nativeMutex);
      if (g_nativeCommandQueue.size() > 200) g_nativeCommandQueue.pop_front();
      g_nativeCommandQueue.push_back(commandBody);
      g_nativeCommandHistory.push_back(commandBody);
      if (g_nativeCommandHistory.size() > 120) {
        g_nativeCommandHistory.erase(g_nativeCommandHistory.begin(), g_nativeCommandHistory.begin() + static_cast<long>(g_nativeCommandHistory.size() - 120));
      }
      ++g_nativeCommandSequence;
    }
    body = nativeHttpResponse(200, "{\"ok\":true,\"nativeBridge\":true}");
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

static void startupTimer()
{
  nativeBridgeTick();
  ++g_state.startupTimerTicks;

  const std::string projectSignature = getCurrentProjectSignature();
  if (projectSignature != g_state.activeProjectSignature) {
    g_state.activeProjectSignature = projectSignature;
    g_state.projectStableTicks = 0;
  } else {
    ++g_state.projectStableTicks;
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
  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  Main_OnCommand_ptr = reinterpret_cast<Main_OnCommand_t>(rec->GetFunc("Main_OnCommand"));
  GetToggleCommandState_ptr = reinterpret_cast<GetToggleCommandState_t>(rec->GetFunc("GetToggleCommandState"));
  GetToggleCommandStateEx_ptr = reinterpret_cast<GetToggleCommandStateEx_t>(rec->GetFunc("GetToggleCommandStateEx"));
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));
  GetExtState_ptr = reinterpret_cast<GetExtState_t>(rec->GetFunc("GetExtState"));
  SetExtState_ptr = reinterpret_cast<SetExtState_t>(rec->GetFunc("SetExtState"));
  EnumProjects_ptr = reinterpret_cast<EnumProjects_t>(rec->GetFunc("EnumProjects"));
  GetProjExtState_ptr = reinterpret_cast<GetProjExtState_t>(rec->GetFunc("GetProjExtState"));
  SetProjExtState_ptr = reinterpret_cast<SetProjExtState_t>(rec->GetFunc("SetProjExtState"));
  CountProjectMarkers_ptr = reinterpret_cast<CountProjectMarkers_t>(rec->GetFunc("CountProjectMarkers"));
  EnumProjectMarkers3_ptr = reinterpret_cast<EnumProjectMarkers3_t>(rec->GetFunc("EnumProjectMarkers3"));
  GetPlayStateEx_ptr = reinterpret_cast<GetPlayStateEx_t>(rec->GetFunc("GetPlayStateEx"));
  GetPlayPositionEx_ptr = reinterpret_cast<GetPlayPositionEx_t>(rec->GetFunc("GetPlayPositionEx"));
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
  TakeFX_GetCount_ptr = reinterpret_cast<TakeFX_GetCount_t>(rec->GetFunc("TakeFX_GetCount"));
  TakeFX_GetEnabled_ptr = reinterpret_cast<TakeFX_GetEnabled_t>(rec->GetFunc("TakeFX_GetEnabled"));
  TakeFX_SetEnabled_ptr = reinterpret_cast<TakeFX_SetEnabled_t>(rec->GetFunc("TakeFX_SetEnabled"));
  GetMediaTrackInfo_Value_ptr = reinterpret_cast<GetMediaTrackInfo_Value_t>(rec->GetFunc("GetMediaTrackInfo_Value"));
  GetTrackGUID_ptr = reinterpret_cast<GetTrackGUID_t>(rec->GetFunc("GetTrackGUID"));
  guidToString_ptr = reinterpret_cast<guidToString_t>(rec->GetFunc("guidToString"));

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

  for (ScriptEntry& script : g_scripts) {
    script.commandId = plugin_register_ptr("custom_action", (void*)&script.action);
    if (script.commandId != 0) {
      hasRegisteredAction = true;
    }
  }

  registerClipboardApi();
  registerNativeBridgeApi();
  startNativeBridgeServer();

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
  g_state.lastLaunchedMode.clear();
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

  if (g_state.timerRegistered) {
    plugin_register_ptr("-timer", reinterpret_cast<void*>(&startupTimer));
    g_state.timerRegistered = false;
  }

  stopNativeBridgeServer();
  unregisterNativeBridgeApi();
  unregisterClipboardApi();

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
  g_state.lastLaunchedMode.clear();
  g_state.initialized = false;
}

} // namespace vshook

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
  REAPER_PLUGIN_HINSTANCE instance,
  reaper_plugin_info_t* rec)
{
  (void)instance;

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

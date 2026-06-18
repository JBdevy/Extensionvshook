#define SWELL_PROVIDED_BY_APP
#include "reaper_plugin.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/stat.h>
#endif

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using plugin_getapi_t = void* (*)(const char*);
using ShowMessageBox_t = int (*)(const char*, const char*, int);
using GetResourcePath_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);
using GetExtState_t = const char* (*)(const char*, const char*);
using SetExtState_t = void (*)(const char*, const char*, const char*, bool);

static plugin_register_t plugin_register_ptr = nullptr;
static plugin_getapi_t plugin_getapi_ptr = nullptr;
static ShowMessageBox_t ShowMessageBox_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;
static GetExtState_t GetExtState_ptr = nullptr;
static SetExtState_t SetExtState_ptr = nullptr;

static const char* kExtStateSection = "VS_HOOK_LOADER";
static const char* kAutoOpenKey = "AUTO_OPEN_VSHOOK";

struct ScriptEntry {
  custom_action_register_t action;
  const char* displayName;
  const char* fileName;
  std::string scriptPath;
  int scriptCommandId = 0;
  int commandId = 0;
};

static ScriptEntry g_scripts[] = {
  {
    { 0, "VSHOOKRUN", "VS Hook APP", nullptr },
    "VS Hook APP",
    "VS Hook.lua"
  },
  {
    { 0, "VSHOOKLYRICS", "Hook Lyrics", nullptr },
    "Hook Lyrics",
    "Hook lyrics.lua"
  }
};

struct State {
  bool initialized = false;
  bool commandHookRegistered = false;
  bool commandHook2Registered = false;
  bool menuHookRegistered = false;
  bool timerRegistered = false;
  int startupTimerTicks = 0;
} g_state;

static custom_action_register_t g_toggleAutoOpenAction = { 0, "VSHOOKAUTOOPEN", "Abrir VS Hook junto com o REAPER", nullptr };
static int g_toggleAutoOpenCommandId = 0;

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

static void runScript(ScriptEntry& script)
{
  if (!ensureScriptRegistered(script)) {
    return;
  }

  if (Main_OnCommand_ptr && script.scriptCommandId != 0) {
    Main_OnCommand_ptr(script.scriptCommandId, 0);
  }
}


static bool getAutoOpenEnabled()
{
  if (!GetExtState_ptr) return false;

  const char* value = GetExtState_ptr(kExtStateSection, kAutoOpenKey);
  return value && std::strcmp(value, "1") == 0;
}

static void setAutoOpenEnabled(bool enabled)
{
  if (!SetExtState_ptr) {
    showDiagnostic("REAPER nao entregou a API SetExtState para salvar essa configuracao.");
    return;
  }

  SetExtState_ptr(kExtStateSection, kAutoOpenKey, enabled ? "1" : "0", true);
}

static void toggleAutoOpen()
{
  const bool enabled = !getAutoOpenEnabled();
  setAutoOpenEnabled(enabled);

  showDiagnostic(enabled
    ? "VS Hook configurado para abrir junto com o REAPER."
    : "VS Hook nao vai mais abrir junto com o REAPER.");
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

  if (g_toggleAutoOpenCommandId != 0 && command == g_toggleAutoOpenCommandId) {
    toggleAutoOpen();
    return true;
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

  if (g_toggleAutoOpenCommandId != 0 && command == g_toggleAutoOpenCommandId) {
    toggleAutoOpen();
    return true;
  }

  ScriptEntry* script = findScriptByCommand(command);
  if (script) {
    runScript(*script);
    return true;
  }

  return false;
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  if (!menustr || !hMenu) return;
  if (flag != 0) return;

  if (std::strcmp(menustr, "Main extensions") == 0) {
    // Insere de tras para frente porque cada item entra na posicao 0.
    if (g_toggleAutoOpenCommandId != 0) {
      MENUITEMINFO mi = { sizeof(MENUITEMINFO), };
      mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
      mi.fType = MFT_STRING;
      mi.fState = getAutoOpenEnabled() ? MFS_CHECKED : MFS_UNCHECKED;
      mi.dwTypeData = (char*)"Abrir junto com o REAPER";
      mi.wID = g_toggleAutoOpenCommandId;
      InsertMenuItem(hMenu, 0, true, &mi);

      MENUITEMINFO sep = { sizeof(MENUITEMINFO), };
      sep.fMask = MIIM_TYPE;
      sep.fType = MFT_SEPARATOR;
      InsertMenuItem(hMenu, 0, true, &sep);
    }

    for (int i = static_cast<int>(sizeof(g_scripts) / sizeof(g_scripts[0])) - 1; i >= 0; --i) {
      if (g_scripts[i].commandId == 0) continue;

      MENUITEMINFO mi = { sizeof(MENUITEMINFO), };
      mi.fMask = MIIM_TYPE | MIIM_ID;
      mi.fType = MFT_STRING;
      mi.dwTypeData = (char*)g_scripts[i].displayName;
      mi.wID = g_scripts[i].commandId;
      InsertMenuItem(hMenu, 0, true, &mi);
    }
  }
}


static void startupTimer()
{
  ++g_state.startupTimerTicks;

  // Espera alguns ciclos para o REAPER terminar de montar a interface.
  if (g_state.startupTimerTicks < 30) return;

  if (g_state.timerRegistered && plugin_register_ptr) {
    plugin_register_ptr("-timer", reinterpret_cast<void*>(&startupTimer));
    g_state.timerRegistered = false;
  }

  if (getAutoOpenEnabled()) {
    for (ScriptEntry& script : g_scripts) {
      runScript(script);
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
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));
  GetExtState_ptr = reinterpret_cast<GetExtState_t>(rec->GetFunc("GetExtState"));
  SetExtState_ptr = reinterpret_cast<SetExtState_t>(rec->GetFunc("SetExtState"));

  if (!plugin_register_ptr) {
    showDiagnostic("VS Hook Loader nao carregou: plugin_register indisponivel.");
    return false;
  }

  return true;
}

static void initialize()
{
  if (g_state.initialized) return;

  g_toggleAutoOpenCommandId = plugin_register_ptr("custom_action", (void*)&g_toggleAutoOpenAction);

  bool hasRegisteredAction = g_toggleAutoOpenCommandId != 0;
  for (ScriptEntry& script : g_scripts) {
    script.commandId = plugin_register_ptr("custom_action", (void*)&script.action);
    if (script.commandId != 0) {
      hasRegisteredAction = true;
    }
  }

  if (hasRegisteredAction) {
    if (plugin_register_ptr("hookcommand", reinterpret_cast<void*>(&hookCommand))) {
      g_state.commandHookRegistered = true;
    }

    if (plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2))) {
      g_state.commandHook2Registered = true;
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

  if (getAutoOpenEnabled()) {
    g_state.startupTimerTicks = 0;
    if (plugin_register_ptr("timer", reinterpret_cast<void*>(&startupTimer))) {
      g_state.timerRegistered = true;
    } else {
      runScript(g_scripts[0]);
    }
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

  if (g_state.menuHookRegistered) {
    plugin_register_ptr("-hookcustommenu", reinterpret_cast<void*>(&menuHook));
    g_state.menuHookRegistered = false;
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

  if (g_toggleAutoOpenCommandId != 0) {
    plugin_register_ptr("-custom_action", (void*)&g_toggleAutoOpenAction);
    g_toggleAutoOpenCommandId = 0;
  }

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

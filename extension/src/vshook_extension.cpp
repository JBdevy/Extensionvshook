#define SWELL_PROVIDED_BY_APP
#include "reaper_plugin.h"
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using GetResourcePath_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);

static plugin_register_t plugin_register_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;

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
} g_state;

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

static bool fileExists(const std::string& path)
{
#ifdef _WIN32
  const std::wstring widePath = utf8ToWide(path);
  if (widePath.empty()) return false;

  const DWORD attrs = GetFileAttributesW(widePath.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  std::ifstream f(path, std::ios::binary);
  return f.good();
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
#ifdef _WIN32
  // Windows: usa o caminho publico do instalador, sem depender do perfil do usuario.
  // Isso evita falhas quando o perfil do Windows tem acentos, ex: Produção.
  return {
    std::string("C:/Users/Public/VS Hook APP/") + fileName
  };
#else
  const char* resource = GetResourcePath_ptr ? GetResourcePath_ptr() : "";
  const std::string base = normalizeSlashes(resource ? resource : "");

  return {
    base + "/Scripts/VS Hook APP/" + fileName
  };
#endif
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
    return false;
  }

  if (!resolveScriptPath(script)) {
    return false;
  }

  const int commandId = AddRemoveReaScript_ptr(true, 0, script.scriptPath.c_str(), true);
  if (commandId == 0) {
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

static bool loadApi(reaper_plugin_info_t* rec)
{
  if (!rec || !rec->Register || !rec->GetFunc) return false;

  plugin_register_ptr = rec->Register;
  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  Main_OnCommand_ptr = reinterpret_cast<Main_OnCommand_t>(rec->GetFunc("Main_OnCommand"));
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));

  return plugin_register_ptr != nullptr;
}

static void initialize()
{
  if (g_state.initialized) return;

  bool hasRegisteredAction = false;
  for (ScriptEntry& script : g_scripts) {
    script.commandId = plugin_register_ptr("custom_action", (void*)&script.action);
    if (script.commandId != 0) {
      hasRegisteredAction = true;
    }
  }

  if (hasRegisteredAction) {
    plugin_register_ptr("hookcommand", reinterpret_cast<void*>(&hookCommand));
    g_state.commandHookRegistered = true;

    plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHook2Registered = true;
  }

  plugin_register_ptr("hookcustommenu", reinterpret_cast<void*>(&menuHook));
  g_state.menuHookRegistered = true;

  if (AddExtensionsMainMenu_ptr) {
    AddExtensionsMainMenu_ptr();
  }

  g_state.initialized = true;
}

static void shutdown()
{
  if (!plugin_register_ptr) return;

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

#include "reaper_plugin.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <fstream>
#include <string>
#include <vector>

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using GetResourcePath_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);
using ShowConsoleMsg_t = void (*)(const char*);

static plugin_register_t plugin_register_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;
static ShowConsoleMsg_t ShowConsoleMsg_ptr = nullptr;

static REAPER_PLUGIN_HINSTANCE g_hInstance = nullptr;

struct State {
  std::string resourcePath;
  std::string scriptPath;
  int scriptCommandId = 0;
  int customRunCommandId = 0;
  bool initialized = false;
  bool menuHookRegistered = false;
  bool actionHookRegistered = false;
} g_state;

static custom_action_register_t g_runAction {
  0,
  const_cast<char*>("VSHookRun"),
  const_cast<char*>("VS Hook"),
  nullptr
};

static void logMessage(const std::string& text)
{
  const std::string line = "[VS Hook] " + text + "\n";
  if(ShowConsoleMsg_ptr) {
    ShowConsoleMsg_ptr(line.c_str());
  }
}

static bool fileExists(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

static std::string normalizeSlashes(std::string path)
{
  for(char& c : path) {
    if(c == '\\') c = '/';
  }
  return path;
}

static std::vector<std::string> buildScriptCandidates()
{
  const char* resource = GetResourcePath_ptr ? GetResourcePath_ptr() : "";
  const std::string base = normalizeSlashes(resource ? resource : "");

  return {
    base + "/Scripts/VS Hook APP/VS Hook.lua",
    base + "/Scripts/VS Hook/VS Hook.lua",
    base + "/Scripts/VS Hook Server/VS Hook.lua"
  };
}

static std::string buildDefaultScriptPath()
{
  const auto candidates = buildScriptCandidates();
  return candidates.empty() ? std::string() : candidates.front();
}

static bool resolveScriptPath()
{
  const auto candidates = buildScriptCandidates();
  for(const auto& candidate : candidates) {
    if(fileExists(candidate)) {
      g_state.scriptPath = candidate;
      return true;
    }
  }

  g_state.scriptPath.clear();
  return false;
}

static bool ensureScriptRegistered()
{
  if(g_state.scriptCommandId != 0) {
    return true;
  }

  if(!AddRemoveReaScript_ptr) {
    logMessage("API AddRemoveReaScript nao disponivel. A extensao carregou, mas nao conseguiu registrar o Lua automaticamente.");
    return false;
  }

  if(!resolveScriptPath()) {
    logMessage("Nao encontrou o VS Hook.lua nos caminhos esperados.");
    for(const auto& candidate : buildScriptCandidates()) {
      logMessage("Tentado: " + candidate);
    }
    return false;
  }

  const int commandId = AddRemoveReaScript_ptr(true, 0, g_state.scriptPath.c_str(), true);
  if(commandId == 0) {
    logMessage("Falha ao registrar o script: " + g_state.scriptPath);
    return false;
  }

  g_state.scriptCommandId = commandId;
  logMessage("Script registrado com sucesso: " + g_state.scriptPath);
  return true;
}

static void runScript()
{
  if(g_state.scriptCommandId == 0) {
    if(!ensureScriptRegistered()) {
#ifdef _WIN32
      MessageBoxA(
        nullptr,
        ("VS Hook nao encontrou ou nao conseguiu registrar o script em:\n\n" + buildDefaultScriptPath()).c_str(),
        "VS Hook",
        MB_OK | MB_ICONERROR
      );
#endif
      return;
    }
  }

  if(Main_OnCommand_ptr && g_state.scriptCommandId != 0) {
    Main_OnCommand_ptr(g_state.scriptCommandId, 0);
  }
}

static bool hookCommand2(KbdSectionInfo* sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  (void)sec;
  (void)val;
  (void)val2;
  (void)relmode;
  (void)hwnd;

  if(command == g_state.customRunCommandId) {
    runScript();
    return true;
  }

  return false;
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  if(flag != 0 || !menustr || !hMenu) return;
  if(g_state.customRunCommandId == 0) return;

  if(std::string(menustr) == "Main extensions") {
    MENUITEMINFO mi{};
    mi.cbSize = sizeof(MENUITEMINFO);
    mi.fMask = MIIM_TYPE | MIIM_ID;
    mi.fType = MFT_STRING;
    mi.dwTypeData = const_cast<char*>("VS Hook");
    mi.wID = g_state.customRunCommandId;
    InsertMenuItem(hMenu, GetMenuItemCount(hMenu), TRUE, &mi);
  }
}

static bool loadApi(reaper_plugin_info_t* rec)
{
  if(!rec || !rec->GetFunc || !rec->Register) {
    return false;
  }

  plugin_register_ptr = rec->Register;

  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  Main_OnCommand_ptr = reinterpret_cast<Main_OnCommand_t>(rec->GetFunc("Main_OnCommand"));
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));
  ShowConsoleMsg_ptr = reinterpret_cast<ShowConsoleMsg_t>(rec->GetFunc("ShowConsoleMsg"));

  if(!plugin_register_ptr || !GetResourcePath_ptr || !Main_OnCommand_ptr) {
    return false;
  }

  return true;
}

static void initialize()
{
  if(g_state.initialized) return;

  const char* resource = GetResourcePath_ptr ? GetResourcePath_ptr() : "";
  g_state.resourcePath = resource ? resource : "";

  g_state.customRunCommandId = plugin_register_ptr("custom_action", &g_runAction);
  if(g_state.customRunCommandId != 0) {
    plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.actionHookRegistered = true;
    logMessage("Custom action registrada. Command ID: " + std::to_string(g_state.customRunCommandId));
  } else {
    logMessage("Falha ao registrar custom_action da extensao.");
  }

  plugin_register_ptr("hookcustommenu", reinterpret_cast<void*>(&menuHook));
  g_state.menuHookRegistered = true;
  logMessage("Menu hook registrado.");

  if(AddExtensionsMainMenu_ptr) {
    AddExtensionsMainMenu_ptr();
  }

  ensureScriptRegistered();

  g_state.initialized = true;
  logMessage("Extensao inicializada.");
}

static void shutdown()
{
  if(!g_state.initialized) return;

  if(plugin_register_ptr && g_state.menuHookRegistered) {
    plugin_register_ptr("-hookcustommenu", reinterpret_cast<void*>(&menuHook));
    g_state.menuHookRegistered = false;
  }

  if(plugin_register_ptr && g_state.actionHookRegistered) {
    plugin_register_ptr("-hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.actionHookRegistered = false;
  }

  g_state.initialized = false;
}

} // namespace vshook

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
  REAPER_PLUGIN_HINSTANCE instance,
  reaper_plugin_info_t* rec)
{
  vshook::g_hInstance = instance;

  if(!rec) {
    vshook::shutdown();
    return 0;
  }

  if(rec->caller_version != REAPER_PLUGIN_VERSION) return 0;
  if(!vshook::loadApi(rec)) return 0;

  vshook::initialize();
  return 1;
}

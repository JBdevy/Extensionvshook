#include "reaper_plugin.h"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <string>

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using ShowConsoleMsg_t = void (*)(const char*);
using GetResourcePath_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);

static plugin_register_t plugin_register_ptr = nullptr;
static ShowConsoleMsg_t ShowConsoleMsg_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;

static HINSTANCE g_hInstance = nullptr;

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
  const_cast<char*>("VSHookAppRun"),
  const_cast<char*>("VS Hook APP"),
  nullptr
};

static void logLine(const std::string& text)
{
  if(ShowConsoleMsg_ptr) {
    ShowConsoleMsg_ptr((text + "\n").c_str());
  }
}

static bool fileExists(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

static std::string buildDefaultScriptPath()
{
  const char* resource = GetResourcePath_ptr ? GetResourcePath_ptr() : "";
  std::string base = resource ? resource : "";
  for(char& c : base) if(c == '\\') c = '/';
  return base + "/Scripts/VS Hook APP/VS Hook.lua";
}

static bool ensureScriptRegistered()
{
  if(!AddRemoveReaScript_ptr) return false;
  g_state.scriptPath = buildDefaultScriptPath();

  if(!fileExists(g_state.scriptPath)) {
    logLine("[VS Hook APP] Script nao encontrado em: " + g_state.scriptPath);
    return false;
  }

  g_state.scriptCommandId = AddRemoveReaScript_ptr(true, 0, g_state.scriptPath.c_str(), true);
  if(g_state.scriptCommandId == 0) {
    logLine("[VS Hook APP] Falha ao registrar o script no Action List.");
    return false;
  }

  logLine("[VS Hook APP] Script registrado: " + g_state.scriptPath);
  return true;
}

static void runScript()
{
  if(g_state.scriptCommandId == 0) {
    if(!ensureScriptRegistered()) {
      MessageBoxA(nullptr,
                  ("VS Hook APP nao encontrou o script:\n\n" + buildDefaultScriptPath()).c_str(),
                  "VS Hook APP",
                  MB_OK | MB_ICONERROR);
      return;
    }
  }

  if(Main_OnCommand_ptr) {
    Main_OnCommand_ptr(g_state.scriptCommandId, 0);
  }
}

static bool hookCommand2(KbdSectionInfo* sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  (void)sec; (void)val; (void)val2; (void)relmode; (void)hwnd;

  if(command == g_state.customRunCommandId) {
    runScript();
    return true;
  }

  return false;
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  if(flag != 0 || !menustr || !hMenu) return;

  if(std::string(menustr) == "Main extensions") {
    AppendMenuA(hMenu, MF_STRING, static_cast<UINT_PTR>(g_state.customRunCommandId), "VS Hook APP");
  }
}

static bool loadApi(reaper_plugin_info_t* rec)
{
  if(!rec || !rec->GetFunc) return false;

  plugin_register_ptr = reinterpret_cast<plugin_register_t>(rec->GetFunc("plugin_register"));
  ShowConsoleMsg_ptr = reinterpret_cast<ShowConsoleMsg_t>(rec->GetFunc("ShowConsoleMsg"));
  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  Main_OnCommand_ptr = reinterpret_cast<Main_OnCommand_t>(rec->GetFunc("Main_OnCommand"));
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));

  return plugin_register_ptr && GetResourcePath_ptr && Main_OnCommand_ptr && AddRemoveReaScript_ptr;
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
  }

  plugin_register_ptr("hookcustommenu", reinterpret_cast<void*>(&menuHook));
  g_state.menuHookRegistered = true;

  if(AddExtensionsMainMenu_ptr) {
    AddExtensionsMainMenu_ptr();
  }

  ensureScriptRegistered();

  logLine("[VS Hook APP] Loader carregado.");
  logLine("[VS Hook APP] Resource path: " + g_state.resourcePath);
  logLine("[VS Hook APP] Script path: " + buildDefaultScriptPath());

  g_state.initialized = true;
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

#define SWELL_PROVIDED_BY_APP
#include "reaper_plugin.h"
#include <fstream>
#include <string>
#include <vector>

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using GetResourcePath_t = const char* (*)();
using Main_OnCommand_t = void (*)(int, int);
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);
using ShowConsoleMsg_t = void (*)(const char*);

static plugin_register_t plugin_register_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static Main_OnCommand_t Main_OnCommand_ptr = nullptr;
static AddRemoveReaScript_t AddRemoveReaScript_ptr = nullptr;
static ShowConsoleMsg_t ShowConsoleMsg_ptr = nullptr;

static custom_action_register_t g_action = {
  0,
  "VSHOOKRUN",
  "VS Hook",
  nullptr
};

struct State {
  std::string resourcePath;
  std::string scriptPath;
  std::string logPath;
  int scriptCommandId = 0;
  int commandId = 0;
  bool initialized = false;
  bool commandHookRegistered = false;
} g_state;

static void appendFileLog(const std::string& line)
{
  const std::string path = g_state.logPath.empty() ? "/tmp/vshook_nomenu_log.txt" : g_state.logPath;
  std::ofstream f(path, std::ios::app | std::ios::binary);
  if (f.good()) {
    f << line << "\n";
  }
}

static void logMessage(const std::string& text)
{
  const std::string line = "[VS Hook NO MENU] " + text;
  appendFileLog(line);

  if (ShowConsoleMsg_ptr) {
    ShowConsoleMsg_ptr((line + "\n").c_str());
  }
}

static bool fileExists(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

static std::string normalizeSlashes(std::string path)
{
  for (char& c : path) {
    if (c == '\\') c = '/';
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

static bool resolveScriptPath()
{
  const auto candidates = buildScriptCandidates();

  for (const auto& candidate : candidates) {
    if (fileExists(candidate)) {
      g_state.scriptPath = candidate;
      logMessage("Script encontrado em: " + candidate);
      return true;
    }
  }

  g_state.scriptPath.clear();
  logMessage("VS Hook.lua nao foi encontrado.");
  for (const auto& candidate : candidates) {
    logMessage("Tentado: " + candidate);
  }
  return false;
}

static bool ensureScriptRegistered()
{
  if (g_state.scriptCommandId != 0) {
    return true;
  }

  if (!AddRemoveReaScript_ptr) {
    logMessage("API AddRemoveReaScript indisponivel.");
    return false;
  }

  if (!resolveScriptPath()) {
    return false;
  }

  const int commandId = AddRemoveReaScript_ptr(true, 0, g_state.scriptPath.c_str(), true);
  if (commandId == 0) {
    logMessage("Falha ao registrar o script: " + g_state.scriptPath);
    return false;
  }

  g_state.scriptCommandId = commandId;
  logMessage("Script registrado com command ID: " + std::to_string(commandId));
  return true;
}

static void runScript()
{
  if (!ensureScriptRegistered()) {
    return;
  }

  if (Main_OnCommand_ptr && g_state.scriptCommandId != 0) {
    logMessage("Executando VS Hook.lua");
    Main_OnCommand_ptr(g_state.scriptCommandId, 0);
  } else {
    logMessage("Main_OnCommand indisponivel.");
  }
}

static bool hookCommand2(KbdSectionInfo* sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  (void)sec;
  (void)val;
  (void)val2;
  (void)relmode;
  (void)hwnd;

  if (command == g_state.commandId) {
    logMessage("hookCommand2 recebeu o commandId da extensao.");
    runScript();
    return true;
  }

  return false;
}

static bool loadApi(reaper_plugin_info_t* rec)
{
  if (!rec || !rec->Register || !rec->GetFunc) return false;

  plugin_register_ptr = rec->Register;
  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  Main_OnCommand_ptr = reinterpret_cast<Main_OnCommand_t>(rec->GetFunc("Main_OnCommand"));
  AddRemoveReaScript_ptr = reinterpret_cast<AddRemoveReaScript_t>(rec->GetFunc("AddRemoveReaScript"));
  ShowConsoleMsg_ptr = reinterpret_cast<ShowConsoleMsg_t>(rec->GetFunc("ShowConsoleMsg"));

  return plugin_register_ptr != nullptr;
}

static void initialize()
{
  if (g_state.initialized) return;

  if (GetResourcePath_ptr) {
    const char* rp = GetResourcePath_ptr();
    g_state.resourcePath = normalizeSlashes(rp ? rp : "");
    if (!g_state.resourcePath.empty()) {
      g_state.logPath = g_state.resourcePath + "/vshook_nomenu_log.txt";
    }
  }

  appendFileLog("========== VS Hook NO MENU START ==========");
  logMessage("resourcePath=" + (g_state.resourcePath.empty() ? std::string("(vazio)") : g_state.resourcePath));
  logMessage("logPath=" + (g_state.logPath.empty() ? std::string("/tmp/vshook_nomenu_log.txt") : g_state.logPath));

  g_state.commandId = plugin_register_ptr("custom_action", (void*)&g_action);
  if (g_state.commandId != 0) {
    logMessage("custom_action registrado: " + std::to_string(g_state.commandId));

    plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = true;
    logMessage("hookcommand2 registrado.");
  } else {
    logMessage("Falha ao registrar custom_action.");
  }

  g_state.initialized = true;
  logMessage("initialize finalizado.");
}

static void shutdown()
{
  logMessage("shutdown iniciado.");

  if (!plugin_register_ptr) return;

  if (g_state.commandHookRegistered) {
    plugin_register_ptr("-hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = false;
  }

  if (g_state.commandId != 0) {
    plugin_register_ptr("-custom_action", (void*)&g_action);
    g_state.commandId = 0;
  }

  g_state.initialized = false;
  appendFileLog("========== VS Hook NO MENU END ==========");
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

  if (rec->caller_version != REAPER_PLUGIN_VERSION) return 0;
  if (!vshook::loadApi(rec)) return 0;

  vshook::initialize();
  return 1;
}

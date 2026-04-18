#include "reaper_plugin.h"
#include <cstring>
#include <cctype>
#include <string>
#include <sstream>
#include <fstream>

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using GetResourcePath_t = const char* (*)();
using AddExtensionsMainMenu_t = bool (*)();
using ShowConsoleMsg_t = void (*)(const char*);

static plugin_register_t plugin_register_ptr = nullptr;
static GetResourcePath_t GetResourcePath_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static ShowConsoleMsg_t ShowConsoleMsg_ptr = nullptr;

static custom_action_register_t g_action = {
  0,
  "VSHOOKDIAG",
  "VS Hook Diagnostic",
  nullptr
};

struct State {
  int commandId = 0;
  bool initialized = false;
  bool menuHookRegistered = false;
  bool commandHookRegistered = false;
  std::string resourcePath;
  std::string logPath;
} g_state;

static std::string toLowerCopy(std::string s)
{
  for (char& c : s) {
    c = (char)std::tolower((unsigned char)c);
  }
  return s;
}

static std::string normalizeSlashes(std::string path)
{
  for (char& c : path) {
    if (c == '\\') c = '/';
  }
  return path;
}

static void appendFileLog(const std::string& line)
{
  std::string path = g_state.logPath.empty() ? "/tmp/vshook_diag_log.txt" : g_state.logPath;
  std::ofstream f(path, std::ios::app | std::ios::binary);
  if (f.good()) {
    f << line << "\n";
  }
}

static void logMessage(const std::string& text)
{
  const std::string line = "[VS Hook DIAG] " + text;
  appendFileLog(line);

  if (ShowConsoleMsg_ptr) {
    ShowConsoleMsg_ptr((line + "\n").c_str());
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
    logMessage("hookCommand2: VS Hook Diagnostic acionado com sucesso.");
    return true;
  }

  return false;
}

static void tryInsertMenuItem(HMENU hMenu, const char* label)
{
  if (!hMenu || g_state.commandId == 0) return;

  MENUITEMINFO mi = { sizeof(MENUITEMINFO), };
  mi.fMask = MIIM_TYPE | MIIM_ID;
  mi.fType = MFT_STRING;
  mi.dwTypeData = (char*)label;
  mi.wID = g_state.commandId;

  InsertMenuItem(hMenu, GetMenuItemCount(hMenu), true, &mi);
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  std::ostringstream oss;
  oss << "menuHook: flag=" << flag
      << " commandId=" << g_state.commandId
      << " menustr=" << (menustr ? menustr : "(null)");
  logMessage(oss.str());

  if (!menustr || !hMenu) return;
  if (flag != 0) return;
  if (g_state.commandId == 0) {
    logMessage("menuHook: commandId == 0, nao foi possivel inserir menu.");
    return;
  }

  const std::string ms = toLowerCopy(menustr);

  if (ms == "main extensions") {
    tryInsertMenuItem(hMenu, "VS Hook DIAG EXACT");
    logMessage("menuHook: inserido por match exato em 'Main extensions'.");
    return;
  }

  if (ms.find("extensions") != std::string::npos) {
    tryInsertMenuItem(hMenu, "VS Hook DIAG FUZZY");
    logMessage("menuHook: inserido por match parcial contendo 'extensions'.");
    return;
  }
}

static bool loadApi(reaper_plugin_info_t* rec)
{
  if (!rec || !rec->Register || !rec->GetFunc) return false;

  plugin_register_ptr = rec->Register;
  GetResourcePath_ptr = reinterpret_cast<GetResourcePath_t>(rec->GetFunc("GetResourcePath"));
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
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
      g_state.logPath = g_state.resourcePath + "/vshook_diag_log.txt";
    }
  }

  appendFileLog("========== VS Hook DIAG START ==========");
  logMessage("resourcePath=" + (g_state.resourcePath.empty() ? std::string("(vazio)") : g_state.resourcePath));
  logMessage("logPath=" + (g_state.logPath.empty() ? std::string("/tmp/vshook_diag_log.txt") : g_state.logPath));

  g_state.commandId = plugin_register_ptr("custom_action", (void*)&g_action);
  if (g_state.commandId != 0) {
    logMessage("custom_action registrado: " + std::to_string(g_state.commandId));
    plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = true;
    logMessage("hookcommand2 registrado.");
  } else {
    logMessage("Falha ao registrar custom_action.");
  }

  plugin_register_ptr("hookcustommenu", reinterpret_cast<void*>(&menuHook));
  g_state.menuHookRegistered = true;
  logMessage("hookcustommenu registrado.");

  if (AddExtensionsMainMenu_ptr) {
    const bool ok = AddExtensionsMainMenu_ptr();
    logMessage(std::string("AddExtensionsMainMenu retornou: ") + (ok ? "true" : "false"));
  } else {
    logMessage("AddExtensionsMainMenu indisponivel.");
  }

  g_state.initialized = true;
  logMessage("initialize finalizado.");
}

static void shutdown()
{
  logMessage("shutdown iniciado.");

  if (!plugin_register_ptr) return;

  if (g_state.menuHookRegistered) {
    plugin_register_ptr("-hookcustommenu", reinterpret_cast<void*>(&menuHook));
    g_state.menuHookRegistered = false;
  }

  if (g_state.commandHookRegistered) {
    plugin_register_ptr("-hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = false;
  }

  if (g_state.commandId != 0) {
    plugin_register_ptr("-custom_action", (void*)&g_action);
    g_state.commandId = 0;
  }

  g_state.initialized = false;
  appendFileLog("========== VS Hook DIAG END ==========");
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

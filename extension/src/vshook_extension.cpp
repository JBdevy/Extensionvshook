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

static custom_action_register_t g_action = {
  0,
  "VSHOOKRUN",
  "VS Hook APP",
  nullptr
};

struct State {
  std::string scriptPath;
  int scriptCommandId = 0;
  int commandId = 0;
  bool initialized = false;
  bool commandHookRegistered = false;
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

static std::vector<std::string> buildScriptCandidates()
{
#ifdef _WIN32
  // Windows: usa somente o caminho fixo do instalador, fora da pasta do usuario.
  // Isso evita falhas quando o perfil do Windows tem acentos, ex: Produção.
  return {
    "C:/Program Files/VS Hook APP/VS Hook.lua"
  };
#else
  const char* resource = GetResourcePath_ptr ? GetResourcePath_ptr() : "";
  const std::string base = normalizeSlashes(resource ? resource : "");

  return {
    base + "/Scripts/VS Hook APP/VS Hook.lua"
  };
#endif
}

static bool resolveScriptPath()
{
  const auto candidates = buildScriptCandidates();

  for (const auto& candidate : candidates) {
    if (fileExists(candidate)) {
      g_state.scriptPath = candidate;
      return true;
    }
  }

  g_state.scriptPath.clear();
  return false;
}

static bool ensureScriptRegistered()
{
  if (g_state.scriptCommandId != 0) {
    return true;
  }

  if (!AddRemoveReaScript_ptr) {
    return false;
  }

  if (!resolveScriptPath()) {
    return false;
  }

  const int commandId = AddRemoveReaScript_ptr(true, 0, g_state.scriptPath.c_str(), true);
  if (commandId == 0) {
    return false;
  }

  g_state.scriptCommandId = commandId;
  return true;
}

static void runScript()
{
  if (!ensureScriptRegistered()) {
    return;
  }

  if (Main_OnCommand_ptr && g_state.scriptCommandId != 0) {
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

  if (command == g_state.commandId) {
    runScript();
    return true;
  }

  return false;
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  if (!menustr || !hMenu) return;
  if (flag != 0) return;
  if (g_state.commandId == 0) return;

  if (std::strcmp(menustr, "Main extensions") == 0) {
    MENUITEMINFO mi = { sizeof(MENUITEMINFO), };
    mi.fMask = MIIM_TYPE | MIIM_ID;
    mi.fType = MFT_STRING;
    mi.dwTypeData = (char*)"VS Hook APP";
    mi.wID = g_state.commandId;
    InsertMenuItem(hMenu, 0, true, &mi);
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

  g_state.commandId = plugin_register_ptr("custom_action", (void*)&g_action);
  if (g_state.commandId != 0) {
    plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = true;
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

  if (g_state.commandHookRegistered) {
    plugin_register_ptr("-hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = false;
  }

  if (g_state.commandId != 0) {
    plugin_register_ptr("-custom_action", (void*)&g_action);
    g_state.commandId = 0;
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

  if (rec->caller_version != REAPER_PLUGIN_VERSION) return 0;
  if (!vshook::loadApi(rec)) return 0;

  vshook::initialize();
  return 1;
}

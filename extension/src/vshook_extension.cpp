#include "reaper_plugin.h"

namespace vshook {

using plugin_register_t = int (*)(const char*, void*);
using AddExtensionsMainMenu_t = bool (*)();
using ShowConsoleMsg_t = void (*)(const char*);

static plugin_register_t plugin_register_ptr = nullptr;
static AddExtensionsMainMenu_t AddExtensionsMainMenu_ptr = nullptr;
static ShowConsoleMsg_t ShowConsoleMsg_ptr = nullptr;

struct State {
  int commandId = 0;
  bool initialized = false;
  bool menuHookRegistered = false;
  bool commandHookRegistered = false;
} g_state;

static void logMessage(const char* text)
{
  if(ShowConsoleMsg_ptr) {
    ShowConsoleMsg_ptr(text);
  }
}

static bool hookCommand2(KbdSectionInfo* sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  (void)sec;
  (void)val;
  (void)val2;
  (void)relmode;
  (void)hwnd;

  if(command == g_state.commandId) {
    logMessage("[VS Hook Test] menu clicado com sucesso.\n");
    return true;
  }

  return false;
}

static void menuHook(const char* menustr, HMENU hMenu, int flag)
{
  if(flag != 0 || !menustr || !hMenu) return;
  if(g_state.commandId == 0) return;

  if(strcmp(menustr, "Main extensions") == 0) {
    MENUITEMINFO mi{};
    mi.cbSize = sizeof(MENUITEMINFO);
    mi.fMask = MIIM_TYPE | MIIM_ID;
    mi.fType = MFT_STRING;
    mi.dwTypeData = const_cast<char*>("VS Hook Test");
    mi.wID = g_state.commandId;
    InsertMenuItem(hMenu, GetMenuItemCount(hMenu), TRUE, &mi);
  }
}

static bool loadApi(reaper_plugin_info_t* rec)
{
  if(!rec || !rec->Register || !rec->GetFunc) return false;

  plugin_register_ptr = rec->Register;
  AddExtensionsMainMenu_ptr = reinterpret_cast<AddExtensionsMainMenu_t>(rec->GetFunc("AddExtensionsMainMenu"));
  ShowConsoleMsg_ptr = reinterpret_cast<ShowConsoleMsg_t>(rec->GetFunc("ShowConsoleMsg"));

  return plugin_register_ptr != nullptr;
}

static void initialize()
{
  if(g_state.initialized) return;

  if(AddExtensionsMainMenu_ptr) {
    AddExtensionsMainMenu_ptr();
    logMessage("[VS Hook Test] AddExtensionsMainMenu OK.\n");
  } else {
    logMessage("[VS Hook Test] AddExtensionsMainMenu indisponivel.\n");
  }

  g_state.commandId = plugin_register_ptr(
    "command_id",
    reinterpret_cast<void*>(const_cast<char*>("_VSHOOK_TEST_COMMAND"))
  );

  if(g_state.commandId != 0) {
    plugin_register_ptr("hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = true;
    logMessage("[VS Hook Test] command_id registrado.\n");
  } else {
    logMessage("[VS Hook Test] falha ao registrar command_id.\n");
  }

  plugin_register_ptr("hookcustommenu", reinterpret_cast<void*>(&menuHook));
  g_state.menuHookRegistered = true;
  logMessage("[VS Hook Test] hookcustommenu registrado.\n");

  g_state.initialized = true;
}

static void shutdown()
{
  if(!g_state.initialized || !plugin_register_ptr) return;

  if(g_state.menuHookRegistered) {
    plugin_register_ptr("-hookcustommenu", reinterpret_cast<void*>(&menuHook));
    g_state.menuHookRegistered = false;
  }

  if(g_state.commandHookRegistered) {
    plugin_register_ptr("-hookcommand2", reinterpret_cast<void*>(&hookCommand2));
    g_state.commandHookRegistered = false;
  }

  g_state.initialized = false;
}

} // namespace vshook

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
  REAPER_PLUGIN_HINSTANCE instance,
  reaper_plugin_info_t* rec)
{
  (void)instance;

  if(!rec) {
    vshook::shutdown();
    return 0;
  }

  if(rec->caller_version != REAPER_PLUGIN_VERSION) return 0;
  if(!vshook::loadApi(rec)) return 0;

  vshook::initialize();
  return 1;
}

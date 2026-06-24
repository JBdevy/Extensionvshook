#define SWELL_PROVIDED_BY_APP
#include "reaper_plugin.h"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iomanip>
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
using GetToggleCommandState_t = int (*)(int);
using GetToggleCommandStateEx_t = int (*)(int, int);
using AddExtensionsMainMenu_t = bool (*)();
using AddRemoveReaScript_t = int (*)(bool, int, const char*, bool);
using GetExtState_t = const char* (*)(const char*, const char*);
using SetExtState_t = void (*)(const char*, const char*, const char*, bool);
using EnumProjects_t = ReaProject* (*)(int, char*, int);
using GetProjExtState_t = int (*)(ReaProject*, const char*, const char*, char*, int);
using SetProjExtState_t = int (*)(ReaProject*, const char*, const char*, const char*);

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

    if (getScriptToggleState(other) == 1) {
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

  if (!toggleIfAlreadyOpen && getScriptToggleState(script) == 1) {
    return;
  }

  Main_OnCommand_ptr(script.scriptCommandId, 0);
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

static void setAutoOpenMode(const char* mode)
{
  if (!SetExtState_ptr) {
    showDiagnostic("REAPER nao entregou a API SetExtState para salvar essa configuracao.");
    return;
  }

  SetExtState_ptr(kExtStateSection, kAutoOpenModeKey, mode ? mode : "", true);

  // Desliga a chave antiga para ela nao religar o Pro depois que o usuario
  // escolher Basic ou desativar o auto-inicio.
  SetExtState_ptr(kExtStateSection, kLegacyAutoOpenKey, "0", true);
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

static void setProjectAutoOpenMode(const char* mode)
{
  if (!SetProjExtState_ptr) {
    showDiagnostic("REAPER nao entregou a API SetProjExtState para salvar configuracao deste projeto.");
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

  setAutoOpenMode(mode);
  showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " configurado para iniciar junto com o REAPER.");
}

static void toggleProjectAutoOpenMode(const char* mode)
{
  const std::string currentMode = getProjectAutoOpenMode();

  if (mode && currentMode == mode) {
    setProjectAutoOpenMode("");
    showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " nao vai mais iniciar junto com ESTE projeto.");
    return;
  }

  setProjectAutoOpenMode(mode);
  showDiagnostic(std::string(displayNameForAutoOpenMode(mode)) + " configurado para iniciar junto com ESTE projeto. Salve o projeto para gravar essa configuracao no RPP.");
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


static void startupTimer()
{
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

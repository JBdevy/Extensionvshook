// VS Hook compatibility subset for the ReaScript functions currently used by
// the VS Hook Lua scripts. Window enumeration, dialog and message behavior is
// adapted from Julian Sader's js_ReaScriptAPI (MIT); see THIRD_PARTY_NOTICES.md.

#ifndef SWELL_PROVIDED_BY_APP
#define SWELL_PROVIDED_BY_APP
#endif

#include "reaper_plugin.h"
#include "js_api_compat.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <commctrl.h>
  #include <commdlg.h>
  #include <shlobj.h>
#else
  #undef CF_TEXT
  #define CF_TEXT RegisterClipboardFormat("SWELL__CF_TEXT")
#endif

#ifndef BM_CLICK
  #define BM_CLICK 0x00F5
#endif
#ifndef CB_SELECTSTRING
  #define CB_SELECTSTRING 0x014D
#endif

namespace vshook_jsapi {
namespace {

plugin_register_t g_pluginRegister = nullptr;
plugin_getapi_t g_pluginGetApi = nullptr;

struct ApiEntry {
  const char* name;
  void* function;
  void* vararg;
  char* definition;
  bool registered = false;
};

static int intArg(void** args, int index)
{
  return args && args[index] ? static_cast<int>(reinterpret_cast<intptr_t>(args[index])) : 0;
}

static bool boolArg(void** args, int index)
{
  return intArg(args, index) != 0;
}

static double doubleArg(void** args, int index)
{
  return args && args[index] ? *static_cast<double*>(args[index]) : 0.0;
}

static const char* stringArg(void** args, int index)
{
  return args && args[index] ? static_cast<const char*>(args[index]) : "";
}

static void copyText(const std::string& text, char* output, int outputSize)
{
  if (!output || outputSize <= 0) return;
  const size_t count = std::min(text.size(), static_cast<size_t>(outputSize - 1));
  if (count) std::memcpy(output, text.data(), count);
  output[count] = '\0';
}

#ifdef _WIN32
static std::wstring utf8ToWide(const char* text)
{
  if (!text || !*text) return {};
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
  UINT codepage = CP_UTF8;
  DWORD flags = MB_ERR_INVALID_CHARS;
  if (count <= 0) {
    codepage = CP_ACP;
    flags = 0;
    count = MultiByteToWideChar(codepage, flags, text, -1, nullptr, 0);
  }
  if (count <= 0) return {};
  std::wstring result(static_cast<size_t>(count), L'\0');
  MultiByteToWideChar(codepage, flags, text, -1, result.data(), count);
  if (!result.empty() && result.back() == L'\0') result.pop_back();
  return result;
}

static std::string wideToUtf8(const wchar_t* text)
{
  if (!text || !*text) return {};
  const int count = WideCharToMultiByte(
    CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (count <= 0) return {};
  std::string result(static_cast<size_t>(count), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count, nullptr, nullptr);
  if (!result.empty() && result.back() == '\0') result.pop_back();
  return result;
}

static std::wstring utf8MultiStringToWide(const char* text)
{
  if (!text || !*text) {
    return std::wstring(L"All files (*.*)\0*.*\0\0", 21);
  }

  std::wstring result;
  const char* cursor = text;
  size_t scanned = 0;
  constexpr size_t maxScan = 64 * 1024;
  while (scanned < maxScan) {
    const size_t length = std::strlen(cursor);
    if (length == 0) {
      result.push_back(L'\0');
      break;
    }
    const std::wstring part = utf8ToWide(cursor);
    result.append(part);
    result.push_back(L'\0');
    cursor += length + 1;
    scanned += length + 1;
  }
  if (result.empty() || result.back() != L'\0') result.push_back(L'\0');
  if (result.size() < 2 || result[result.size() - 2] != L'\0') result.push_back(L'\0');
  return result;
}

static bool copyWideMultiStringToUtf8(
  const wchar_t* input,
  size_t inputCapacity,
  bool multiple,
  char* output,
  int outputSize)
{
  if (!input || !output || outputSize <= 0) return false;

  std::string packed;
  size_t offset = 0;
  do {
    if (offset >= inputCapacity) return false;
    const wchar_t* part = input + offset;
    const size_t length = std::wcslen(part);
    const std::string utf8 = wideToUtf8(part);
    packed.append(utf8);
    packed.push_back('\0');
    offset += length + 1;
    if (!multiple || length == 0 || input[offset] == L'\0') break;
  } while (offset < inputCapacity);

  if (multiple) packed.push_back('\0');
  if (packed.size() > static_cast<size_t>(outputSize)) {
    output[0] = '\0';
    return false;
  }
  std::memcpy(output, packed.data(), packed.size());
  return true;
}

static std::string getWindowTextUtf8(HWND hwnd)
{
  if (!hwnd) return {};
  const int length = GetWindowTextLengthW(hwnd);
  std::wstring value(static_cast<size_t>(std::max(length, 0)) + 1, L'\0');
  GetWindowTextW(hwnd, value.data(), static_cast<int>(value.size()));
  return wideToUtf8(value.c_str());
}
#else
static std::string getWindowTextUtf8(HWND hwnd)
{
  if (!hwnd) return {};
  std::vector<char> value(32768, '\0');
  GetWindowText(hwnd, value.data(), static_cast<int>(value.size()));
  return value.data();
}
#endif

static std::string lowerAscii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

// -------------------------------------------------------------------------
// Dialogs

static int Compat_Dialog_BrowseForSaveFile(
  const char* windowTitle,
  const char* initialFolder,
  const char* initialFile,
  const char* extensionList,
  char* fileNameOutNeedBig,
  int fileNameOutNeedBigSize)
{
  if (!fileNameOutNeedBig || fileNameOutNeedBigSize <= 1) return -1;
  fileNameOutNeedBig[0] = '\0';

#ifdef _WIN32
  std::vector<wchar_t> selected(32768, L'\0');
  const std::wstring initial = utf8ToWide(initialFile);
  if (!initial.empty()) {
    std::wcsncpy(selected.data(), initial.c_str(), selected.size() - 1);
  }
  const std::wstring folder = utf8ToWide(initialFolder);
  const std::wstring title = utf8ToWide(windowTitle);
  const std::wstring filters = utf8MultiStringToWide(extensionList);

  OPENFILENAMEW info{};
  info.lStructSize = sizeof(info);
  info.lpstrFilter = filters.c_str();
  info.lpstrFile = selected.data();
  info.nMaxFile = static_cast<DWORD>(selected.size());
  info.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
  info.lpstrTitle = title.empty() ? nullptr : title.c_str();
  info.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
  if (!GetSaveFileNameW(&info)) return 0;

  const std::string utf8 = wideToUtf8(selected.data());
  if (utf8.size() + 1 > static_cast<size_t>(fileNameOutNeedBigSize)) return -1;
  copyText(utf8, fileNameOutNeedBig, fileNameOutNeedBigSize);
  return 1;
#else
  const char* filters = extensionList ? extensionList : "";
  try {
    const bool selected = BrowseForSaveFile(
      windowTitle ? windowTitle : "",
      initialFolder ? initialFolder : "",
      initialFile ? initialFile : "",
      filters,
      fileNameOutNeedBig,
      fileNameOutNeedBigSize);
    if (!selected) fileNameOutNeedBig[0] = '\0';
    return selected ? 1 : 0;
  } catch (...) {
    fileNameOutNeedBig[0] = '\0';
    return -1;
  }
#endif
}

static int Compat_Dialog_BrowseForOpenFiles(
  const char* windowTitle,
  const char* initialFolder,
  const char* initialFile,
  const char* extensionList,
  bool allowMultiple,
  char* fileNamesOutNeedBig,
  int fileNamesOutNeedBigSize)
{
  if (!fileNamesOutNeedBig || fileNamesOutNeedBigSize <= 1) return -1;
  fileNamesOutNeedBig[0] = '\0';

#ifdef _WIN32
  std::vector<wchar_t> selected(1024 * 1024, L'\0');
  const std::wstring initial = utf8ToWide(initialFile);
  if (!initial.empty()) {
    std::wcsncpy(selected.data(), initial.c_str(), selected.size() - 1);
  }
  const std::wstring folder = utf8ToWide(initialFolder);
  const std::wstring title = utf8ToWide(windowTitle);
  const std::wstring filters = utf8MultiStringToWide(extensionList);

  OPENFILENAMEW info{};
  info.lStructSize = sizeof(info);
  info.lpstrFilter = filters.c_str();
  info.lpstrFile = selected.data();
  info.nMaxFile = static_cast<DWORD>(selected.size());
  info.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
  info.lpstrTitle = title.empty() ? nullptr : title.c_str();
  info.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  if (allowMultiple) info.Flags |= OFN_ALLOWMULTISELECT;
  if (!GetOpenFileNameW(&info)) return 0;

  return copyWideMultiStringToUtf8(
    selected.data(), selected.size(), allowMultiple,
    fileNamesOutNeedBig, fileNamesOutNeedBigSize) ? 1 : -1;
#else
  char* selected = nullptr;
  try {
    selected = BrowseForFiles(
      windowTitle ? windowTitle : "",
      initialFolder ? initialFolder : "",
      initialFile ? initialFile : "",
      allowMultiple,
      extensionList ? extensionList : "");
  } catch (...) {
    return -1;
  }
  if (!selected) return 0;

  size_t total = std::strlen(selected) + 1;
  if (allowMultiple) {
    while (total + 1 < 1024 * 1024 &&
           !(selected[total - 1] == '\0' && selected[total] == '\0')) {
      ++total;
    }
    ++total;
  }
  const bool fits = total <= static_cast<size_t>(fileNamesOutNeedBigSize);
  if (fits) std::memcpy(fileNamesOutNeedBig, selected, total);
  std::free(selected);
  return fits ? 1 : -1;
#endif
}

#ifdef _WIN32
static int CALLBACK browseFolderCallback(HWND hwnd, UINT message, LPARAM, LPARAM data)
{
  if (message == BFFM_INITIALIZED && data) {
    SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, data);
  }
  return 0;
}
#endif

static int Compat_Dialog_BrowseForFolder(
  const char* caption,
  const char* initialFolder,
  char* folderOutNeedBig,
  int folderOutNeedBigSize)
{
  if (!folderOutNeedBig || folderOutNeedBigSize <= 1) return -1;
  folderOutNeedBig[0] = '\0';

#ifdef _WIN32
  const std::wstring title = utf8ToWide(caption);
  const std::wstring initial = utf8ToWide(initialFolder);
  std::vector<wchar_t> displayName(32768, L'\0');

  BROWSEINFOW info{};
  info.pszDisplayName = displayName.data();
  info.lpszTitle = title.empty() ? nullptr : title.c_str();
  info.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
  info.lpfn = browseFolderCallback;
  info.lParam = reinterpret_cast<LPARAM>(initial.c_str());

  PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&info);
  if (!item) return 0;

  std::vector<wchar_t> selected(32768, L'\0');
  const BOOL ok = SHGetPathFromIDListW(item, selected.data());
  CoTaskMemFree(item);
  if (!ok) return -1;

  const std::string utf8 = wideToUtf8(selected.data());
  if (utf8.size() + 1 > static_cast<size_t>(folderOutNeedBigSize)) return -1;
  copyText(utf8, folderOutNeedBig, folderOutNeedBigSize);
  return 1;
#else
  try {
    return BrowseForDirectory(
      caption ? caption : "",
      initialFolder ? initialFolder : "",
      folderOutNeedBig,
      folderOutNeedBigSize) ? 1 : 0;
  } catch (...) {
    folderOutNeedBig[0] = '\0';
    return -1;
  }
#endif
}

// -------------------------------------------------------------------------
// Windows and controls

static bool Compat_Window_GetRect(
  void* window, int* left, int* top, int* right, int* bottom)
{
  if (!window || !left || !top || !right || !bottom) return false;
  RECT rect{};
  const bool ok = !!GetWindowRect(static_cast<HWND>(window), &rect);
#ifdef __APPLE__
  if (rect.top < rect.bottom) {
#else
  if (rect.top > rect.bottom) {
#endif
    *top = static_cast<int>(rect.bottom);
    *bottom = static_cast<int>(rect.top);
  } else {
    *top = static_cast<int>(rect.top);
    *bottom = static_cast<int>(rect.bottom);
  }
  *left = static_cast<int>(rect.left);
  *right = static_cast<int>(rect.right);
  return ok;
}

struct FindWindowData {
  std::string target;
  bool exact = false;
  HWND found = nullptr;
};

static bool titleMatches(HWND hwnd, const FindWindowData& data)
{
  const std::string title = lowerAscii(getWindowTextUtf8(hwnd));
  return data.exact ? title == data.target : title.find(data.target) != std::string::npos;
}

static BOOL CALLBACK findChildCallback(HWND hwnd, LPARAM parameter)
{
  auto& data = *reinterpret_cast<FindWindowData*>(parameter);
  if (titleMatches(hwnd, data)) {
    data.found = hwnd;
    return FALSE;
  }
  return TRUE;
}

static BOOL CALLBACK findTopCallback(HWND hwnd, LPARAM parameter)
{
  auto& data = *reinterpret_cast<FindWindowData*>(parameter);
  if (titleMatches(hwnd, data)) {
    data.found = hwnd;
    return FALSE;
  }
  EnumChildWindows(hwnd, findChildCallback, parameter);
  return data.found ? FALSE : TRUE;
}

static void* Compat_Window_Find(const char* title, bool exact)
{
  FindWindowData data{lowerAscii(title ? title : ""), exact, nullptr};
  EnumWindows(findTopCallback, reinterpret_cast<LPARAM>(&data));
  return data.found;
}

static BOOL CALLBACK listChildCallback(HWND hwnd, LPARAM parameter)
{
  auto& windows = *reinterpret_cast<std::set<HWND>*>(parameter);
  windows.insert(hwnd);
  return TRUE;
}

static int Compat_Window_ListAllChild(
  void* parent, char* listOutNeedBig, int listOutNeedBigSize)
{
  if (!listOutNeedBig || listOutNeedBigSize <= 0) return 0;
  listOutNeedBig[0] = '\0';
  if (!parent) return 0;

  std::set<HWND> windows;
  EnumChildWindows(
    static_cast<HWND>(parent), listChildCallback, reinterpret_cast<LPARAM>(&windows));

  std::string packed;
  char item[40]{};
  for (HWND hwnd : windows) {
    const int length = std::snprintf(
      item, sizeof(item), "0x%llX,",
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)));
    if (length > 0) packed.append(item, static_cast<size_t>(length));
  }
  if (!packed.empty()) packed.pop_back();
  if (packed.size() + 1 > static_cast<size_t>(listOutNeedBigSize)) return -static_cast<int>(windows.size());
  copyText(packed, listOutNeedBig, listOutNeedBigSize);
  return static_cast<int>(windows.size());
}

static void Compat_Window_GetTitle(void* window, char* titleOutNeedBig, int titleOutNeedBigSize)
{
  if (!titleOutNeedBig || titleOutNeedBigSize <= 0) return;
  copyText(getWindowTextUtf8(static_cast<HWND>(window)), titleOutNeedBig, titleOutNeedBigSize);
}

static bool Compat_Window_SetTitle(void* window, const char* title)
{
  if (!window) return false;
#ifdef _WIN32
  const std::wstring wide = utf8ToWide(title);
  return !!SetWindowTextW(static_cast<HWND>(window), wide.c_str());
#else
  return !!SetWindowText(static_cast<HWND>(window), title ? title : "");
#endif
}

static void Compat_Window_GetClassName(HWND window, char* classOut, int classOutSize)
{
  if (!classOut || classOutSize <= 0) return;
  classOut[0] = '\0';
  if (!window) return;
#ifdef _WIN32
  std::vector<wchar_t> wide(1024, L'\0');
  if (GetClassNameW(window, wide.data(), static_cast<int>(wide.size()))) {
    copyText(wideToUtf8(wide.data()), classOut, classOutSize);
  }
#else
  GetClassName(window, classOut, classOutSize);
  classOut[classOutSize - 1] = '\0';
#endif
}

static void* Compat_Window_HandleFromAddress(double address)
{
  const intptr_t integerAddress = static_cast<intptr_t>(address);
  return static_cast<double>(integerAddress) == address
    ? reinterpret_cast<void*>(integerAddress)
    : nullptr;
}

static void Compat_Window_SetFocus(void* window)
{
  if (window) SetFocus(static_cast<HWND>(window));
}

static bool Compat_Window_SetClipboard(const char* text)
{
  const char* value = text ? text : "";
#ifdef _WIN32
  const std::wstring wide = utf8ToWide(value);
  const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
#else
  const size_t bytes = std::strlen(value) + 1;
#endif
  HANDLE memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<int>(bytes));
  if (!memory) return false;
  void* destination = GlobalLock(memory);
  if (!destination) {
    GlobalFree(memory);
    return false;
  }
#ifdef _WIN32
  std::memcpy(destination, wide.c_str(), bytes);
#else
  std::memcpy(destination, value, bytes);
#endif
  GlobalUnlock(memory);

  if (!OpenClipboard(nullptr)) {
    GlobalFree(memory);
    return false;
  }
  EmptyClipboard();
#ifdef _WIN32
  const bool ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
#else
  SetClipboardData(CF_TEXT, memory);
  const bool ok = true;
#endif
  CloseClipboard();
  if (!ok) GlobalFree(memory);
  return ok;
}

static const char* Compat_Window_GetClipboard()
{
  static thread_local std::string clipboard;
  clipboard.clear();
  if (!OpenClipboard(nullptr)) return clipboard.c_str();
#ifdef _WIN32
  HANDLE memory = GetClipboardData(CF_UNICODETEXT);
  if (memory) {
    const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(memory));
    if (text) {
      clipboard = wideToUtf8(text);
      GlobalUnlock(memory);
    }
  }
#else
  HANDLE memory = GetClipboardData(CF_TEXT);
  if (memory) {
    const char* text = static_cast<const char*>(GlobalLock(memory));
    if (text) {
      clipboard = text;
      GlobalUnlock(memory);
    }
  }
#endif
  CloseClipboard();
  return clipboard.c_str();
}

static int Compat_ListView_GetItemCount(HWND listView)
{
  return listView ? ListView_GetItemCount(listView) : -1;
}

static void Compat_ListView_EnsureVisible(HWND listView, int index, bool partialOK)
{
  if (listView) ListView_EnsureVisible(listView, index, partialOK);
}

static void Compat_ListView_GetItemText(
  HWND listView, int index, int subItem, char* textOut, int textOutSize)
{
  if (!textOut || textOutSize <= 0) return;
  textOut[0] = '\0';
  if (listView) ListView_GetItemText(listView, index, subItem, textOut, textOutSize);
}

static bool Compat_ListView_SetItemState(HWND listView, int index, int state, int stateMask)
{
  if (!listView) return false;
  UINT nativeState = 0;
  UINT nativeMask = 0;
  if (state & 1) nativeState |= LVIS_FOCUSED;
  if (state & 2) nativeState |= LVIS_SELECTED;
  if (stateMask & 1) nativeMask |= LVIS_FOCUSED;
  if (stateMask & 2) nativeMask |= LVIS_SELECTED;
#ifdef _WIN32
  LVITEMW item{};
  item.state = nativeState;
  item.stateMask = nativeMask;
  return !!SendMessageW(
    listView, LVM_SETITEMSTATE, static_cast<WPARAM>(index),
    reinterpret_cast<LPARAM>(&item));
#else
  return !!ListView_SetItemState(listView, index, nativeState, nativeMask);
#endif
}

static bool messageFromName(const char* name, UINT& message)
{
  if (!name || !*name) return false;
  struct NamedMessage { const char* name; UINT value; };
  static const NamedMessage messages[] = {
    {"WM_SETTEXT", WM_SETTEXT},
    {"WM_COMMAND", WM_COMMAND},
    {"WM_KEYUP", WM_KEYUP},
    {"CB_GETCOUNT", CB_GETCOUNT},
    {"CB_GETLBTEXT", CB_GETLBTEXT},
    {"CB_SETCURSEL", CB_SETCURSEL},
    {"CB_FINDSTRINGEXACT", CB_FINDSTRINGEXACT},
    {"CB_SELECTSTRING", CB_SELECTSTRING},
    {"EM_SETSEL", EM_SETSEL},
    {"BM_CLICK", BM_CLICK},
  };
  for (const auto& candidate : messages) {
    if (std::strcmp(name, candidate.name) == 0) {
      message = candidate.value;
      return true;
    }
  }

  errno = 0;
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(name, &end, 0);
  if (errno != 0 || end == name || (end && *end != '\0')) return false;
  message = static_cast<UINT>(parsed);
  return true;
}

static WPARAM combineWParam(double low, int high)
{
  return high
    ? MAKEWPARAM(static_cast<int>(low), high)
    : static_cast<WPARAM>(static_cast<int64_t>(low));
}

static LPARAM combineLParam(double low, int high)
{
  return high
    ? MAKELPARAM(static_cast<int>(low), high)
    : static_cast<LPARAM>(static_cast<int64_t>(low));
}

static bool Compat_WindowMessage_Post(
  void* window, const char* messageName,
  double wParam, int wParamHighWord,
  double lParam, int lParamHighWord)
{
  if (!window) return false;
  UINT message = 0;
  if (!messageFromName(messageName, message)) return false;
#ifndef _WIN32
  if (message == BM_CLICK) {
    const HWND hwnd = static_cast<HWND>(window);
    const WPARAM controlId = static_cast<WPARAM>(GetWindowLong(hwnd, GWL_ID));
    return !!PostMessage(GetParent(hwnd), WM_COMMAND, controlId, reinterpret_cast<LPARAM>(hwnd));
  }
#endif
  return !!PostMessage(
    static_cast<HWND>(window), message,
    combineWParam(wParam, wParamHighWord),
    combineLParam(lParam, lParamHighWord));
}

static int Compat_WindowMessage_Send(
  void* window, const char* messageName,
  double wParam, int wParamHighWord,
  double lParam, int lParamHighWord)
{
  if (!window) return 0;
  UINT message = 0;
  if (!messageFromName(messageName, message)) return 0;
#ifndef _WIN32
  if (message == BM_CLICK) {
    const HWND hwnd = static_cast<HWND>(window);
    const WPARAM controlId = static_cast<WPARAM>(GetWindowLong(hwnd, GWL_ID));
    return static_cast<int>(SendMessage(
      GetParent(hwnd), WM_COMMAND, controlId, reinterpret_cast<LPARAM>(hwnd)));
  }
#endif
  return static_cast<int>(SendMessage(
    static_cast<HWND>(window), message,
    combineWParam(wParam, wParamHighWord),
    combineLParam(lParam, lParamHighWord)));
}

// -------------------------------------------------------------------------
// ReaScript vararg wrappers

static void* vararg_Dialog_BrowseForSaveFile(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_Dialog_BrowseForSaveFile(
    stringArg(a, 0), stringArg(a, 1), stringArg(a, 2), stringArg(a, 3),
    static_cast<char*>(a[4]), intArg(a, 5))));
}

static void* vararg_Dialog_BrowseForOpenFiles(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_Dialog_BrowseForOpenFiles(
    stringArg(a, 0), stringArg(a, 1), stringArg(a, 2), stringArg(a, 3),
    boolArg(a, 4), static_cast<char*>(a[5]), intArg(a, 6))));
}

static void* vararg_Dialog_BrowseForFolder(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_Dialog_BrowseForFolder(
    stringArg(a, 0), stringArg(a, 1), static_cast<char*>(a[2]), intArg(a, 3))));
}

static void* vararg_Window_GetRect(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_Window_GetRect(
    a[0], static_cast<int*>(a[1]), static_cast<int*>(a[2]),
    static_cast<int*>(a[3]), static_cast<int*>(a[4]))));
}

static void* vararg_Window_Find(void** a, int)
{
  return Compat_Window_Find(stringArg(a, 0), boolArg(a, 1));
}

static void* vararg_Window_ListAllChild(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_Window_ListAllChild(
    a[0], static_cast<char*>(a[1]), intArg(a, 2))));
}

static void* vararg_Window_GetTitle(void** a, int)
{
  Compat_Window_GetTitle(a[0], static_cast<char*>(a[1]), intArg(a, 2));
  return nullptr;
}

static void* vararg_Window_SetTitle(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(
    Compat_Window_SetTitle(a[0], stringArg(a, 1))));
}

static void* vararg_Window_GetClassName(void** a, int)
{
  Compat_Window_GetClassName(
    static_cast<HWND>(a[0]), static_cast<char*>(a[1]), intArg(a, 2));
  return nullptr;
}

static void* vararg_Window_HandleFromAddress(void** a, int)
{
  return Compat_Window_HandleFromAddress(doubleArg(a, 0));
}

static void* vararg_Window_SetFocus(void** a, int)
{
  Compat_Window_SetFocus(a[0]);
  return nullptr;
}

static void* vararg_Window_SetClipboard(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(
    Compat_Window_SetClipboard(stringArg(a, 0))));
}

static void* vararg_Window_GetClipboard(void**, int)
{
  return const_cast<char*>(Compat_Window_GetClipboard());
}

static void* vararg_ListView_GetItemCount(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(
    Compat_ListView_GetItemCount(static_cast<HWND>(a[0]))));
}

static void* vararg_ListView_EnsureVisible(void** a, int)
{
  Compat_ListView_EnsureVisible(
    static_cast<HWND>(a[0]), intArg(a, 1), boolArg(a, 2));
  return nullptr;
}

static void* vararg_ListView_GetItemText(void** a, int)
{
  Compat_ListView_GetItemText(
    static_cast<HWND>(a[0]), intArg(a, 1), intArg(a, 2),
    static_cast<char*>(a[3]), intArg(a, 4));
  return nullptr;
}

static void* vararg_ListView_SetItemState(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_ListView_SetItemState(
    static_cast<HWND>(a[0]), intArg(a, 1), intArg(a, 2), intArg(a, 3))));
}

static void* vararg_WindowMessage_Post(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_WindowMessage_Post(
    a[0], stringArg(a, 1), doubleArg(a, 2), intArg(a, 3),
    doubleArg(a, 4), intArg(a, 5))));
}

static void* vararg_WindowMessage_Send(void** a, int)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(Compat_WindowMessage_Send(
    a[0], stringArg(a, 1), doubleArg(a, 2), intArg(a, 3),
    doubleArg(a, 4), intArg(a, 5))));
}

// API definition strings must remain alive until unregisterApi().
static char defDialogSave[] =
  "int\0const char*,const char*,const char*,const char*,char*,int\0"
  "windowTitle,initialFolder,initialFile,extensionList,fileNameOutNeedBig,fileNameOutNeedBig_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defDialogOpen[] =
  "int\0const char*,const char*,const char*,const char*,bool,char*,int\0"
  "windowTitle,initialFolder,initialFile,extensionList,allowMultiple,fileNamesOutNeedBig,fileNamesOutNeedBig_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defDialogFolder[] =
  "int\0const char*,const char*,char*,int\0"
  "caption,initialFolder,folderOutNeedBig,folderOutNeedBig_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowGetRect[] =
  "bool\0void*,int*,int*,int*,int*\0windowHWND,leftOut,topOut,rightOut,bottomOut\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowFind[] =
  "void*\0const char*,bool\0title,exact\0VS Hook built-in compatibility implementation.";
static char defWindowListAllChild[] =
  "int\0void*,char*,int\0parentHWND,listOutNeedBig,listOutNeedBig_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowGetTitle[] =
  "void\0void*,char*,int\0windowHWND,titleOutNeedBig,titleOutNeedBig_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowSetTitle[] =
  "bool\0void*,const char*\0windowHWND,title\0VS Hook built-in compatibility implementation.";
static char defWindowGetClassName[] =
  "void\0void*,char*,int\0windowHWND,classOut,classOut_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowHandleFromAddress[] =
  "void*\0double\0address\0VS Hook built-in compatibility implementation.";
static char defWindowSetFocus[] =
  "void\0void*\0windowHWND\0VS Hook built-in compatibility implementation.";
static char defWindowSetClipboard[] =
  "bool\0const char*\0text\0VS Hook built-in compatibility implementation.";
static char defWindowGetClipboard[] =
  "const char*\0\0\0VS Hook built-in compatibility implementation.";
static char defListViewGetItemCount[] =
  "int\0void*\0listviewHWND\0VS Hook built-in compatibility implementation.";
static char defListViewEnsureVisible[] =
  "void\0void*,int,bool\0listviewHWND,index,partialOK\0"
  "VS Hook built-in compatibility implementation.";
static char defListViewGetItemText[] =
  "void\0void*,int,int,char*,int\0listviewHWND,index,subItem,textOut,textOut_sz\0"
  "VS Hook built-in compatibility implementation.";
static char defListViewSetItemState[] =
  "bool\0void*,int,int,int\0listviewHWND,index,state,stateMask\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowMessagePost[] =
  "bool\0void*,const char*,double,int,double,int\0"
  "windowHWND,message,wParam,wParamHighWord,lParam,lParamHighWord\0"
  "VS Hook built-in compatibility implementation.";
static char defWindowMessageSend[] =
  "int\0void*,const char*,double,int,double,int\0"
  "windowHWND,message,wParam,wParamHighWord,lParam,lParamHighWord\0"
  "VS Hook built-in compatibility implementation.";

static ApiEntry apiEntries[] = {
  {"JS_Dialog_BrowseForFolder", reinterpret_cast<void*>(&Compat_Dialog_BrowseForFolder), reinterpret_cast<void*>(&vararg_Dialog_BrowseForFolder), defDialogFolder},
  {"JS_Dialog_BrowseForOpenFiles", reinterpret_cast<void*>(&Compat_Dialog_BrowseForOpenFiles), reinterpret_cast<void*>(&vararg_Dialog_BrowseForOpenFiles), defDialogOpen},
  {"JS_Dialog_BrowseForSaveFile", reinterpret_cast<void*>(&Compat_Dialog_BrowseForSaveFile), reinterpret_cast<void*>(&vararg_Dialog_BrowseForSaveFile), defDialogSave},
  {"JS_ListView_EnsureVisible", reinterpret_cast<void*>(&Compat_ListView_EnsureVisible), reinterpret_cast<void*>(&vararg_ListView_EnsureVisible), defListViewEnsureVisible},
  {"JS_ListView_GetItemCount", reinterpret_cast<void*>(&Compat_ListView_GetItemCount), reinterpret_cast<void*>(&vararg_ListView_GetItemCount), defListViewGetItemCount},
  {"JS_ListView_GetItemText", reinterpret_cast<void*>(&Compat_ListView_GetItemText), reinterpret_cast<void*>(&vararg_ListView_GetItemText), defListViewGetItemText},
  {"JS_ListView_SetItemState", reinterpret_cast<void*>(&Compat_ListView_SetItemState), reinterpret_cast<void*>(&vararg_ListView_SetItemState), defListViewSetItemState},
  {"JS_Window_Find", reinterpret_cast<void*>(&Compat_Window_Find), reinterpret_cast<void*>(&vararg_Window_Find), defWindowFind},
  {"JS_Window_GetClassName", reinterpret_cast<void*>(&Compat_Window_GetClassName), reinterpret_cast<void*>(&vararg_Window_GetClassName), defWindowGetClassName},
  {"JS_Window_GetClipboard", reinterpret_cast<void*>(&Compat_Window_GetClipboard), reinterpret_cast<void*>(&vararg_Window_GetClipboard), defWindowGetClipboard},
  {"JS_Window_GetRect", reinterpret_cast<void*>(&Compat_Window_GetRect), reinterpret_cast<void*>(&vararg_Window_GetRect), defWindowGetRect},
  {"JS_Window_GetTitle", reinterpret_cast<void*>(&Compat_Window_GetTitle), reinterpret_cast<void*>(&vararg_Window_GetTitle), defWindowGetTitle},
  {"JS_Window_HandleFromAddress", reinterpret_cast<void*>(&Compat_Window_HandleFromAddress), reinterpret_cast<void*>(&vararg_Window_HandleFromAddress), defWindowHandleFromAddress},
  {"JS_Window_ListAllChild", reinterpret_cast<void*>(&Compat_Window_ListAllChild), reinterpret_cast<void*>(&vararg_Window_ListAllChild), defWindowListAllChild},
  {"JS_Window_SetClipboard", reinterpret_cast<void*>(&Compat_Window_SetClipboard), reinterpret_cast<void*>(&vararg_Window_SetClipboard), defWindowSetClipboard},
  {"JS_Window_SetFocus", reinterpret_cast<void*>(&Compat_Window_SetFocus), reinterpret_cast<void*>(&vararg_Window_SetFocus), defWindowSetFocus},
  {"JS_Window_SetTitle", reinterpret_cast<void*>(&Compat_Window_SetTitle), reinterpret_cast<void*>(&vararg_Window_SetTitle), defWindowSetTitle},
  {"JS_WindowMessage_Post", reinterpret_cast<void*>(&Compat_WindowMessage_Post), reinterpret_cast<void*>(&vararg_WindowMessage_Post), defWindowMessagePost},
  {"JS_WindowMessage_Send", reinterpret_cast<void*>(&Compat_WindowMessage_Send), reinterpret_cast<void*>(&vararg_WindowMessage_Send), defWindowMessageSend},
};

static std::string registrationKey(const char* prefix, const char* name, bool remove)
{
  return std::string(remove ? "-" : "") + prefix + name;
}

static bool registerEntry(ApiEntry& entry)
{
  if (g_pluginGetApi && g_pluginGetApi(entry.name) != nullptr) return true;

  const std::string directKey = registrationKey("API_", entry.name, false);
  const std::string varargKey = registrationKey("APIvararg_", entry.name, false);
  const std::string definitionKey = registrationKey("APIdef_", entry.name, false);

  const bool direct = g_pluginRegister(directKey.c_str(), entry.function) != 0;
  const bool vararg = direct && g_pluginRegister(varargKey.c_str(), entry.vararg) != 0;
  const bool definition = vararg && g_pluginRegister(definitionKey.c_str(), entry.definition) != 0;
  if (direct && vararg && definition) {
    entry.registered = true;
    return true;
  }

  if (vararg) {
    const std::string remove = registrationKey("APIvararg_", entry.name, true);
    g_pluginRegister(remove.c_str(), entry.vararg);
  }
  if (direct) {
    const std::string remove = registrationKey("API_", entry.name, true);
    g_pluginRegister(remove.c_str(), entry.function);
  }
  return false;
}

static void unregisterEntry(ApiEntry& entry)
{
  if (!entry.registered || !g_pluginRegister) return;
  const std::string definitionKey = registrationKey("APIdef_", entry.name, true);
  const std::string varargKey = registrationKey("APIvararg_", entry.name, true);
  const std::string directKey = registrationKey("API_", entry.name, true);
  g_pluginRegister(definitionKey.c_str(), entry.definition);
  g_pluginRegister(varargKey.c_str(), entry.vararg);
  g_pluginRegister(directKey.c_str(), entry.function);
  entry.registered = false;
}

} // namespace

bool browseForFolder(
  const char* caption,
  const char* initialFolder,
  char* folderOut,
  int folderOutSize)
{
  return Compat_Dialog_BrowseForFolder(
    caption, initialFolder, folderOut,
    folderOutSize) == 1;
}

bool browseForSaveFile(
  const char* title,
  const char* initialFolder,
  const char* initialFile,
  const char* extensionList,
  char* fileOut,
  int fileOutSize)
{
  return Compat_Dialog_BrowseForSaveFile(
    title, initialFolder, initialFile,
    extensionList, fileOut, fileOutSize) == 1;
}

bool browseForOpenFile(
  const char* title,
  const char* initialFolder,
  const char* initialFile,
  const char* extensionList,
  char* fileOut,
  int fileOutSize)
{
  return Compat_Dialog_BrowseForOpenFiles(
    title, initialFolder, initialFile,
    extensionList, false, fileOut,
    fileOutSize) == 1;
}

bool registerApi(plugin_register_t pluginRegister, plugin_getapi_t pluginGetApi)
{
  if (!pluginRegister) return false;
  if (isRegistered()) return true;
  g_pluginRegister = pluginRegister;
  g_pluginGetApi = pluginGetApi;

  bool ok = true;
  for (auto& entry : apiEntries) {
    if (!registerEntry(entry)) ok = false;
  }
  return ok;
}

void unregisterApi()
{
  for (auto iterator = std::rbegin(apiEntries); iterator != std::rend(apiEntries); ++iterator) {
    unregisterEntry(*iterator);
  }
  g_pluginGetApi = nullptr;
  g_pluginRegister = nullptr;
}

bool isRegistered()
{
  return std::any_of(std::begin(apiEntries), std::end(apiEntries), [](const ApiEntry& entry) {
    return entry.registered;
  });
}

} // namespace vshook_jsapi

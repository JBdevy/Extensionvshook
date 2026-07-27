#pragma once

namespace vshook_jsapi {

using plugin_register_t = int (*)(const char*, void*);
using plugin_getapi_t = void* (*)(const char*);

// Registers only the JS_* functions that are not already provided by an
// installed js_ReaScriptAPI extension.
bool registerApi(plugin_register_t pluginRegister, plugin_getapi_t pluginGetApi);
void unregisterApi();
bool isRegistered();
bool browseForFolder(
  const char* caption,
  const char* initialFolder,
  char* folderOut,
  int folderOutSize);
bool browseForSaveFile(
  const char* title,
  const char* initialFolder,
  const char* initialFile,
  const char* extensionList,
  char* fileOut,
  int fileOutSize);
bool browseForOpenFile(
  const char* title,
  const char* initialFolder,
  const char* initialFile,
  const char* extensionList,
  char* fileOut,
  int fileOutSize);

} // namespace vshook_jsapi

// ----------------------------------------------------------------------------
// Basic APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include "imgui.h"
#include "includes/htmodloader.h"
#include "htinternal.hpp"

static thread_local HTError gLastError = HTError_Success;

HTMLAPIATTR VOID HTMLAPI HTSetLastError(
  HTError dwError
) {
  gLastError = dwError;
}

HTMLAPIATTR HTError HTMLAPI HTGetLastError() {
  return gLastError;
}

HTMLAPIATTR VOID HTMLAPI HTGetLoaderVersion(
  UINT32 *result
) {
  *result = HTML_VERSION;
}

HTMLAPIATTR VOID HTMLAPI HTGetLoaderVersionName(
  LPSTR result,
  UINT32 max
) {
  if (!max)
    return;
  if (max == 1)
    *result = '\0';
  strncpy(result, HTML_VERSION_NAME, max - 1);
}

HTMLAPIATTR VOID HTMLAPI HTGetGameStatus(
  HTGameStatus *status
) {
  if (status)
    *status = gGameStatus;
}

HTMLAPIATTR VOID HTMLAPI HTGetGameExeFolder(
  LPSTR result,
  UINT64 maxLen
) {
  if (!result)
    return;
  strcpy_s(result, maxLen, gPathGameExe);
}

HTMLAPIATTR VOID HTMLAPI HTGetModFolder(
  LPSTR result,
  UINT64 maxLen
) {
  if (!result)
    return;
  strcpy_s(result, maxLen, gPathMods);
}

HTMLAPIATTR HMODULE HTMLAPI HTGetModuleHandle(
  LPCSTR module
) {
  HMODULE result = nullptr;

  if (!module) {
    // Get caller's module handle.
    void *callerAddr = __builtin_return_address(0);
    if (!GetModuleHandleExA(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (LPCSTR)callerAddr,
      &result
    ))
      return nullptr;

    if (!HTiCheckHandleType((HTHandle)result, HTHandleType_Mod))
      return nullptr;

    return result;
  }

  // Get module handle from name.
  auto it = gModDataLoader.find(module);
  if (it == gModDataLoader.end())
    return nullptr;

  return it->second.runtime->handle;
}

HTMLAPIATTR HTHandle HTMLAPI HTGetModManifest(
  HMODULE hModule
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  if (!hModule)
    return HT_INVALID_HANDLE;

  ModRuntime *rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HT_INVALID_HANDLE;

  HTiRegisterHandle((HTHandle)rt->manifest, HTHandleType_Manifest);

  return (HTHandle)rt->manifest;
}

HTMLAPIATTR UINT32 HTMLAPI HTGetModInfoFrom(
  HTHandle hManifest,
  HTModInfoFields info,
  LPVOID out,
  UINT32 maxLen
) {
  if (!HTiCheckHandleType(hManifest, HTHandleType_Manifest))
    return HTiErrAndRet(HTError_InvalidHandle, 0);

  ModManifest *manifest = (ModManifest *)hManifest;
  u64 size;
  void *result;

  if (maxLen && !out)
    return HTiErrAndRet(HTError_InvalidParam, 0);

  switch (info) {
    case HTModInfoFields_ModName:
      size = manifest->modName.length() + 1;
      result = (void *)manifest->modName.c_str();
      break;
    case HTModInfoFields_PackageName:
      size = manifest->meta.packageName.length() + 1;
      result = (void *)manifest->meta.packageName.c_str();
      break;
    case HTModInfoFields_Folder:
      size = (manifest->paths.folder.length() + 1) * sizeof(wchar_t);
      result = (void *)manifest->meta.packageName.c_str();
      break;
    default:
      return HTiErrAndRet(HTError_InvalidParam, 0);
  }

  if (!maxLen)
    return HTiErrAndRet(HTError_Success, size);
  if (size > maxLen)
    return HTiErrAndRet(HTError_InsufficientBuffer, 0);

  memcpy(out, result, size);

  return HTiErrAndRet(HTError_Success, size);
}

HTMLAPIATTR HTStatus HTMLAPI HTImGuiDispatch(
  HTImGuiContexts *context
) {
  if (!context)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!ImGui::GetCurrentContext())
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  context->pImGui = ImGui::GetCurrentContext();
  ImGui::GetAllocatorFunctions(
    (ImGuiMemAllocFunc *)&context->pImAllocatorAllocFunc,
    (ImGuiMemFreeFunc *)&context->pImAllocatorFreeFunc,
    (void **)&context->pImAllocatorUserData);

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTOptionGetCustom(
  HMODULE hModule,
  LPCSTR key,
  HTOptionType type,
  LPVOID data,
  UINT32 *cch
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  // Param validation.
  if (!hModule)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!data && type != HTOptionType_String)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!data && !cch)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);

  ModRuntime *rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);
  
  auto it = rt->options.find(key);
  if (it == rt->options.end())
    return HTiErrAndRet(HTError_NotFound, HT_FAIL);

  ModCustomOption &option = it->second;
  if (option.type != type)
    return HTiErrAndRet(HTError_NotFound, HT_FAIL);
  
  switch(type) {
    case HTOptionType_Bool:
      *(bool *)data = option.valueBool;
      break;
    case HTOptionType_Double:
      *(f64 *)data = option.valueNumber;
      break;
    case HTOptionType_String:
      if (!data && cch)
        // Returns the required byte count in `cch`.
        *cch = option.valueString.length() + 1;
      else if (data && !cch)
        // Copy the entire string.
        strcpy((char *)data, option.valueString.c_str());
      else if (data && cch) {
        // Copy string with length limit. If the value `cch` points to is 0,
        // the function acts the same as `cch` == NULL.
        strncpy((char *)data, option.valueString.c_str(), *cch - 1);
        *cch = std::min(*cch, (UINT32)(option.valueString.length() + 1));
      }
      break;
    default:
      return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  }

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTOptionSetCustom(
  HMODULE hModule,
  LPCSTR key,
  HTOptionType type,
  LPCVOID data
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  if (!hModule || !data)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);

  ModRuntime *rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);
  
  switch(type) {
    case HTOptionType_Bool:
      rt->options[key].valueBool = *(bool *)data;
      break;
    case HTOptionType_Double:
      rt->options[key].valueNumber = *(f64 *)data;
      break;
    case HTOptionType_String:
      rt->options[key].valueString = (const char *)data;
      break;
    default:
      return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  }
  rt->options[key].type = type;

  HTiOptionsMarkDirty();

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR VOID HTMLAPI HTGetActiveBackendName(
  LPSTR gl,
  LPSTR game
) {
  if (gl) {
    strncpy(gl, gActiveGLBackendName, 31);
    gl[31] = 0;
  }

  if (game) {
    strncpy(game, gActiveGameBackendName, 31);
    game[31] = 0;
  }
}

static u32 copyOrReturn(
  wchar_t *buffer,
  const std::wstring &str,
  u32 maxLen
) {
  if (str.size() >= maxLen)
    return 0;
  if (!buffer)
    return str.size() + 1;

  wcsncpy(buffer, str.c_str(), maxLen - 1);
  buffer[maxLen - 1] = 0;

  return str.size() + 1;
}

HTMLAPIATTR UINT32 HTMLAPI HTPathNormalize(
  LPWSTR result,
  LPCWSTR path,
  UINT32 maxLen
) {
  if (!path)
    return 0;

  std::wstring normalized = HTiPathNormalize(path);

  return copyOrReturn(
    result,
    normalized,
    maxLen);
}

HTMLAPIATTR UINT32 HTMLAPI HTPathJoin(
  LPWSTR result,
  LPCWSTR *paths,
  UINT32 maxLen
) {
  if (!paths)
    return 0;

  std::vector<std::wstring> arrayPaths;
  for (int i = 0; paths[i]; i++)
    arrayPaths.push_back(paths[i]);

  std::wstring joined = HTiPathJoin(arrayPaths);

  return copyOrReturn(
    result,
    joined,
    maxLen);
}

HTMLAPIATTR UINT32 HTMLAPI HTPathResolve(
  LPWSTR result,
  LPCWSTR *paths,
  UINT32 maxLen
) {
  if (!paths)
    return 0;

  std::vector<std::wstring> arrayPaths;
  for (int i = 0; paths[i]; i++)
    arrayPaths.push_back(paths[i]);

  std::wstring resolved = HTiPathResolve(arrayPaths);

  return copyOrReturn(
    result,
    resolved,
    maxLen);
}

HTMLAPIATTR UINT32 HTMLAPI HTPathRelative(
  LPWSTR result,
  LPCWSTR path1,
  LPCWSTR path2,
  UINT32 maxLen
) {
  if (!path1 || !path2)
    return 0;

  std::wstring relative = HTiPathRelative(path1, path2);

  return copyOrReturn(
    result,
    relative,
    maxLen);
}

HTMLAPIATTR UINT32 HTMLAPI HTPathIsAbsolute(
  LPCWSTR path
) {
  return HTiPathIsAbsolute(path);
}

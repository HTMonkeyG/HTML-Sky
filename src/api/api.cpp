// ----------------------------------------------------------------------------
// Basic APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include "includes/htmodloader.h"
#include "htinternal.h"

static thread_local HTError gLastError = HTError_Success;

HTMLAPIATTR void HTMLAPI HTSetLastError(
  HTError dwError
) {
  gLastError = dwError;
}

HTMLAPIATTR HTError HTMLAPI HTGetLastError() {
  return gLastError;
}

HTMLAPIATTR void HTMLAPI HTGetGameStatus(
  HTGameStatus *status
) {
  if (status)
    *status = gGameStatus;
}

HTMLAPIATTR void HTMLAPI HTGetGameExeFolder(
  LPSTR result,
  UINT64 maxLen
) {
  if (!result)
    return;
  strcpy_s(result, maxLen, gPathGameExe);
}

HTMLAPIATTR void HTMLAPI HTGetModFolder(
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

HTMLAPIATTR u32 HTMLAPI HTGetModInfoFrom(
  HTHandle hManifest,
  HTModInfoFields info,
  LPVOID out,
  UINT32 maxLen
) {
  if (!HTiCheckHandleType(hManifest, HTHandleType_Manifest))
    return 0;

  ModManifest *manifest = (ModManifest *)hManifest;
  u64 size;

  if (maxLen && !out)
    return 0;

  switch (info) {
    case HTModInfoFields_ModName:
      size = manifest->modName.length();
      if (!maxLen)
        return size;
      if (size > maxLen)
        return 0;
      strcpy_s((char *)out, maxLen, manifest->modName.c_str());
      return size;
    case HTModInfoFields_PackageName:
      size = manifest->meta.packageName.length();
      if (!maxLen)
        return size;
      if (size > maxLen)
        return 0;
      strcpy_s((char *)out, maxLen, manifest->meta.packageName.c_str());
      return size;
    case HTModInfoFields_Folder:
      size = manifest->paths.folder.length() * sizeof(wchar_t);
      if (!maxLen)
        return size;
      if (size + 2 > maxLen)
        return 0;
      memcpy_s(out, maxLen, manifest->meta.packageName.c_str(), size);
      return size;
    default:
      break;
  }

  return 0;
}

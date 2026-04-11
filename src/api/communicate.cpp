// ----------------------------------------------------------------------------
// Mod communication APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <string>
#include <map>
#include <shared_mutex>
#include <set>
#include "includes/htmodloader.h"
#include "htinternal.hpp"

typedef std::pair<PFN_HTEventCallback, HMODULE> EventCallbackInfo;
typedef std::set<EventCallbackInfo> EventSet;

// All event callbacks.
static std::map<std::string, EventSet> gEventCallbacks;
// This mutex is only for gEventCallbacks.
static std::shared_mutex gMutex;

HTMLAPIATTR PFN_HTVoidFunction HTMLAPI HTGetProcAddr(
  HMODULE hModule,
  LPCSTR name
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  if (!hModule || !name)
    return HTiErrAndRet(HTError_InvalidParam, nullptr);
  if (!HTiCheckHandleType(hModule, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, nullptr);

  ModRuntime *rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidParam, nullptr);

  auto it = rt->functions.find(name);
  if (it == rt->functions.end())
    return HTiErrAndRet(HTError_NoMoreMatches, nullptr);

  return HTiErrAndRet(HTError_Success, it->second);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommRegFunction(
  HMODULE hModule,
  LPCSTR name,
  PFN_HTVoidFunction func
) {
  std::lock_guard<std::mutex> lock(gModDataLock);
  ModRuntime *rt;

  if (!hModule || !name || !func)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModule, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  rt->functions[name] = func;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommOnEvent(
  HMODULE hModuleOwner,
  LPCSTR name,
  PFN_HTEventCallback callback
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  if (!hModuleOwner || !name || !callback)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  // Check the protection of given address.
  if (!HTiIsExecutableAddr((void *)callback))
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  // Insert our event callback with its owner.
  gEventCallbacks[name].insert(std::make_pair(callback, hModuleOwner));

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommOffEvent(
  HMODULE hModuleOwner,
  LPCSTR name,
  PFN_HTEventCallback callback
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  if (!hModuleOwner || !name || !callback)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  auto it = gEventCallbacks.find(name);
  if (it == gEventCallbacks.end())
    return HTiErrAndRet(HTError_NoMoreMatches, HT_FAIL);

  it->second.erase(std::make_pair(callback, hModuleOwner));

  if (it->second.empty())
    gEventCallbacks.erase(it);

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommEmitEvent(
  LPCSTR name,
  LPVOID reserved,
  LPVOID data
) {
  std::vector<EventCallbackInfo> localCallbacks;

  if (!name)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);

  {
    std::shared_lock<std::shared_mutex> lock(gMutex);

    auto it = gEventCallbacks.find(name);
    if (it == gEventCallbacks.end())
      return HTiErrAndRet(HTError_NoMoreMatches, HT_FAIL);
    if (it->second.empty())
      return HTiErrAndRet(HTError_Success, HT_SUCCESS);

    // Save a local copy of the callbacks to avoid deadlock.
    localCallbacks.assign(it->second.begin(), it->second.end());
  }

  for (const auto &it: localCallbacks) {
    PFN_HTEventCallback cb = it.first;
    if (cb)
      cb(data);
  }

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

void HTiRemoveAllEventCallbacksOf(
  HMODULE hModuleOwner
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  for (auto itEvent = gEventCallbacks.begin(); itEvent != gEventCallbacks.end(); ) {
    auto &callbacks = itEvent->second;

    // Walks along all callbacks of current event.
    for (auto itCallback = callbacks.begin(); itCallback != callbacks.end(); ) {
      if (itCallback->second == hModuleOwner)
        // Remove the callback from the mod.
        itCallback = callbacks.erase(itCallback);
      else
        itCallback++;

      // Remove the event entry if the event is empty.
      if (callbacks.empty())
        itEvent = gEventCallbacks.erase(itEvent);
      else
        itEvent++;
    }
  }
}

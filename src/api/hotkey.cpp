// ----------------------------------------------------------------------------
// Hotkey APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <string>
#include "imgui.h"
#include "utils/texts.h"
#include "includes/htmodloader.h"
#include "htinternal.h"

// Cooldown timer in frames.
#define HOTKEY_MODIFY_COOLDOWN 5

static std::map<HTKeyCode, std::set<ModKeyBind *>> gHotkeyCallbacks;
static i32 gKeyModifyCooldown = 0;

/**
 * Dispatch a key event to all related callbacks.
 */
void HTHotkeyDispatch(
  HTKeyCode key,
  HTKeyEventFlags flags,
  u08 *userSetBlocked
) {
  std::vector<ModKeyBind *> localCallbacks;
  bool blocked = flags & HTKeyEventFlags_Blocked;

  if (gKeyModifyCooldown)
    return;
  if (flags & HTKeyEventFlags_Repeat)
    // We only dispatch the "real" physical key event.
    return;
  flags &= HTKeyEventFlags_Mask;

  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    auto it = gHotkeyCallbacks.find(key);
    if (it == gHotkeyCallbacks.end())
      return;
    localCallbacks.assign(it->second.begin(), it->second.end());
  }

  HTKeyEvent event;
  event.down = flags == HTKeyEventFlags_Down;
  event.key = key;
  event.flags = flags;
  event.preventFlags = HTKeyEventPreventFlags_None;

  for (auto it = localCallbacks.begin(); it != localCallbacks.end(); it++) {
    if (blocked && !((*it)->flags & HTHotkeyFlags_NoBlock))
      // Skip all key binds which isn't marked as NoBlock when the key event
      // is intented to be blocked.
      continue;

    // Set the key state.
    (*it)->isDown = flags == HTKeyEventFlags_Down;

    // Trigger the event callback.
    PFN_HTHotkeyCallback cb = (*it)->listener;
    if (cb) {
      event.hKey = *it;
      cb(&event);
    }

    if (event.preventFlags & HTKeyEventPreventFlags_Next)
      // Prevent the event pass to the next callback.
      break;
  }

  if (event.preventFlags & HTKeyEventPreventFlags_Game)
    *userSetBlocked = 1;
}

/**
 * Call this function to set the cooldown and block key events.
 */
void HTHotkeySetCooldown() {
  gKeyModifyCooldown = HOTKEY_MODIFY_COOLDOWN;
}

/**
 * After changing the key binding, there is a cooldown to prevent key message
 * transmission. This function is used to update the cooldown.
 * 
 * Call this function every frame.
 */
void HTHotkeyUpdateCooldown() {
  if (gKeyModifyCooldown > 0)
    gKeyModifyCooldown--;
}

HTMLAPIATTR HTHandle HTMLAPI HTHotkeyRegister(
  HMODULE hModule,
  LPCSTR name,
  HTKeyCode defaultCode
) {
  return HTHotkeyRegisterEx(hModule, name, defaultCode, HTHotkeyFlags_None);
}

HTMLAPIATTR HTHandle HTMLAPI HTHotkeyRegisterEx(
  HMODULE hModule,
  LPCSTR name,
  HTKeyCode defaultCode,
  HTHotkeyFlags flags
) {
  std::lock_guard<std::mutex> lock(gModDataLock);
  ModRuntime *rt;
  ModKeyBind *result;

  if (!hModule || !name)
    // Invalid param.
    return (HTHandle)HTSetErrorAndReturn(HTError_InvalidParam, HT_INVALID_HANDLE);

  rt = HTiGetModRuntime(hModule);
  if (!rt)
    // Module handle is invalid.
    return (HTHandle)HTSetErrorAndReturn(HTError_InvalidHandle, HT_INVALID_HANDLE);

  if (defaultCode != HTKey_None && !HTiIsNamedKey(defaultCode))
    // Register fails when the defaultCode is invalid.
    return (HTHandle)HTSetErrorAndReturn(HTError_InvalidParam, HT_INVALID_HANDLE);

  auto it = rt->keyBinds.find(name);
  if (it != rt->keyBinds.end())
    // We won't override prewritten key code.
    result = &it->second;
  else {
    result = &rt->keyBinds[name];

    // The first time to register the key, set the key code to default.
    result->key = defaultCode;
    result->isRegistered = 1;
  }

  result->defaultKey = defaultCode;
  result->keyName = name;
  result->displayName = name;
  result->flags = flags;

  gHotkeyCallbacks[result->key].insert(result);
  HTiRegisterHandle(result, HTHandleType_Hotkey);

  return HTSetErrorAndReturn(HTError_Success, result);
}

static HTStatus HTHotkeyBindEx(
  HTHandle hKey,
  HTKeyCode keyCode,
  bool reset
) {
  ModKeyBind *kb;
  HTKeyEvent event;

  if (!hKey)
    return HTSetErrorAndReturn(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hKey, HTHandleType_Hotkey))
    return HTSetErrorAndReturn(HTError_InvalidHandle, HT_FAIL);

  kb = (ModKeyBind *)hKey;

  if (reset)
    keyCode = kb->defaultKey;
  if (kb->key == keyCode)
    // If the key is not changed, we won't actually set the key.
    return HTSetErrorAndReturn(HTError_Success, HT_SUCCESS);

  event.flags = reset
    ? HTKeyEventFlags_ChangeBind
    : HTKeyEventFlags_ResetBind;
  event.hKey = (HTHandle)kb;
  event.key = kb->key;
  event.down = 0;

  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    gHotkeyCallbacks[kb->key].erase(kb);
    kb->key = keyCode;
    if (keyCode)
      // We won't dispatch HTKey_None as an event.
      gHotkeyCallbacks[keyCode].insert(kb);
  }

  // Trigger an event.
  if (kb->listener)
    kb->listener(&event);

  // Mark the options as "dirty", so we can save it after a delay.
  HTiOptionsMarkDirty();

  return HTSetErrorAndReturn(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTHotkeyBind(
  HTHandle hKey,
  HTKeyCode keyCode
) {
  return HTHotkeyBindEx(hKey, keyCode, false);
}

HTMLAPIATTR HTStatus HTMLAPI HTHotkeyBindReset(
  HTHandle hKey
) {
  return HTHotkeyBindEx(hKey, HTKey_None, true);
}

HTMLAPIATTR u32 HTMLAPI HTHotkeyPressed(
  HTHandle hKey
) {
  ModKeyBind *kb;

  if (!hKey)
    return 0;
  if (gKeyModifyCooldown)
    return 0;
  kb = (ModKeyBind *)hKey;

  return (u32)kb->isDown;
}

HTMLAPIATTR HTStatus HTMLAPI HTHotkeyListen(
  HTHandle hKey,
  PFN_HTHotkeyCallback callback
) {
  ModKeyBind *kb;

  if (!hKey || !callback)
    return HTSetErrorAndReturn(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hKey, HTHandleType_Hotkey))
    return HTSetErrorAndReturn(HTError_InvalidHandle, HT_FAIL);

  kb = (ModKeyBind *)hKey;
  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    kb->listener = callback;
  }

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTHotkeyUnlisten(
  HTHandle hKey,
  LPVOID reserved
) {
  ModKeyBind *kb;

  if (!hKey)
    return HTSetErrorAndReturn(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hKey, HTHandleType_Hotkey))
    return HTSetErrorAndReturn(HTError_InvalidHandle, HT_FAIL);

  kb = (ModKeyBind *)hKey;
  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    kb->listener = nullptr;
  }

  return HT_SUCCESS;
}

/**
 * Modified from ImGui. Get the name string of a key.
 */
HTMLAPIATTR const char *HTMLAPI HTHotkeyGetName(HTKeyCode key) {
  if (key == HTKey_None)
    return "None";
  if (!HTiIsNamedKey(key))
    return "Unknown";

  return HTKeyNames[key - HTKey_NamedKey_BEGIN];
}

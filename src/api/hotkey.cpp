// ----------------------------------------------------------------------------
// Hotkey APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <string>
#include "imgui.h"
#include "includes/htmodloader.h"
#include "htinternal.h"

// Cooldown timer in frames.
#define HOTKEY_MODIFY_COOLDOWN 5

static std::map<HTKeyCode, std::set<ModKeyBind *>> gHotkeyCallbacks;
static i32 gKeyModifyCooldown = 0;

/**
 * Call key event callback functions.
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

void HTHotkeySetCooldown() {
  gKeyModifyCooldown = HOTKEY_MODIFY_COOLDOWN;
}

void HTHotkeyUpdateCooldown() {
  if (gKeyModifyCooldown > 0)
    gKeyModifyCooldown--;
}

HTMLAPIATTR HTHandle HTMLAPI HTHotkeyRegister(
  HMODULE hModule,
  const char *name,
  HTKeyCode defaultCode
) {
  return HTHotkeyRegisterEx(hModule, name, defaultCode, HTHotkeyFlags_None);
}

HTMLAPIATTR HTHandle HTMLAPI HTHotkeyRegisterEx(
  HMODULE hModule,
  const char *name,
  HTKeyCode defaultCode,
  HTHotkeyFlags flags
) {
  std::lock_guard<std::mutex> lock(gModDataLock);
  ModRuntime *rt;
  ModKeyBind *result;

  if (!hModule || !name)
    return HT_INVALID_HANDLE;

  rt = getModRuntime(hModule);
  if (!rt)
    return HT_INVALID_HANDLE;
  
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
  registerHandle(result, HTHandleType_Hotkey);

  return result;
}

HTMLAPIATTR HTStatus HTMLAPI HTHotkeyBind(
  HTHandle hKey,
  HTKeyCode key
) {
  ModKeyBind *kb;
  HTKeyEvent event;

  if (!hKey)
    return HT_FAIL;

  kb = (ModKeyBind *)hKey;

  if (key == HTKey_None) {
    key = kb->defaultKey;
    event.flags = HTKeyEventFlags_ResetBind;
  } else
    event.flags = HTKeyEventFlags_ChangeBind;
  if (kb->key == key) 
    // If the key is not changed, we won't actually set the key.
    return HT_SUCCESS;

  event.hKey = (HTHandle)kb;
  event.key = kb->key;
  event.down = 0;

  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    gHotkeyCallbacks[kb->key].erase(kb);
    kb->key = key;
    gHotkeyCallbacks[key].insert(kb);
  }

  // Trigger an event.
  if (kb->listener)
    kb->listener(&event);

  return HT_SUCCESS;
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
    return HT_FAIL;

  kb = (ModKeyBind *)hKey;
  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    kb->listener = callback;
  }

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTHotkeyUnlisten(
  HTHandle hKey,
  void *reserved
) {
  ModKeyBind *kb;

  if (!hKey)
    return HT_FAIL;

  kb = (ModKeyBind *)hKey;
  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    kb->listener = nullptr;
  }

  return HT_SUCCESS;
}

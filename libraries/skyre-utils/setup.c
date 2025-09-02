#include <windows.h>

#include "sigscan.h"
#include "log.h"
#include "setup.h"
#include "MinHook.h"

i08 skyre_setupFuncWithSig(const Signature_t **sigcodes, void **functions, i32 num) {
  i08 r = 1;
  i32 length;
  void *p;
  const Signature_t *sig;

  LOGI("Scaning functions...\n");

  if (!functions || !sigcodes)
    return 0;

  length = num;
  for (i32 i = 0; i < length; i++) {
    sig = sigcodes[i];
    if (!sig)
      continue;
    if (sig->indirect == SKYRE_SETUP_E8)
      p = skyre_sigScanE8("Sky.exe", sig->sig, sig->offset);
    else if (sig->indirect == SKYRE_SETUP_FF15)
      p = skyre_sigScanFF15("Sky.exe", sig->sig, sig->offset);
    else
      p = skyre_sigScan("Sky.exe", sig->sig, sig->offset);
    
    functions[i] = p;

    if (p)
      LOGI("Found %s: 0x%p\n", sig->name, p);
    else {
      LOGE("Scan %s failed!\n", sig->name);
      r = 0;
    }
  }
  
  return r;
}

i08 skyre_createHookWithSig(FuncSig_t *func, void *detour) {
  void *p;
  const Signature_t *sig;

  if (!func || !detour)
    return 0;

  sig = &func->sig;
  if (sig->indirect == SKYRE_SETUP_E8)
    p = skyre_sigScanE8("Sky.exe", sig->sig, sig->offset);
  else if (sig->indirect == SKYRE_SETUP_FF15)
    p = skyre_sigScanFF15("Sky.exe", sig->sig, sig->offset);
  else
    p = skyre_sigScan("Sky.exe", sig->sig, sig->offset);

  if (p)
    LOGI("Found %s: 0x%p\n", sig->name, p);
  else {
    LOGE("Scan %s failed!\n", sig->name);
    return 0;
  }

  if (MH_CreateHook(p, detour, &func->fn) != MH_OK)
    return 0;
  
  return 1;
}

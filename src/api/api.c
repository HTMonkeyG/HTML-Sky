// ----------------------------------------------------------------------------
// Basic APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include "globals.h"

void HTGetGameStatus(HTGameStatus *status) {
  if (status)
    *status = gGameStatus;
}

void HTGetGameExeFolder(char *result, u64 maxLen) {
  if (!result)
    return;
  strcpy_s(result, maxLen, gPathGameExe);
}

void HTGetModFolder(char *result, u64 maxLen) {
  if (!result)
    return;
  strcpy_s(result, maxLen, gPathMods);
}

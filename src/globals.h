#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <windows.h>
#include "aliases.h"
#include "htmodloader.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HTGameStatus gGameStatus;
extern char gPathDll[MAX_PATH]
  , gPathGameExe[MAX_PATH]
  , gPathLayerConfig[MAX_PATH]
  , gPathMods[MAX_PATH];
extern wchar_t gPathModsWide[MAX_PATH];
extern HANDLE gHeap
  , gEventGuiInit;

#ifdef __cplusplus
}
#endif

#endif

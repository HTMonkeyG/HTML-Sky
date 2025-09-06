#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <windows.h>
#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  // Uninitialized state.
  HT_EDITION_UNKNOWN = 0,
  // Chinese edition.
  HT_EDITION_CHINESE,
  // International edition.
  HT_EDITION_INTERNATIONAL
} HTGameEdition;

typedef struct {
  // Base address of game executable file.
  void *baseAddr;
  // The edition of the game.
  HTGameEdition edition;
  // The window handle of the game.
  HWND window;
  // The process id of the game.
  DWORD pid;
} HTGameStatus;

extern HTGameStatus gGameStatus;

#ifdef __cplusplus
}
#endif

#endif

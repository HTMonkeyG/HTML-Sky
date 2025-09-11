// ----------------------------------------------------------------------------
// Built-in global variables of the mod loader. Mods MUST NOT access these
// variables directly, use HTML APIs to get a copy of them instead.
// ----------------------------------------------------------------------------
#include <windows.h>
#include "globals.h"

// Game basic informations.
HTGameStatus gGameStatus = {0};

// The folder path where the DLL is located.
char gPathDll[MAX_PATH] = {0};
// The folder path where the game executable is located. In most cases the
// same as gPathDll.
char gPathGameExe[MAX_PATH] = {0};
// Path to the Vulkan layer config json file.
char gPathLayerConfig[MAX_PATH] = {0};
// Path to the mods folder.
char gPathMods[MAX_PATH] = {0};
// Path to the mods folder, in wide char.
wchar_t gPathModsWide[MAX_PATH] = {0};

// Independent heap.
HANDLE gHeap = NULL;
// This event is set when the gui is completely inited and begins rendering.
HANDLE gEventGuiInit = NULL;

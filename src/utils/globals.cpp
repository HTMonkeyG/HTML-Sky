// ----------------------------------------------------------------------------
// Built-in global variables of the mod loader. Mods MUST NOT access these
// variables directly, use HTML APIs to get a copy of them instead.
// ----------------------------------------------------------------------------
#include <windows.h>
#include "htinternal.hpp"

// Game basic informations.
HTGameStatus gGameStatus = {0};

// The folder path where the DLL is located.
char gPathDll[MAX_PATH] = {0};
// The folder path where the game executable is located. In most cases the
// same as gPathDll.
char gPathGameExe[MAX_PATH] = {0};
// Path to the HTML data folder.
char gPathData[MAX_PATH] = {0};
// Path to the mods folder.
char gPathMods[MAX_PATH] = {0};
// Path to the data folder, in wide char.
wchar_t gPathDataWide[MAX_PATH] = {0};
// Path to the mods folder, in wide char.
wchar_t gPathModsWide[MAX_PATH] = {0};
// Path to ImGui .ini file. This string is formatted in UTF-8.
char gPathGuiIni[MAX_PATH] = {0};

// Mod loader dll handle.
HMODULE gModLoaderHandle = NULL;
// Independent heap.
HANDLE gHeap = NULL;
// This event is set when the gui is completely inited and begins rendering.
HANDLE gEventGuiInit = NULL;

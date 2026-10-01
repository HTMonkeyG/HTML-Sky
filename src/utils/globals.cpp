// ----------------------------------------------------------------------------
// Built-in global variables of the mod loader. Mods MUST NOT access these
// variables directly, use HTML APIs to get a copy of them instead.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <string>
#include "htinternal.hpp"

// Game basic informations.
HTGameStatus gGameStatus = {0};

// The folder path where the DLL is located.
std::string gPathDll;
// The folder path where the game executable is located. In most cases the
// same as gPathDll.
std::string gPathGameExe;
// Path to the HTML data folder.
std::string gPathData;
// Path to the mods folder.
std::string gPathMods;
// Path to the data folder, in wide char.
std::wstring gPathDataWide;
// Path to the mods folder, in wide char.
std::wstring gPathModsWide;
// Path to ImGui .ini file. This string is formatted in UTF-8.
std::string gPathGuiIni;

// Mod loader dll handle.
HMODULE gModLoaderHandle = NULL;
// Independent heap.
HANDLE gHeap = NULL;
// This event is set when the gui is completely inited and begins rendering.
HANDLE gEventGuiInit = NULL;
HANDLE gInitThread = NULL;
std::atomic<bool> gLoaderShuttingDown{false};
std::atomic<bool> gLoaderInitialized{false};

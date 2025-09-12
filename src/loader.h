#ifndef __LOADER_H__
#define __LOADER_H__

#include <string>
#include <unordered_map>
#include <vector>
#include "htmodloader.h"

struct ModPaths {
  // The folder contains manifest.json.
  std::wstring folder;
  // "main" in manifest.json. The name of the main executable file of the mod.
  std::wstring dll;
  // The path to manifest.json.
  std::wstring json;
};

struct ModMeta {
  // This name is used to identify mods and add dependencies, and must be
  // unique for every mod.
  std::string packageName;
  // The version number of the mod.
  u32 version[3];
};

struct ModRuntime {
  HMODULE handle;
};

struct ModManifest {
  // Mod identification data.
  ModMeta meta;
  // Paths to the related files of the mod.
  ModPaths paths;
  // This name is used to display mod basic information.
  std::string modName;
  // The description of the mod.
  std::string description;
  // The author of the mod.
  std::string author;
  // Game edition the mod supports.
  u08 gameEditionFlags;
  // Dependencies of the mod.
  std::vector<ModMeta> dependencies;
  // Mod runtime data.
  ModRuntime runtime;
};

extern std::unordered_map<std::string, ModManifest> gModDataLoader;

extern "C" {
  HTStatus HTLoadMods();
  void HTLoadSingleMod();
  void HTUnloadSingleMod();
  HTStatus HTInjectDll(const wchar_t *path);
  HTStatus HTRejectDll();
}

#endif

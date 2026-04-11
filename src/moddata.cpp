#include "htinternal.hpp"

std::map<std::string, ModManifest> gModDataLoader;
std::map<HMODULE, ModRuntime> gModDataRuntime;
std::unordered_map<HTHandle, HTHandleType> gHandleTypes;
std::mutex gModDataLock;

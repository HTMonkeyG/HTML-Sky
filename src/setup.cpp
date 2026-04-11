#include <mutex>

#include "htinternal.hpp"

static std::mutex gGameStatusMutex;

void HTiSetGameStatus(
  HTGameStatus *status
) {
  std::lock_guard<std::mutex> lock(gGameStatusMutex);

  if (gGameStatus.edition)
    // The game is initialized.
    return;

  gGameStatus = *status;

  LOGI("Game info: \n");
  LOGI("  pid = %lu\n", gGameStatus.pid);
  LOGI("  baseAddr = 0x%p\n", gGameStatus.baseAddr);
}

void HTiSetupAll() {
  std::wstring optionsPath(gPathDataWide);
  optionsPath += L"\\options.json";
  HTiOptionsLoadFromFile(optionsPath.c_str());

  HTiLoadMods();
}

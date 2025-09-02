#include "dllmain.h"

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    HMODULE hWinHttp = LoadLibraryA("C:\\Windows\\System32\\winhttp.dll");
    proxy_importFunctions(hWinHttp);
    MH_Initialize();
    
  } else if (dwReason == DLL_PROCESS_DETACH) {
  }

  return TRUE;
}

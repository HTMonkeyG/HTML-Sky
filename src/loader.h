#ifndef __LOADER_H__
#define __LOADER_H__

#ifdef __cplusplus
extern "C" {
#endif

void HTLoadMods();
void HTLoadSingleMod();
void HTUnloadSingleMod();
void HTInjectDll();
void HTRejectDll();

#ifdef __cplusplus
}
#endif

#endif
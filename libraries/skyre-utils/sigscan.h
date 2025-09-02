#ifndef __SIGSCAN_H__
#define __SIGSCAN_H__

#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

void *skyre_sigScan(const char *moduleName, const char *sig, i32 offset);
void *skyre_sigScanE8(const char *moduleName, const char *sig, i32 offset);
void *skyre_sigScanFF15(const char *moduleName, const char *sig, i32 offset);
i08 skyre_sigCheckProcess(void *addr, char *sig);

#ifdef __cplusplus
}
#endif

#endif

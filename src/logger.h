#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGI(format, ...) HTLogA("[INFO] " format, ##__VA_ARGS__)
#define WLOGI(format, ...) HTLogW(L"[INFO] " format, ##__VA_ARGS__)
#define LOGW(format, ...) HTLogA("[WARN] " format, ##__VA_ARGS__)
#define WLOGW(format, ...) HTLogW(L"[WARN] " format, ##__VA_ARGS__)
#define LOGE(format, ...) HTLogA("[ERR] " format, ##__VA_ARGS__)
#define WLOGE(format, ...) HTLogW(L"[ERR] " format, ##__VA_ARGS__)
#define LOGEF(format, ...) HTLogA("[ERR][FATAL] " format, ##__VA_ARGS__)
#define WLOGEF(format, ...) HTLogW(L"[ERR][FATAL] " format, ##__VA_ARGS__)

void HTInitLogger(const wchar_t *fileName, i08 allocConsole);
void HTLogA(const char *format, ...);
void HTLogW(const wchar_t *format, ...);

#ifdef __cplusplus
}
#endif

#endif
#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

#define DEBUG_CONSOLE

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG_CONSOLE
#define LOGI(format, ...) (skyre_logImp("[INFO] " format, ##__VA_ARGS__))
#define WLOGI(format, ...) (skyre_wlogImp(L"[INFO] " format, ##__VA_ARGS__))
#define LOGW(format, ...) (skyre_logImp("[WARN] " format, ##__VA_ARGS__))
#define WLOGW(format, ...) (skyre_wlogImp(L"[WARN] " format, ##__VA_ARGS__))
#define LOGE(format, ...) (skyre_logImp("[ERR] " format, ##__VA_ARGS__))
#define WLOGE(format, ...) (skyre_wlogImp(L"[ERR] " format, ##__VA_ARGS__))
#define LOGEF(format, ...) (skyre_logImp("[ERR][FATAL] " format, ##__VA_ARGS__))
#define WLOGEF(format, ...) (skyre_wlogImp(L"[ERR][FATAL] " format, ##__VA_ARGS__))
#else
#define LOGI(format, ...) /* Nothing. */
#define WLOGI(format, ...) /* Nothing. */
#define LOGW(format, ...) /* Nothing. */
#define WLOGW(format, ...) /* Nothing. */
#define LOGE(format, ...) /* Nothing. */
#define WLOGE(format, ...) /* Nothing. */
#define LOGEF(format, ...) /* Nothing. */
#define WLOGEF(format, ...) /* Nothing. */
#endif

void skyre_recreateConsole();
void skyre_logImp(const char *format, ...);
void skyre_wlogImp(const wchar_t *format, ...);

#ifdef __cplusplus
}
#endif

#endif

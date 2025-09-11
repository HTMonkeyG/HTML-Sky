#include <windows.h>
#include <ntstatus.h>
#include <stdio.h>
#include <unordered_map>
#include "MinHook.h"

#include "aliases.h"
#include "globals.h"
#include "logger.h"
#include "loader.h"
#include "proxy/winhttp-proxy.h"

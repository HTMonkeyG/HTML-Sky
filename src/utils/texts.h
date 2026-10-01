#define HTTexts_ModLoaderPackageName "htmodloader"
#define HTTexts_ModLoaderName "HT's Mod Loader"
#define HTTexts_ModLoaderDesc "HTML basic apis."

// Default contents of html-config.json. This doubles as the Vulkan layer
// manifest (consumed by the Vulkan loader) and HTML's own loader config. The
// "ht_mod_loader" section is ignored by the Vulkan loader and read by HTML:
//   target_executable - overrides the game executable name used for process
//                       detection, e.g. when the exe was renamed to
//                       "Sky-test.exe". Empty = use backend defaults.
//   backend           - force a specific backend by name (e.g. "Impl_Sky").
//                       Empty = auto-detect.
#define HTTexts_DefaultLayerConfig "{\n"\
  "\"file_format_version\": \"1.0.0\",\n"\
  "\"layer\": {\n"\
    "\"name\": \"VK_LAYER_HT_MOD_LOADER\",\n"\
    "\"type\": \"GLOBAL\",\n"\
    "\"api_version\": \"1.3\",\n"\
    "\"library_path\":\".\\\\winhttp.dll\",\n"\
    "\"implementation_version\": \"1\",\n"\
    "\"description\": \"Layer for HT's Mod Loader\",\n"\
    "\"functions\":{\n"\
      "\"vkGetInstanceProcAddr\": \"HT_vkGetInstanceProcAddr\",\n"\
      "\"vkGetDeviceProcAddr\": \"HT_vkGetDeviceProcAddr\"\n"\
    "},\n"\
    "\"disable_environment\":{\n"\
      "\"DISABLE_HT_MOD_LOADER\":\"1\"\n"\
    "}\n"\
  "},\n"\
  "\"ht_mod_loader\": {\n"\
    "\"target_executable\": \"\",\n"\
    "\"backend\": \"\"\n"\
  "}\n"\
"}"

// Modified from ImGui.
static const char *const HTKeyNames[] = {
  "Tab", "LeftArrow", "RightArrow", "UpArrow", "DownArrow", "PageUp", "PageDown",
  "Home", "End", "Insert", "Delete", "Backspace", "Space", "Enter", "Escape",
  "LeftCtrl", "LeftShift", "LeftAlt", "LeftSuper", "RightCtrl", "RightShift", "RightAlt", "RightSuper", "Menu",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F", "G", "H",
  "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
  "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
  "'", ",", "-", ".", "/", ";", "=", "[",
  "\\", "]", "`", "CapsLock", "ScrollLock", "NumLock", "PrintScreen",
  "Pause", "Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6",
  "Num7", "Num8", "Num9", "Num.", "Num/", "Num*",
  "Num-", "Num+", "NumEnter", "NumEqual",
  "AppBack", "AppForward", "Oem102",
  "MouseLeft", "MouseRight", "MouseMiddle", "MouseX1", "MouseX2",
  "MWheelUp", "MWheelDown", "MWheelLeft", "MWheelRight"
};

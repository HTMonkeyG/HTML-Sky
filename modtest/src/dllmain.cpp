#include <windows.h>
#include "imgui.h"
#include "htmodloader/htmod.h"

static HTHandle hKeyTest = nullptr;
static HMODULE hModuleDll = nullptr;
static char gVersionRtName[10] = {0};
static char gGameBackendName[32] = {0};
static char gGLBackendName[32] = {0};
static unsigned int gVersionRtNum = 0;
static HTKeyEventFlags gLastKeyEventFlags = 0;
static HTKeyEventPreventFlags gKeyEventPreventFlags = 0;

static void HTTestShowMainWindow(
  float timeElapsed,
  bool *open
) {
  if (!ImGui::Begin("Mod Test", open))
    // Early out if the window is collapsed, as an optimization.
    return ImGui::End();

  ImGui::Text("HT Mod Loader test mod.");
  ImGui::Text("FPS %.1f", 1.0 / timeElapsed);
  ImGui::Text("This mod has been compiled with HTModLoader SDK v%s (%d).", HTML_VERSION_NAME, HTML_VERSION);
  ImGui::Text("This mod is running on HTModLoader v%s (%d).", gVersionRtName, gVersionRtNum);
  ImGui::Text("GL backend: %s, Game backend: %s", gGLBackendName, gGameBackendName);

  if (ImGui::CollapsingHeader("Help")) {
    ImGui::SeparatorText("ABOUT THIS DEMO:");
    ImGui::BulletText("This mod provides a template on how to write a mod on HTModLoader.");
    ImGui::BulletText("Sections below are demonstrating many aspects of the library.");

    ImGui::SeparatorText("PROGRAMMER GUIDE:");
    ImGui::BulletText("See the codes of this mod.");
    ImGui::BulletText("See comments in htmodloader.h.");
  }

  if (ImGui::CollapsingHeader("Inputs")) {
    ImGui::SeparatorText("Basic key bindings");
    ImGui::BulletText("Test key pressed: %d", HTHotkeyPressed(hKeyTest));
    ImGui::BulletText("Currently bound: %s", HTHotkeyGetName(HTHotkeyBindGet(hKeyTest)));
    ImGui::BulletText("Use \"Settings/Key Bindings\" in HTML Main Menu to change the key binding.");

    static bool preventGame = false;
    static bool preventNext = false;
    ImGui::SeparatorText("HTKeyEventPreventFlags");
    ImGui::Checkbox("HTKeyEventPreventFlags_Game", &preventGame);
    ImGui::Checkbox("HTKeyEventPreventFlags_Next", &preventNext);

    gKeyEventPreventFlags = 0;
    if (preventGame) gKeyEventPreventFlags |= HTKeyEventPreventFlags_Game;
    if (preventNext) gKeyEventPreventFlags |= HTKeyEventPreventFlags_Next;
  }

  if (ImGui::CollapsingHeader("Console")) {
    static char buf[128];

    ImGui::InputText("##ConsoleTest", buf, sizeof(buf));
    ImGui::SameLine();
    if (ImGui::Button("Print"))
      HTTellText("%s", buf);

    ImGui::Text("Use '§' (Alt+0167) to change the color of the text.");
    if (ImGui::Button("Colorful"))
      strcpy(buf, "§aC§bo§cl§do§er§1f§2u§3l");
  }

  if (ImGui::CollapsingHeader("Data Store")) {
    if (ImGui::TreeNode("Options")) {
      static char optionString[128] = {0};
      static double optionDouble = 0.0;
      static bool optionBool = false;

      ImGui::InputText("OptionText", optionString, sizeof(optionString));
      ImGui::InputDouble("OptionDouble", &optionDouble);
      ImGui::Checkbox("OptionBool", &optionBool);

      if (ImGui::Button("Save")) {
        HTOptionSetCustom(hModuleDll, "modtest:optionText", HTOptionType_String, (void *)optionString);
        HTOptionSetCustom(hModuleDll, "modtest:optionDouble", HTOptionType_Double, (void *)&optionDouble);
        HTOptionSetCustom(hModuleDll, "modtest:optionBool", HTOptionType_Bool, (void *)&optionBool);
      }

      ImGui::SameLine();

      if (ImGui::Button("Read")) {
        HTOptionGetCustom(hModuleDll, "modtest:optionText", HTOptionType_String, (void *)optionString, NULL);
        HTOptionGetCustom(hModuleDll, "modtest:optionDouble", HTOptionType_Double, (void *)&optionDouble, NULL);
        HTOptionGetCustom(hModuleDll, "modtest:optionBool", HTOptionType_Bool, (void *)&optionBool, NULL);
      }

      ImGui::TreePop();
    }

    if (ImGui::TreeNode("LevelDB")) {
      static char data[128] = {0};
      static const char key[] = "persistentData";

      ImGui::InputText("PersistentDataText", data, sizeof(data));

      if (ImGui::Button("Save"))
        // Including '\0' when storage string values.
        HTDataStore(hModuleDll, key, strlen(key), data, strlen(data) + 1);

      ImGui::SameLine();

      if (ImGui::Button("Read")) {
        unsigned long long length;
        char *dataRead = HTDataGet(hModuleDll, key, strlen(key), &length);
        memcpy_s(data, sizeof(data), dataRead, length);
        HTDataFree(dataRead);
      }

      ImGui::TreePop();
    }
  }

  ImGui::End();
}

__declspec(dllexport) void HTMLAPI HTModRenderGui(
  float timeElapsed,
  void *
) {
  HTTestShowMainWindow(timeElapsed, nullptr);
}

void HTTestKeyEventCallback(HTKeyEvent *event) {
  gLastKeyEventFlags = event->flags;
  event->preventFlags = gKeyEventPreventFlags;
}

__declspec(dllexport) HTStatus HTMLAPI HTModOnInit(
  void *
) {
  HTGetLoaderVersion(&gVersionRtNum);
  HTGetLoaderVersionName(
    gVersionRtName,
    sizeof(gVersionRtName));

  hKeyTest = HTHotkeyRegister(
    hModuleDll,
    "Test key",
    HTKey_M);
  HTHotkeyListen(hKeyTest, HTTestKeyEventCallback);

  return HT_SUCCESS;
}

__declspec(dllexport) HTStatus HTMLAPI HTModOnEnable(
  void *
) {
  HTTellText("HT Mod Test on enable.");
  HTGetActiveBackendName(
    gGLBackendName,
    gGameBackendName);
  return HT_SUCCESS;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH)
    hModuleDll = hModule;
  return TRUE;
}

// ----------------------------------------------------------------------------
// Graphics renderer.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <unordered_map>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.h"

#include "utils/globals.h"
#include "utils/texts.h"
#include "htinternal.h"

// Widget widths.
#define HOTKEY_DISPLAY_WIDTH 120.0
#define HOTKEY_RESET_WIDTH 65.0
#define HOTKEY_BUTTONS_WIDTH (HOTKEY_DISPLAY_WIDTH + HOTKEY_RESET_WIDTH)

static bool gShowMainMenu = true
  , gFirstFrame = true;
static const ImVec4 modNameColor(1, 1, 1, 1)
  , modDescColor(0.75, 0.75, 0.75, 1);
static float gMenuKeyBindMaxPosX = 0;
static ModKeyBind *gActiveKey = nullptr;
static char gFakeBuffer[5] = {0};

/**
 * Modified from ImGui. Check whether a key has name string.
 */
static inline bool isNamedKey(HTKeyCode key) {
  return key >= HTKey_NamedKey_BEGIN && key < HTKey_NamedKey_END;
}

/**
 * Modified from ImGui. Get the name string of a key.
 */
static const char *getKeyName(HTKeyCode key) {
  if (key == HTKey_None)
    return "None";
  if (!isNamedKey(key))
    return "Unknown";

  return HTKeyNames[key - HTKey_NamedKey_BEGIN];
}

/**
 * Render mod list tab item.
 */
static void HTMenuAbouts() {
  ImGui::Text("HT's Mod Loader v" HTML_VERSION_NAME " by HTMonkeyG");
  ImGui::Text("A mod loader developed for Sky:CotL.");
  ImGui::TextLinkOpenURL(
    "<https://www.github.com/HTMonkeyG/HTML-Sky>",
    "https://www.github.com/HTMonkeyG/HTML-Sky");
}

/**
 * Render mod list tab item.
 */
static void HTMenuModList() {
  i32 i = 0;

  if (!ImGui::BeginChild("##HTModList"))
    return (void)ImGui::EndChild();
  
  ImGui::PushID("##HTModListItems");
  ImVec2 size(0, ImGui::GetTextLineHeight() * 4 + 6);
  ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
  spacing.y = 1;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, spacing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
  for (auto it = gModDataLoader.begin(); it != gModDataLoader.end(); ++it, i++) {
    ModManifest &manifest = it->second;

    // Show mod info.
    ImGuiID childId = ImGui::GetID((void *)(u64)i);
    ImGui::BeginChild(
      childId,
      size,
      ImGuiChildFlags_Borders,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Show mod name.
    ImGui::TextColored(modNameColor, "%s", manifest.modName.data());

    // Show mod description.
    ImGui::PushStyleColor(ImGuiCol_Text, modDescColor);
    ImGui::TextWrapped("%s", manifest.description.data());
    ImGui::PopStyleColor();

    ImGui::EndChild();
  }
  ImGui::PopStyleVar(2);
  ImGui::PopID();

  ImGui::EndChild();
}

static HTKeyCode waitForKeyPress() {
  ImGuiIO &io = ImGui::GetIO();
  for (HTKeyCode i = HTKey_NamedKey_BEGIN; i < HTKey_NamedKey_END; i = (HTKeyCode)(i + 1)) {
    ImGuiKey keyCode = HTKeyToImGuiKey(i);
    if (!ImGui::IsKeyPressed(keyCode))
      continue;
    if (keyCode == ImGuiKey_MouseWheelY) {
      if (io.MouseWheel > 0)
        return HTKey_MouseWheelUp;
      if (io.MouseWheel < 0)
        return HTKey_MouseWheelDown;
    } else if (keyCode == ImGuiKey_MouseWheelX) {
      if (io.MouseWheelH > 0)
        return HTKey_MouseWheelLeft;
      if (io.MouseWheelH < 0)
        return HTKey_MouseWheelRight;
    }
    return i;
  }
  return HTKey_None;
}

static void showSingleKeyBind(
  ModKeyBind *kb,
  f32 cursor
) {
  ImGuiIO &io = ImGui::GetIO();
  f32 x;

  // Show key display name.
  ImGui::AlignTextToFramePadding();
  ImGui::Text(kb->displayName.c_str());
  ImGui::SameLine();

  // Calculate the max pos X of all the texts, for a better align.
  x = ImGui::GetCursorPosX();
  if (gMenuKeyBindMaxPosX < x)
    gMenuKeyBindMaxPosX = x;

  // Right align, show current key.
  ImGui::SetCursorPosX(cursor);
  if (gActiveKey == kb) {
    // Modify a key.
    io.WantCaptureKeyboard = true;
    ImGui::SetNextItemWidth(HOTKEY_DISPLAY_WIDTH);
    ImGui::InputText(
      "##KeyModify",
      gFakeBuffer,
      sizeof(gFakeBuffer));
    bool hovered = ImGui::IsItemHovered();

    if (hovered)
      // Forcely capture the keyboard inputs if the input area is hovered.
      ImGui::SetKeyboardFocusHere(-1);

    HTKeyCode key = waitForKeyPress();
    if (key == HTKey_MouseLeft || key == HTKey_MouseRight) {
      if (!hovered)
        // If clicked other region, then cancel current key editing.
        goto Cancel;
BindKey:
      (void)HTHotkeyBind((HTHandle)gActiveKey, key);
Cancel:
      gFakeBuffer[0] = 0;
      gActiveKey = nullptr;
    } else if (key != HTKey_None)
      // Capture any key inputs and write the captured key into the ModKeyBind
      // struct.
      goto BindKey;
  } else if (ImGui::Button(getKeyName(kb->key), ImVec2(HOTKEY_DISPLAY_WIDTH, 0)))
    // Trigger key modification.
    gActiveKey = kb;
  
  if (gActiveKey)
    HTHotkeySetCooldown();
  HTHotkeyUpdateCooldown();

  // Show reset button.
  ImGui::SameLine();
  ImGui::BeginDisabled(kb->defaultKey == kb->key);
  if (ImGui::Button("Reset", ImVec2(HOTKEY_RESET_WIDTH, 0)))
    // Reset key bind.
    (void)HTHotkeyBind((HTHandle)kb, kb->defaultKey);
  ImGui::EndDisabled();
}

/**
 * Display key binds menu, and handle key bind modification.
 */
static void displayAndUpdateKeys() {
  float windowPadding = ImGui::GetStyle().WindowPadding.x
    , cursor;

  // Calculate cursor pos for right alignment.
  cursor = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
  cursor -= HOTKEY_BUTTONS_WIDTH + windowPadding;
  if (cursor <= gMenuKeyBindMaxPosX)
    cursor = gMenuKeyBindMaxPosX;

  ImGui::PushTextWrapPos(500.0);
  for (auto modIt = gModDataRuntime.begin(); modIt != gModDataRuntime.end(); modIt++) {
    ModRuntime *rt = &modIt->second;
    if (rt->keyBinds.empty())
      continue;

    // Mod name.
    ImGui::SeparatorText(rt->manifest->modName.c_str());

    // Keys.
    for (auto keyIt = rt->keyBinds.begin(); keyIt != rt->keyBinds.end(); keyIt++) {
      ModKeyBind *kb = &keyIt->second;
      ImGui::PushID((void *)kb);
      showSingleKeyBind(kb, cursor);
      ImGui::PopID();
    }
  }
  ImGui::PopTextWrapPos();
}

/**
 * Render settings menu.
 */
static void HTMenuSettings() {
  if (ImGui::CollapsingHeader("Key Bindings##HTModKeys", ImGuiTreeNodeFlags_None))
    displayAndUpdateKeys();
}

/**
 * Initialize ImGui context and window message hook.
 * 
 * This function only be called after the game window is catched.
 */
void HTInitGUI() {
  if (!gGameStatus.window)
    return;
  if (ImGui::GetCurrentContext())
    // Prevent duplicate initialization.
    return;

  // Initialize ImGui. ImGui_ImplVulkan_Init() is called in layer.cpp by
  // renderGui().
  ImGui::CreateContext();
  ImGui_ImplWin32_Init(gGameStatus.window);
  ImGuiIO &io = ImGui::GetIO();
  ImGuiStyle &style = ImGui::GetStyle();
  io.IniFilename = io.LogFilename = nullptr;

  // Scale the window by dpi.
  f32 dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(gGameStatus.window);
  style.ScaleAllSizes(dpiScale);
  io.FontGlobalScale = dpiScale;

  // Setup colors.
  ImVec4 *colors = ImGui::GetStyle().Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.51f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.82f);
  colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.39f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.47f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.47f, 0.47f, 0.47f, 0.47f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.47f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 0.82f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.63f, 0.63f, 0.59f, 0.75f);

  // Setup sizes.
  style.WindowBorderSize = style.ChildBorderSize = 1;
  style.PopupBorderSize = style.FrameBorderSize = 0;
  style.WindowRounding = style.ChildRounding = style.FrameRounding
    = style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = 10;
  style.ScrollbarSize = 10;
  style.SeparatorTextAlign = ImVec2(0, 0.5);
  style.SeparatorTextPadding = ImVec2(0, 3);
  style.WindowTitleAlign = ImVec2(0.5, 0.5);

  // Install window process hook.
  HTInstallInputHook();
}

/**
 * Shutdown ImGui. Unused.
 */
void HTDeinitGUI() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

void HTUpdateGUI() {
  ImGuiIO &io = ImGui::GetIO();

  // Draw all windows.
  for (auto it = gModDataRuntime.begin(); it != gModDataRuntime.end(); it++) {
    PFN_HTModRenderGui guiRenderer = (PFN_HTModRenderGui)it->second.loaderFunc.pfn_HTModRenderGui;
    if (guiRenderer)
      guiRenderer(io.DeltaTime, nullptr);
  }
}

void HTMainMenu(f32, void *) {
  if (!gShowMainMenu)
    return;

  if (gFirstFrame) {
    // Resize window on the first frame.
    ImGui::SetNextWindowSize(ImVec2(480, 320));
    gFirstFrame = false;
  }

  if (!ImGui::Begin("HTML Main Menu", &gShowMainMenu))
    return (void)ImGui::End();
  
  if (ImGui::BeginTabBar("HTNavMain")) {
    if (ImGui::BeginTabItem("Abouts")) {
      HTMenuAbouts();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Console")) {
      HTMenuConsole();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Mods")) {
      HTMenuModList();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Settings")) {
      HTMenuSettings();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  
  ImGui::End();
}

void HTToggleMenuState(HTKeyEvent *event) {
  if ((event->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    gShowMainMenu = !gShowMainMenu;
}

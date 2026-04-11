#include <stdarg.h>
#include <stdio.h>
#include "imgui.h"

#include "htinternal.hpp"

// Widget widths.
#define HOTKEY_DISPLAY_WIDTH 120.0
#define HOTKEY_RESET_WIDTH 65.0
#define HOTKEY_BUTTONS_WIDTH (HOTKEY_DISPLAY_WIDTH + HOTKEY_RESET_WIDTH)

#define CONSOLE_MAX_LINE 2048

static const ImVec4 modNameColor(1, 1, 1, 1)
  , modDescColor(0.75, 0.75, 0.75, 1);
static char gFakeBuffer[5] = {0}
  , gConsoleInputBuffer[1024] = {0};
static float gMenuKeyBindMaxPosX = 0;
static ModKeyBind *gActiveKey = nullptr;
static ImVector<char *> gLines;

// ----------------------------------------------------------------------------
// [SECTION] Key binding implementations.
// ----------------------------------------------------------------------------

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

static i32 keyBindWidget(HTKeyCode *key) {
  ImGuiIO &io = ImGui::GetIO();

  gFakeBuffer[0] = 0;

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

  HTKeyCode keyPressed = waitForKeyPress();
  if (keyPressed == HTKey_None)
    // There's no key's pressed, do nothing.
    return 0;
  else if ((keyPressed == HTKey_MouseLeft || keyPressed == HTKey_MouseRight) && !hovered)
    // If clicked other region, then cancel current key editing.
    return -1;
  else if (keyPressed == HTKey_Escape)
    // Clear key binding, which means setting it to HTKey_None.
    *key = HTKey_None;
  else
    // Capture any key inputs and write the captured key into the ModKeyBind
    // struct.
    *key = keyPressed;

  return 1;
}

static void showSingleKeyBind(
  ModKeyBind *kb,
  f32 cursor
) {
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
    // If 
    HTKeyCode key;
    i32 t = keyBindWidget(&key);
    if (t) {
      gActiveKey = nullptr;
      if (t == 1)
        (void)HTHotkeyBind(kb, key);
    }
  } else {
    if (ImGui::Button(HTHotkeyGetName(kb->key), ImVec2(HOTKEY_DISPLAY_WIDTH, 0)))
      // Trigger key modification.
      gActiveKey = kb;
  }

  ImGui::SameLine();
  // Show reset button.
  ImGui::BeginDisabled(kb->defaultKey == kb->key);
  if (ImGui::Button("Reset", ImVec2(HOTKEY_RESET_WIDTH, 0)))
    // Reset key bind.
    (void)HTHotkeyBind((HTHandle)kb, kb->defaultKey);
  ImGui::EndDisabled();

  if (gActiveKey)
    HTiHotkeySetCooldown();
  HTiHotkeyUpdateCooldown();
}

/**
 * Display key binds menu, and handle key bind modification.
 */
static void displayAndUpdateKeys() {
  f32 windowPadding = ImGui::GetStyle().WindowPadding.x
    , cursor;

  // Calculate cursor pos for right alignment.
  cursor = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
  cursor -= HOTKEY_BUTTONS_WIDTH + windowPadding;
  if (cursor <= gMenuKeyBindMaxPosX)
    cursor = gMenuKeyBindMaxPosX;

  ImGui::PushTextWrapPos(500.0);
  for (auto modIt = gModDataRuntime.begin(); modIt != gModDataRuntime.end(); modIt++) {
    ModRuntime *rt = &modIt->second;
    if (rt->keyBinds.empty() || !rt->hasRegisteredKeys)
      continue;

    // Mod name.
    ImGui::SeparatorText(rt->manifest->modName.c_str());

    // Keys.
    for (auto keyIt = rt->keyBinds.begin(); keyIt != rt->keyBinds.end(); keyIt++) {
      ModKeyBind *kb = &keyIt->second;
      if (!kb->isRegistered)
        continue;
      ImGui::PushID((void *)kb);
      showSingleKeyBind(kb, cursor);
      ImGui::PopID();
    }
  }
  ImGui::PopTextWrapPos();
}

// ----------------------------------------------------------------------------
// [SECTION] Console implementations.
// ----------------------------------------------------------------------------

static int inputCallback(ImGuiInputTextCallbackData *data) {
  return 0;
}

void HTiMenuConsole() {
  ImGuiListClipper clipper;
  f32 height;
  bool reclaimFocus = false;

  height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  if (ImGui::BeginChild(
    "ConsoleLog",
    ImVec2(0, -height),
    ImGuiChildFlags_NavFlattened,
    ImGuiWindowFlags_HorizontalScrollbar
  )) {
    // Show texts.
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::Selectable("Clear console"))
        HTiClearConsole();
      ImGui::EndPopup();
    }

    HTiRenderConsoleTexts();
  }
  ImGui::EndChild();
  ImGui::Separator();

  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::InputText(
    "##ConsoleInput",
    gConsoleInputBuffer,
    IM_ARRAYSIZE(gConsoleInputBuffer),
    ImGuiInputTextFlags_EnterReturnsTrue
      | ImGuiInputTextFlags_EscapeClearsAll
      | ImGuiInputTextFlags_CallbackCompletion
      | ImGuiInputTextFlags_CallbackHistory,
    inputCallback,
    nullptr
  )) {
    // Pressed enter.
    reclaimFocus = true;
    HTiAddConsoleLine(false, "%s", gConsoleInputBuffer);
    gConsoleInputBuffer[0] = 0;
  }

  ImGui::SetItemDefaultFocus();
  if (reclaimFocus)
    ImGui::SetKeyboardFocusHere(-1);
}

// ----------------------------------------------------------------------------
// [SECTION] Settings implementations.
// ----------------------------------------------------------------------------

static void displayLoaderSettings() {
#ifndef HTML_ENABLE_DEBUGGER
  ImGui::BeginDisabled();
  ImGui::Checkbox("Show debugger", &gShowDebugger);
  ImGui::EndDisabled();
#else
  ImGui::Checkbox("Show debugger", &gShowDebugger);
#endif
}

/**
 * Render settings menu.
 */
void HTiMenuSettings() {
  if (ImGui::CollapsingHeader("Loader Settings"))
    displayLoaderSettings();
  if (ImGui::CollapsingHeader("Key Bindings"))
    displayAndUpdateKeys();
}

// ----------------------------------------------------------------------------
// [SECTION] Other submenu implementations.
// ----------------------------------------------------------------------------

/**
 * Render mod list tab item.
 */
void HTiMenuAbouts() {
  ImGui::Text("HT's Mod Loader v" HTML_VERSION_NAME " by HTMonkeyG");
  ImGui::Text("A general mod loader initially developed for Sky:CotL.");
  ImGui::TextLinkOpenURL(
    "<https://www.github.com/HTMonkeyG/HTML-Sky>",
    "https://www.github.com/HTMonkeyG/HTML-Sky");
}

/**
 * Render mod list tab item.
 */
void HTiMenuModList() {
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

    // Don't display mods that isn't expanded successfully.
    if (!manifest.runtime)
      continue;

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

#include <stdarg.h>
#include <stdio.h>
#include "imgui.h"

#include "aliases.h"
#include "ui/console.h"

static ImVector<char *> gLines;
static char gInputBuffer[1024] = {0};

static int inputCallback(ImGuiInputTextCallbackData *data) {
  return 0;
}

static void HTAddConsoleLine(const char* fmt, ...) {
  char buf[1024];
  va_list args;
  size_t len;
  void *mem;

  va_start(args, fmt);
  vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
  buf[IM_ARRAYSIZE(buf) - 1] = 0;
  va_end(args);

  len = strlen(buf) + 1;
  mem = ImGui::MemAlloc(len);
  gLines.push_back((char *)memcpy(mem, (const void*)buf, len));
}

static void HTClearConsole() {
  for (int i = 0; i < gLines.Size; i++)
    ImGui::MemFree(gLines[i]);
  gLines.clear();
}

void HTMenuConsole() {
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
        HTClearConsole();
      ImGui::EndPopup();
    }

    clipper.Begin(gLines.Size);
    while (clipper.Step())
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        ImGui::TextUnformatted(gLines[i]);
      }
  }
  ImGui::EndChild();
  ImGui::Separator();

  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::InputText(
    "##ConsoleInput",
    gInputBuffer,
    IM_ARRAYSIZE(gInputBuffer),
    ImGuiInputTextFlags_EnterReturnsTrue
      | ImGuiInputTextFlags_EscapeClearsAll
      | ImGuiInputTextFlags_CallbackCompletion
      | ImGuiInputTextFlags_CallbackHistory,
    inputCallback,
    nullptr
  )) {
    // Pressed enter.
    reclaimFocus = true;
    HTAddConsoleLine("%s", gInputBuffer);
    gInputBuffer[0] = 0;
  }

  ImGui::SetItemDefaultFocus();
  if (reclaimFocus)
    ImGui::SetKeyboardFocusHere(-1);
}

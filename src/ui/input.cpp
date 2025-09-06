#include <windows.h>
#include "imgui.h"

#include "input.h"
#include "globals.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
  HWND hWnd,
  UINT msg,
  WPARAM wParam,
  LPARAM lParam);

// Original window process of the game.
static WNDPROC gWndProcOrigin = nullptr;

/**
 * The window process used to pass window messages to ImGui and block window
 * message delivery to the game.
 */
static LRESULT APIENTRY HTWndProc(
  HWND hWnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam
) {
  UINT block = 0;
  ImGuiIO &io = ImGui::GetIO();

  // Dispatch the window message to ImGui.
  ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

  // Block the message.
  switch (uMsg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
      block = io.WantCaptureMouse;
      break;
    case WM_KEYDOWN:
    case WM_CHAR:
      block = io.WantCaptureKeyboard;
      break;
  }
  if (block)
    return 1;

  // Pass the window message to the game.
  return CallWindowProcW(
    gWndProcOrigin, hWnd, uMsg, wParam, lParam);
}

/**
 * Hook the window process of the game.
 */
void HTInstallInputHook() {
  if (!gGameStatus.window)
    return;
  gWndProcOrigin = (WNDPROC)SetWindowLongPtrW(
    gGameStatus.window,
    GWLP_WNDPROC,
    (LONG_PTR)HTWndProc);
}

/**
 * Release the window callback hook of the game.
 */
void HTUninstallInputHook() {
  if (gGameStatus.window && gWndProcOrigin)
    (void)SetWindowLongPtrW(
      gGameStatus.window,
      GWLP_WNDPROC,
      (LONG_PTR)gWndProcOrigin);
}

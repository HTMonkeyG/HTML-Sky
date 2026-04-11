#include <windows.h>
#include "imgui.h"

#include "htinternal.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
  HWND hWnd,
  UINT msg,
  WPARAM wParam,
  LPARAM lParam);

// Original window process of the game.
static WNDPROC gWndProcOrigin = nullptr;

static bool isVkDown(int vk) {
  return (GetKeyState(vk) & 0x8000) != 0;
}

/**
 * Modified from ImGui. Map VK_* to HTKeyCode_*.
 */
static HTKeyCode vkToInternalKey(
  WPARAM wParam,
  LPARAM lParam
) {
  if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
    return HTKey_KeypadEnter;

  switch (wParam) {
    case VK_TAB: return HTKey_Tab;
    case VK_LEFT: return HTKey_LeftArrow;
    case VK_RIGHT: return HTKey_RightArrow;
    case VK_UP: return HTKey_UpArrow;
    case VK_DOWN: return HTKey_DownArrow;
    case VK_PRIOR: return HTKey_PageUp;
    case VK_NEXT: return HTKey_PageDown;
    case VK_HOME: return HTKey_Home;
    case VK_END: return HTKey_End;
    case VK_INSERT: return HTKey_Insert;
    case VK_DELETE: return HTKey_Delete;
    case VK_BACK: return HTKey_Backspace;
    case VK_SPACE: return HTKey_Space;
    case VK_RETURN: return HTKey_Enter;
    case VK_ESCAPE: return HTKey_Escape;
    case VK_OEM_COMMA: return HTKey_Comma;
    case VK_OEM_PERIOD: return HTKey_Period;
    case VK_CAPITAL: return HTKey_CapsLock;
    case VK_SCROLL: return HTKey_ScrollLock;
    case VK_NUMLOCK: return HTKey_NumLock;
    case VK_SNAPSHOT: return HTKey_PrintScreen;
    case VK_PAUSE: return HTKey_Pause;
    case VK_NUMPAD0: return HTKey_Keypad0;
    case VK_NUMPAD1: return HTKey_Keypad1;
    case VK_NUMPAD2: return HTKey_Keypad2;
    case VK_NUMPAD3: return HTKey_Keypad3;
    case VK_NUMPAD4: return HTKey_Keypad4;
    case VK_NUMPAD5: return HTKey_Keypad5;
    case VK_NUMPAD6: return HTKey_Keypad6;
    case VK_NUMPAD7: return HTKey_Keypad7;
    case VK_NUMPAD8: return HTKey_Keypad8;
    case VK_NUMPAD9: return HTKey_Keypad9;
    case VK_DECIMAL: return HTKey_KeypadDecimal;
    case VK_DIVIDE: return HTKey_KeypadDivide;
    case VK_MULTIPLY: return HTKey_KeypadMultiply;
    case VK_SUBTRACT: return HTKey_KeypadSubtract;
    case VK_ADD: return HTKey_KeypadAdd;
    case VK_LSHIFT: return HTKey_LeftShift;
    case VK_LCONTROL: return HTKey_LeftCtrl;
    case VK_LMENU: return HTKey_LeftAlt;
    case VK_LWIN: return HTKey_LeftSuper;
    case VK_RSHIFT: return HTKey_RightShift;
    case VK_RCONTROL: return HTKey_RightCtrl;
    case VK_RMENU: return HTKey_RightAlt;
    case VK_RWIN: return HTKey_RightSuper;
    case VK_APPS: return HTKey_Menu;
    case '0': return HTKey_0;
    case '1': return HTKey_1;
    case '2': return HTKey_2;
    case '3': return HTKey_3;
    case '4': return HTKey_4;
    case '5': return HTKey_5;
    case '6': return HTKey_6;
    case '7': return HTKey_7;
    case '8': return HTKey_8;
    case '9': return HTKey_9;
    case 'A': return HTKey_A;
    case 'B': return HTKey_B;
    case 'C': return HTKey_C;
    case 'D': return HTKey_D;
    case 'E': return HTKey_E;
    case 'F': return HTKey_F;
    case 'G': return HTKey_G;
    case 'H': return HTKey_H;
    case 'I': return HTKey_I;
    case 'J': return HTKey_J;
    case 'K': return HTKey_K;
    case 'L': return HTKey_L;
    case 'M': return HTKey_M;
    case 'N': return HTKey_N;
    case 'O': return HTKey_O;
    case 'P': return HTKey_P;
    case 'Q': return HTKey_Q;
    case 'R': return HTKey_R;
    case 'S': return HTKey_S;
    case 'T': return HTKey_T;
    case 'U': return HTKey_U;
    case 'V': return HTKey_V;
    case 'W': return HTKey_W;
    case 'X': return HTKey_X;
    case 'Y': return HTKey_Y;
    case 'Z': return HTKey_Z;
    case VK_F1: return HTKey_F1;
    case VK_F2: return HTKey_F2;
    case VK_F3: return HTKey_F3;
    case VK_F4: return HTKey_F4;
    case VK_F5: return HTKey_F5;
    case VK_F6: return HTKey_F6;
    case VK_F7: return HTKey_F7;
    case VK_F8: return HTKey_F8;
    case VK_F9: return HTKey_F9;
    case VK_F10: return HTKey_F10;
    case VK_F11: return HTKey_F11;
    case VK_F12: return HTKey_F12;
    case VK_F13: return HTKey_F13;
    case VK_F14: return HTKey_F14;
    case VK_F15: return HTKey_F15;
    case VK_F16: return HTKey_F16;
    case VK_F17: return HTKey_F17;
    case VK_F18: return HTKey_F18;
    case VK_F19: return HTKey_F19;
    case VK_F20: return HTKey_F20;
    case VK_F21: return HTKey_F21;
    case VK_F22: return HTKey_F22;
    case VK_F23: return HTKey_F23;
    case VK_F24: return HTKey_F24;
    case VK_BROWSER_BACK: return HTKey_AppBack;
    case VK_BROWSER_FORWARD: return HTKey_AppForward;
    default: break;
  }

  i32 scanCode = (i32)LOBYTE(HIWORD(lParam));
  switch (scanCode) {
    case 41: return HTKey_GraveAccent;
    case 12: return HTKey_Minus;
    case 13: return HTKey_Equal;
    case 26: return HTKey_LeftBracket;
    case 27: return HTKey_RightBracket;
    case 86: return HTKey_Oem102;
    case 43: return HTKey_Backslash;
    case 39: return HTKey_Semicolon;
    case 40: return HTKey_Apostrophe;
    case 51: return HTKey_Comma;
    case 52: return HTKey_Period;
    case 53: return HTKey_Slash;
  }

  return HTKey_None;
}

#define HTHotkeyCheck(a, b) \
  ((void)((isVkDown(a) == isKeyDown) && (HTiHotkeyDispatch(b, down | repeat | blockedKey, blocked), 1)))

/**
 * Modified from ImGui. Dispatch key events to registered callbacks.
 */
static void HTHotKeyWndProc(
  HWND hWnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam,
  u08 *blocked
) {
  HTKeyEventFlags blockedKey = *blocked
    ? HTKeyEventFlags_Blocked
    : HTKeyEventFlags_None;

  switch (uMsg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
      bool isKeyDown = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
      if (wParam >= 256)
        return;

      HTKeyCode key = vkToInternalKey(wParam, lParam);
      i32 vk = (i32)wParam;
      HTKeyEventFlags repeat = ((lParam & 0x40000000) && isKeyDown)
        ? HTKeyEventFlags_Repeat
        : HTKeyEventFlags_None;
      HTKeyEventFlags down = isKeyDown
        ? HTKeyEventFlags_Down
        : HTKeyEventFlags_Up;

      if (key == HTKey_PrintScreen && !isKeyDown)
        HTiHotkeyDispatch(key, HTKeyEventFlags_Down | repeat | blockedKey, blocked);
      else if (vk == VK_SHIFT) {
        HTHotkeyCheck(VK_LSHIFT, HTKey_LeftShift);
        HTHotkeyCheck(VK_LSHIFT, HTKey_LeftShift);
      } else if (vk == VK_CONTROL) {
        HTHotkeyCheck(VK_LCONTROL, HTKey_LeftCtrl);
        HTHotkeyCheck(VK_RCONTROL, HTKey_RightCtrl);
      } else if (vk == VK_MENU) {
        HTHotkeyCheck(VK_LMENU, HTKey_LeftAlt);
        HTHotkeyCheck(VK_RMENU, HTKey_RightAlt);
      } else if (key != HTKey_None)
        HTiHotkeyDispatch(key, down | repeat | blockedKey, blocked);
      break;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK: {
      HTKeyCode button = HTKey_None;
      if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK)
        button = HTKey_MouseLeft;
      if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK)
        button = HTKey_MouseRight;
      if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK)
        button = HTKey_MouseMiddle;
      if (uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONDBLCLK)
        button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
          ? HTKey_MouseX1
          : HTKey_MouseX2;
      HTiHotkeyDispatch(button, HTKeyEventFlags_Down | blockedKey, blocked);
      break;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP: {
      HTKeyCode button = HTKey_None;
      if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK)
        button = HTKey_MouseLeft;
      if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK)
        button = HTKey_MouseRight;
      if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK)
        button = HTKey_MouseMiddle;
      if (uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONDBLCLK)
        button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
          ? HTKey_MouseX1
          : HTKey_MouseX2;
      HTiHotkeyDispatch(button, HTKeyEventFlags_Up | blockedKey, blocked);
      break;
    }
    case WM_MOUSEWHEEL: {
      f32 mouseDelta = (f32)GET_WHEEL_DELTA_WPARAM(wParam) / (f32)WHEEL_DELTA;
      if (mouseDelta > 0)
        HTiHotkeyDispatch(HTKey_MouseWheelUp, HTKeyEventFlags_Down | blockedKey, blocked);
      else
        HTiHotkeyDispatch(HTKey_MouseWheelDown, HTKeyEventFlags_Down | blockedKey, blocked);
      break;
    }
    case WM_MOUSEHWHEEL: {
      f32 mouseDelta = (f32)GET_WHEEL_DELTA_WPARAM(wParam) / (f32)WHEEL_DELTA;
      if (mouseDelta > 0)
        HTiHotkeyDispatch(HTKey_MouseWheelRight, HTKeyEventFlags_Down | blockedKey, blocked);
      else
        HTiHotkeyDispatch(HTKey_MouseWheelLeft, HTKeyEventFlags_Down | blockedKey, blocked);
      break;
    }
  }
}

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
  u08 block = 0;
  ImGuiIO &io = ImGui::GetIO();

  // Dispatch the window message to ImGui.
  (void)ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

  // Block the message. We only check for the key down message, in order to
  // avoid strange behaviors e.g. continuely moving characters.
  switch (uMsg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
      block = io.WantCaptureMouse;
      break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    case WM_CHAR:
      block = io.WantCaptureKeyboard;
      break;
  }
  HTHotKeyWndProc(hWnd, uMsg, wParam, lParam, &block);
  if (block)
    return 0;

  // Pass the window message to the game.
  return CallWindowProcW(
    gWndProcOrigin, hWnd, uMsg, wParam, lParam);
}

ImGuiKey HTKeyToImGuiKey(HTKeyCode key) {
  if (key >= HTKey_NamedKey_BEGIN && key <= HTKey_Oem102)
    return (ImGuiKey)key;
  if (key >= HTKey_Mouse_BEGIN && key <= HTKey_MouseX2)
    return (ImGuiKey)(key - HTKey_Mouse_BEGIN + ImGuiKey_MouseLeft);
  if (key == HTKey_MouseWheelUp || key == HTKey_MouseWheelDown)
    return ImGuiKey_MouseWheelY;
  if (key == HTKey_MouseWheelLeft || key == HTKey_MouseWheelRight)
    return ImGuiKey_MouseWheelX;
  return ImGuiKey_None;
}

/**
 * Hook the window process of the game.
 */
void HTiInstallInputHook() {
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
void HTiUninstallInputHook() {
  if (gGameStatus.window && gWndProcOrigin)
    (void)SetWindowLongPtrW(
      gGameStatus.window,
      GWLP_WNDPROC,
      (LONG_PTR)gWndProcOrigin);
}

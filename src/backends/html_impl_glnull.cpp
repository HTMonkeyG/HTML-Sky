// ----------------------------------------------------------------------------
// Null graphic library backend implementation for HTML.
// 
// This backend neither render anything, nor call HTML functions.
// You may use this backend as a template.
// ----------------------------------------------------------------------------

#include "imgui.h"
#include "imgui_impl_win32.h"

#include "htinternal.hpp"
#include "includes/htconfig.h"

//#ifdef USE_IMPL_NULLGL
#if 0

static bool gInit = false;

static void fakeRenderGui() {
  if (!gInit) {
    // Initialize the ImGui context if not. The backend can set a local flag
    // to execute HTiInitGUI() once.
    gInit = true;
    HTiInitGUI();

    // Backends can put its own initialization codes here, like changing the
    // styles and color.
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
  }

  // Enter the critical zone and initialize the backend data of ImGui.
  if (HTiBackendGLEnterCritical()) {
    // Initialize ImGui's backend.
    //ImGui_ImplOpenGL3_Init();

    // Set the name of the graphics backend.
    HTiSetGLBackendName("Impl_GLNull");

    // Set the GUI inited event.
    HTiBackendGLInitComplete();
  }
  // Call HTiBackendGLLeaveCritical() after every HTiBackendGLEnterCritical().
  HTiBackendGLLeaveCritical();

  // Create new frame.
  //ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  // Render ImGui.
  HTiUpdateGUI();

  ImGui::Render();

  //ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  (void)ImGui::GetDrawData();
}

static DWORD WINAPI fakeRenderLoop(
  LPVOID lpParam
) {
  MSG msg;

  SetTimer(nullptr, 0, 1000 / 10, nullptr);

  while (GetMessageA(&msg, nullptr, 0, 0)) {
    if (msg.message == WM_QUIT)
      break;
    if (msg.message != WM_TIMER)
      continue;

    // Simulate the game's render loop. In real backends, this function should
    // be injected into the render loop, after the game's present.
    fakeRenderGui();
  }
}

int HTi_ImplGLNull_Init() {
  // Other initialize codes: hooks, patches and more...

  // This thread is for demonstration purposes only.
  CreateThread(
    nullptr,
    0,
    fakeRenderLoop,
    nullptr,
    0,
    nullptr);

  return 1;
}

const HTiBackendRegister g_register_ImplGLNull{
  "GLNull",
  HTi_ImplGLNull_Init
};

#endif

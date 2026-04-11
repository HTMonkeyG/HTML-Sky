// ----------------------------------------------------------------------------
// OpenGL3 implementation of HT's Mod Loader.
// ----------------------------------------------------------------------------

#include <windows.h>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

#include "includes/backends/html_impl_opengl3.h"
#include "includes/htconfig.h"
#include "htinternal.hpp"

#ifdef HTML_USE_IMPL_OPENGL3

#define HT_IMGL3W_IMPL
#include "html_impl_opengl3_loader.h"

typedef WINBOOL (WINAPI *PFN_wglSwapBuffers)(
  HDC);
typedef WINBOOL (WINAPI *PFN_wglSwapLayerBuffers)(
  HDC, UINT);
typedef HDC (WINAPI *PFN_wglGetCurrentDC)();

static PFN_wglGetCurrentDC fn_wglGetCurrentDC = nullptr;
static PFN_wglSwapBuffers fn_wglSwapBuffers = nullptr;
static PFN_wglSwapLayerBuffers fn_wglSwapLayerBuffers = nullptr;
static bool gInit = false;
static HMODULE hDllOpengl32 = nullptr;

/*
static struct {

} gSavedStatus;

static void setupRenderState() {

}

static void restoreRenderState() {

}
*/

static void doRender(
  HDC hDC
) {
  (void)hDC;

  if (!gInit) {
    gInit = true;
    HTiInitGUI();
  }

  if (HTiBackendGLEnterCritical()) {
    // Initialize ImGui.
    ImGui_ImplOpenGL3_Init();
    HTiSetGLBackendName(HT_ImplOpenGL3_Name);

    // Set the gui inited event.
    HTiBackendGLInitComplete();
  }
  HTiBackendGLLeaveCritical();

  if (glGetIntegerv == nullptr) {
    ht_imgl3wInit();
    HMODULE hOpengl32 = LoadLibraryA("opengl32.dll");
    if (hOpengl32)
      fn_wglGetCurrentDC = (PFN_wglGetCurrentDC)GetProcAddress(hOpengl32, "wglGetCurrentDC");
    FreeLibrary(hOpengl32);
  }

  // Create new frame.
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  // Render ImGui.
  HTiUpdateGUI();

  ImGui::Render();

  HDC hGameDC = GetDC(gGameStatus.window);
  if (
    hGameDC != hDC
    || (fn_wglGetCurrentDC && fn_wglGetCurrentDC() != hGameDC)
  )
    return (void)ReleaseDC(gGameStatus.window, hGameDC);;

  // Since we hooked wglSwapBuffers, the render pass can be considered as ended,
  // so we don't save and restore GL states.
  // The following GL calls start a new render pass.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDepthRangef(0.0f, 1.0f);
  glClearDepthf(1.0f);
  glClearStencil(0);
  glDepthMask(GL_TRUE);
  glStencilMask(0xFF);
  glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xFF); 
  glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFF); 
  glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP); 
  glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static WINBOOL WINAPI hook_wglSwapLayerBuffers(
  HDC hDC,
  UINT uPlanes
) {
  doRender(hDC);
  return fn_wglSwapLayerBuffers(hDC, uPlanes);
}

static WINBOOL WINAPI hook_wglSwapBuffers(
  HDC hDC
) {
  doRender(hDC);
  return fn_wglSwapBuffers(hDC);
}

/**
 * Setup the vulkan layer injection.
 */
int HTi_ImplOpenGL3_Init() {
  MH_STATUS s;
  void *function = nullptr;

  // To make the gl3w_stripped happy.
  (void)GL_VERSION;
  (void)GL_MAJOR_VERSION;
  (void)GL_MINOR_VERSION;
  (void)glGetString;

  LOG("[ImplOpenGL3][INFO] HTi_ImplOpenGL3_Init() called.\n");

  // We don't want to create a new thread and wait, so we load gdi32.dll
  // directly.
  // To keep the hook effective, we don't free the dll.
  hDllOpengl32 = LoadLibraryA("opengl32.dll");
  if (!hDllOpengl32) {
    LOG("[ImplOpenGL3][ERR] Failed to load 'opengl32.dll'.\n");
    return 0;
  }

  s = MH_CreateHookApiEx(
    L"opengl32.dll",
    "wglSwapBuffers",
    (void *)hook_wglSwapBuffers,
    (void **)&fn_wglSwapBuffers,
    &function
  );
  if (s != MH_OK)
    return 0;

  if (MH_EnableHook(function) != MH_OK)
    return 0;

  LOG("[ImplOpenGL3][INFO] Hooked wglSwapBuffers(): 0x%p.\n", function);

  s = MH_CreateHookApiEx(
    L"opengl32.dll",
    "wglSwapLayerBuffers",
    (void *)hook_wglSwapLayerBuffers,
    (void **)&fn_wglSwapLayerBuffers,
    &function
  );
  if (s != MH_OK)
    return 0;

  if (MH_EnableHook(function) != MH_OK)
    return 0;

  LOG("[ImplOpenGL3][INFO] Hooked wglSwapLayerBuffers(): 0x%p.\n", function);

  return 1;
}

int HTi_ImplOpenGL3_Shutdown() {
  // Free opengl32.dll
  FreeLibrary(hDllOpengl32);
  return 1;
}

const HTiBackendRegister g_register_ImplOpenGL3{
  HT_ImplOpenGL3_Name,
  HTi_ImplOpenGL3_Init
};

#endif

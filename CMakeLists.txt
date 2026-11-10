# ---------------------------------------------------------------------------
# HTModLoader (HTML-Sky) build script.
#
# Replaces the old Makefile: compiles the bundled third-party libraries from
# source, builds the proxy DLL (winhttp.dll) and produces the import library
# used by mods (htmodloader.lib).
#
# The project is only supported with the MinGW toolchain, matching the
# previous Makefile.
#
# Usage:
#   cmake -S . -B build -G Ninja
#   cmake --build build
#
# Use the Ninja generator when the project path contains non-ASCII characters
# (CMake's "MinGW Makefiles" generator cannot handle such paths).
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20)

project(HTModLoader VERSION 1.11.0 LANGUAGES C CXX)

if (NOT MINGW)
  message(FATAL_ERROR "This project only supports compilation using the MinGW toolchain.")
endif()

# CMake's Makefile generators drive the build with mingw32-make, which converts
# command lines through the ANSI code page. Non-ASCII paths (e.g. a project
# folder with Chinese characters) are therefore corrupted before they reach
# CMake or the compiler. Ninja uses wide-character APIs and is unaffected.
if (CMAKE_GENERATOR MATCHES "Makefiles")
  if ("${CMAKE_SOURCE_DIR}" MATCHES "[^ -~]" OR "${CMAKE_BINARY_DIR}" MATCHES "[^ -~]")
    message(WARNING
      "The \"${CMAKE_GENERATOR}\" generator cannot handle non-ASCII paths: "
      "mingw32-make corrupts them through the ANSI code page. Use the Ninja "
      "generator (-G Ninja) or build from an ASCII-only path (e.g. via a "
      "directory junction).")
  endif()
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# The old Makefile always built with -O3 -DNDEBUG, so default to Release.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Options.
# ---------------------------------------------------------------------------

option(HTMODLOADER_ENABLE_LTO "Enable link-time optimization in Release builds." ON)

set(HTMODLOADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    CACHE PATH "Directory where winhttp.dll and htmodloader.lib are written.")

# ---------------------------------------------------------------------------
# Paths.
# ---------------------------------------------------------------------------

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBRARIES_DIR "${PROJECT_ROOT}/libraries")

set(IMGUI_DIR "${LIBRARIES_DIR}/imgui")
set(FREETYPE_DIR "${LIBRARIES_DIR}/freetype")
set(MINHOOK_DIR "${LIBRARIES_DIR}/minhook")
set(CJSON_DIR "${LIBRARIES_DIR}/cJSON")
set(LEVELDB_DIR "${LIBRARIES_DIR}/leveldb")
set(VULKAN_DIR "${LIBRARIES_DIR}/vulkan")

# Common warning flags (same set as the previous Makefile).
set(HTMODLOADER_WARNING_FLAGS
  -Wall
  -Wformat
  -Wno-unused-function
  -Wno-stringop-overread
)

# Applies the flags shared by every target in this project.
function(htmodloader_apply_common_flags target)
  target_compile_options(${target} PRIVATE ${HTMODLOADER_WARNING_FLAGS})
endfunction()

# ---------------------------------------------------------------------------
# Third-party libraries built from source.
# ---------------------------------------------------------------------------

# FreeType is a git submodule with its own CMake build system. ImGui uses it
# as its font rasterizer when IMGUI_ENABLE_FREETYPE is defined.
# Optional backends are disabled so the build stays self-contained; zlib comes
# from FreeType's bundled copy.
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HVF TRUE CACHE BOOL "" FORCE)
add_subdirectory("${FREETYPE_DIR}" "${CMAKE_BINARY_DIR}/freetype" EXCLUDE_FROM_ALL)

# Dear ImGui: core + Win32 / Vulkan / OpenGL3 backends.
add_library(imgui STATIC
  "${IMGUI_DIR}/imgui.cpp"
  "${IMGUI_DIR}/imgui_draw.cpp"
  "${IMGUI_DIR}/imgui_tables.cpp"
  "${IMGUI_DIR}/imgui_widgets.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
  "${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp"
)
target_include_directories(imgui PUBLIC
  "${IMGUI_DIR}"
  "${IMGUI_DIR}/backends"
)
target_include_directories(imgui SYSTEM PRIVATE
  "${VULKAN_DIR}/Include"
)
# The vendored ImGui used to define these in imconfig.h. Keep the same
# behaviour with the upstream submodule:
#  - IMGUI_API=dllexport keeps ImGui symbols in winhttp.dll's export table so
#    mods can link against them through htmodloader.lib.
#  - IME support is off by default for non-MSVC compilers; the loader needs it.
target_compile_options(imgui PUBLIC "-DIMGUI_API=__declspec(dllexport)")
target_compile_definitions(imgui PUBLIC
  IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
  IMGUI_ENABLE_FREETYPE
)
target_link_libraries(imgui PRIVATE freetype)
htmodloader_apply_common_flags(imgui)

# MinHook is a git submodule that ships its own MinGW Makefile
# (build/MinGW/Makefile). Drive that Makefile instead of compiling its
# sources directly, so the submodule stays self-contained.
find_program(HTMODLOADER_MINGW_MAKE NAMES mingw32-make mingw32-make.exe REQUIRED)

# The Makefile expects to run from the MinHook root (it uses src/*.c and
# -Isrc/-Iinclude). Refer to it through a relative path: mingw32-make turns
# command-line arguments through the ANSI code page, which would corrupt the
# non-ASCII parts of the project path.
file(RELATIVE_PATH MINHOOK_DIR_REL "${CMAKE_BINARY_DIR}" "${MINHOOK_DIR}")

add_custom_target(minhook
  COMMAND "${HTMODLOADER_MINGW_MAKE}" -C "${MINHOOK_DIR_REL}"
          -f build/MinGW/Makefile libMinHook.a
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Building MinHook..."
  VERBATIM
)

# LevelDB is a git submodule with its own CMake build system. Build it through
# its CMakeLists.txt, with tests / benchmarks / install rules disabled.
set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LEVELDB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LEVELDB_DIR}" "${CMAKE_BINARY_DIR}/leveldb" EXCLUDE_FROM_ALL)

# cJSON.
add_library(cjson STATIC "${CJSON_DIR}/cJSON.c")
target_include_directories(cjson PUBLIC "${CJSON_DIR}")
htmodloader_apply_common_flags(cjson)

# ---------------------------------------------------------------------------
# External libraries.
#
# MinGW's linker cannot open absolute paths that contain non-ASCII characters
# (common on Windows when the project lives in, e.g., a Chinese-named folder).
# Linker inputs are therefore referenced through paths relative to the build
# directory, which the OS resolves through the working directory handle.
# ---------------------------------------------------------------------------

file(RELATIVE_PATH VULKAN_LIB_DIR_REL "${CMAKE_BINARY_DIR}" "${VULKAN_DIR}/Lib")
file(RELATIVE_PATH EXPORTS_TXT_REL "${CMAKE_BINARY_DIR}" "${PROJECT_ROOT}/src/exports.txt")

# These are plain linker names, resolved through the -L flags below.
# (MinHook's Makefile produces libMinHook.a.)
set(HTMODLOADER_EXTERNAL_LIBS vulkan-1 MinHook)

# ---------------------------------------------------------------------------
# Main target: the proxy DLL (winhttp.dll).
# ---------------------------------------------------------------------------

# Every C/C++ file under src/ is part of the loader, including the generated
# WinHTTP proxy (src/proxy/winhttp-proxy.c). Backup files (*.bak) are ignored
# because only *.c/*.cpp are matched.
file(GLOB_RECURSE HTMODLOADER_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_ROOT}/src/*.c"
  "${PROJECT_ROOT}/src/*.cpp"
)

add_library(winhttp SHARED ${HTMODLOADER_SOURCES})

target_include_directories(winhttp PRIVATE
  "${PROJECT_ROOT}/src"
  "${PROJECT_ROOT}/include"
  "${MINHOOK_DIR}/include"
  "${LEVELDB_DIR}/include"
  "${VULKAN_DIR}/Include"
)

# The API functions are marked with HTMLAPIATTR; on Windows they must be
# exported with __declspec(dllexport). Passed as a raw compiler option because
# CMake's target_compile_definitions() drops function-style definitions.
target_compile_options(winhttp PRIVATE
  "-DHTMLAPIATTR=__declspec(dllexport)"
  -ffunction-sections
  -fdata-sections
)

# The MinGW optimizer falsely reports this warning, so it is simply disabled.
add_compile_options(-Wno-stringop-overread)

target_link_libraries(winhttp PRIVATE
  imgui
  cjson
  leveldb
  ${HTMODLOADER_EXTERNAL_LIBS}
  gdi32
  dwmapi
  d3dcompiler
  imm32
)

# Make sure MinHook's Makefile has produced libMinHook.a before linking.
add_dependencies(winhttp minhook)

target_link_options(winhttp PRIVATE
  # Statically link the C/C++ runtimes, like the previous Makefile.
  -static
  -Wl,--gc-sections
  -Wl,-O3
  # Search paths for the external libraries (relative, see note above).
  "-L${VULKAN_LIB_DIR_REL}"
  "-L${MINHOOK_DIR_REL}"
  # Hide everything that is not explicitly exported (__declspec(dllexport)).
  "-Wl,--version-script=${EXPORTS_TXT_REL}"
)

# Link into a build-tree directory using a *relative* path. CMake collapses
# paths containing ".." to absolute ones, which would reintroduce the
# non-ASCII problem, so the artifacts are copied to HTMODLOADER_OUTPUT_DIR
# afterwards.
set(HTMODLOADER_LINK_OUTPUT_DIR "dist")

set_target_properties(winhttp PROPERTIES
  OUTPUT_NAME winhttp
  PREFIX ""
  SUFFIX ".dll"
  RUNTIME_OUTPUT_DIRECTORY "${HTMODLOADER_LINK_OUTPUT_DIR}"
  # Import library consumed by mods: htmodloader.lib.
  ARCHIVE_OUTPUT_DIRECTORY "${HTMODLOADER_LINK_OUTPUT_DIR}"
  ARCHIVE_OUTPUT_NAME htmodloader
  IMPORT_PREFIX ""
  IMPORT_SUFFIX ".lib"
)

htmodloader_apply_common_flags(winhttp)

# LTO is applied to the loader sources only, matching the previous Makefile.
if (HTMODLOADER_ENABLE_LTO)
  set_property(TARGET winhttp PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# Strip the final binary in Release builds, as the Makefile did with -s.
target_link_options(winhttp PRIVATE $<$<CONFIG:Release>:-s>)

# Copy the DLL and its import library next to the sources (dist/), where the
# packaging scripts expect them.
add_custom_command(TARGET winhttp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${HTMODLOADER_OUTPUT_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:winhttp>"
          "${HTMODLOADER_OUTPUT_DIR}/winhttp.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_LINKER_FILE:winhttp>"
          "${HTMODLOADER_OUTPUT_DIR}/htmodloader.lib"
  COMMENT "Copying winhttp.dll and htmodloader.lib to ${HTMODLOADER_OUTPUT_DIR}"
)

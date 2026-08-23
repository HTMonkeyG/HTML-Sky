SHELL := cmd.exe

MAKEFLAGS += -s -j4


############################################
# Directories
############################################

DIST_DIR = dist
SRC_DIR = src


SRC_DIRS = $(SRC_DIR) $(wildcard $(SRC_DIR)/*/)


############################################
# Source files
############################################

C_SRC = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c)

CPP_SRC = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp)


C_OBJ = $(addprefix $(DIST_DIR)/,$(notdir $(C_SRC:.c=.o)))

CPP_OBJ = $(addprefix $(DIST_DIR)/,$(notdir $(CPP_SRC:.cpp=.o)))


CXX_HEADER = $(wildcard $(SRC_DIR)/*.h $(SRC_DIR)/*/*.h)



############################################
# Target
############################################

TARGET = winhttp.dll

BIN_TARGET = $(DIST_DIR)/$(TARGET)



############################################
# Compiler
############################################

CC = gcc

CXX = g++



############################################
# Compile flags
############################################

CFLAGS = \
-Wall \
-Wformat \
-Wno-unused-function \
-Wno-stringop-overread \
-O3 \
-ffunction-sections \
-fdata-sections \
-static \
-flto=auto \
-s


CFLAGS += \
-I./src



############################################
# Link flags
############################################

LFLAGS = \
-Wl,--gc-sections \
-Wl,-O3 \
-Wl,--out-implib,$(DIST_DIR)/htmodloader.lib



############################################
# System libraries
############################################

LFLAGS += \
-lgdi32 \
-ldwmapi \
-ld3dcompiler \
-lstdc++ \
-limm32



############################################
# ImGui
############################################

CFLAGS += \
-I./libraries/imgui-1.92.2b \
-I./libraries/imgui-1.92.2b/backends


LFLAGS += \
./libraries/imgui-1.92.2b/libimgui.a \
./libraries/imgui-1.92.2b/libimgui_impl_win32.a \
./libraries/imgui-1.92.2b/libimgui_impl_vulkan.a \
./libraries/imgui-1.92.2b/libimgui_impl_opengl3.a



############################################
# MinHook
############################################

CFLAGS += \
-I./libraries/MinHook/include


LFLAGS += \
./libraries/MinHook/libMinHook.a



############################################
# Vulkan
############################################

CFLAGS += \
-I./libraries/vulkan/Include


# Windows MinGW uses system vulkan import library
LFLAGS += \
-lvulkan-1



############################################
# cJSON
############################################

CFLAGS += \
-I./libraries/cJSON


LFLAGS += \
./libraries/cJSON/libcjson.a



############################################
# LevelDB
############################################

CFLAGS += \
-I./libraries/leveldb/include


LFLAGS += \
./libraries/leveldb/lib/libleveldb.a \
./libraries/leveldb/lib/libz.a



############################################
# Macro
############################################

CFLAGS += \
-DNDEBUG \
-DHTMLAPIATTR=__declspec(dllexport)



############################################
# VPATH
############################################

vpath %.c $(SRC_DIRS)

vpath %.cpp $(SRC_DIRS)



############################################
# Phony
############################################

.PHONY: all clean clean_libs clean_all libs



############################################
# Link DLL
############################################

$(BIN_TARGET): $(C_OBJ) $(CPP_OBJ)

	@echo Linking...

	@"$(CXX)" \
	--std=c++17 \
	$(CFLAGS) \
	$^ \
	-shared \
	-o "$@" \
	$(LFLAGS)

	@echo Build finished.



############################################
# Compile C
############################################

$(DIST_DIR)/%.o: %.c $(CXX_HEADER)

	@echo Compiling $<

	@"$(CC)" \
	--std=c11 \
	$(CFLAGS) \
	-c "$<" \
	-o "$@"



############################################
# Compile C++
############################################

$(DIST_DIR)/%.o: %.cpp $(CXX_HEADER)

	@echo Compiling $<

	@"$(CXX)" \
	$(CFLAGS) \
	-c "$<" \
	-o "$@"



############################################
# Create dist
############################################

$(DIST_DIR):

	@if not exist $(DIST_DIR) mkdir $(DIST_DIR)



############################################
# Build
############################################

all: $(DIST_DIR) libs $(BIN_TARGET)



############################################
# Libraries
############################################

libs:

	@echo Building libraries...


	@$(MAKE) -s -C ./libraries/imgui-1.92.2b all


	@$(MAKE) -s -C ./libraries/MinHook libMinHook.a


	@$(MAKE) -s -C ./libraries/cJSON libcjson.a



############################################
# Clean
############################################

clean:

	@if exist dist\*.o del /q dist\*.o

	@if exist dist\*.dll del /q dist\*.dll

	@if exist dist\*.lib del /q dist\*.lib



clean_libs:

	@$(MAKE) -s -C ./libraries/imgui-1.92.2b clean

	@$(MAKE) -s -C ./libraries/MinHook clean

	@$(MAKE) -s -C ./libraries/cJSON clean



clean_all:

	@$(MAKE) clean_libs

	@$(MAKE) clean

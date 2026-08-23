SHELL := cmd.exe

MAKEFLAGS += -s -j4


DIST_DIR = ./dist
SRC_DIR = ./src


DEF = $(SRC_DIR)/proxy/winhttp-proxy.def


SRC_DIRS = $(SRC_DIR) $(wildcard $(SRC_DIR)/*/)


C_SRC = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c)
CPP_SRC = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp)


C_OBJ = $(addprefix $(DIST_DIR)/, $(notdir $(C_SRC:.c=.o)))
CPP_OBJ = $(addprefix $(DIST_DIR)/, $(notdir $(CPP_SRC:.cpp=.o)))


CXX_HEADER = $(wildcard $(SRC_DIR)/*.h $(SRC_DIR)/*/*.h)


TARGET = winhttp.dll

BIN_TARGET = $(DIST_DIR)/$(TARGET)



# Compiler

CC = gcc
CXX = g++



# Compiler params

CFLAGS = -Wall -Wformat -Wno-unused-function -Wno-stringop-overread \
-O3 -ffunction-sections -fdata-sections \
-static -flto=auto -s

CFLAGS += -I./src



# Link params

LFLAGS = -Wl,--gc-sections,-O3,--version-script,$(SRC_DIR)/exports.txt \
-Wl,--out-implib,$(DIST_DIR)/htmodloader.lib

LFLAGS += \
-lgdi32 \
-ldwmapi \
-ld3dcompiler \
-lstdc++ \
-limm32



# ImGui

CFLAGS += \
-I./libraries/imgui-1.92.2b \
-I./libraries/imgui-1.92.2b/backends


LFLAGS += \
-L./libraries/imgui-1.92.2b \
-limgui \
-limgui_impl_win32 \
-limgui_impl_vulkan \
-limgui_impl_opengl3



# MinHook

CFLAGS += -I./libraries/MinHook/include

LFLAGS += \
-L./libraries/MinHook \
-lMinHook



# Vulkan

CFLAGS += -I./libraries/vulkan/Include

LFLAGS += \
-L./libraries/vulkan/Lib \
-lvulkan-1



# cJSON

CFLAGS += -I./libraries/cJSON

LFLAGS += \
-L./libraries/cJSON \
-lcjson



# LevelDB

CFLAGS += -I./libraries/leveldb/include

LFLAGS += \
-L./libraries/leveldb/lib \
-leveldb \
-lz



# Macro

CFLAGS += \
-DNDEBUG \
-DHTMLAPIATTR=__declspec(dllexport)



vpath %.c $(SRC_DIRS)
vpath %.cpp $(SRC_DIRS)



.PHONY: all clean clean_libs clean_all libs



##################################################
# Link DLL
##################################################

$(BIN_TARGET): $(C_OBJ) $(CPP_OBJ)
	@echo Linking ...
	@"$(CXX)" --std=c++17 $(CFLAGS) $^ -shared -o "$@" $(LFLAGS)
	@echo Done.



##################################################
# Compile C
##################################################

$(DIST_DIR)/%.o: %.c $(CXX_HEADER)
	@echo Compiling file "$<" ...
	@"$(CC)" --std=c11 $(CFLAGS) -c "$<" -o "$@"



##################################################
# Compile C++
##################################################

$(DIST_DIR)/%.o: %.cpp $(CXX_HEADER)
	@echo Compiling file "$<" ...
	@"$(CXX)" $(CFLAGS) -c "$<" -o "$@"



##################################################
# Create dist
##################################################

$(DIST_DIR):
	@if not exist dist mkdir dist



##################################################
# Build all
##################################################

all: $(DIST_DIR) libs $(BIN_TARGET)



##################################################
# Build libraries
##################################################

libs:
	@echo Compiling libraries ...

	@$(MAKE) -s -C ./libraries/imgui-1.92.2b all

	@$(MAKE) -s -C ./libraries/MinHook libMinHook.a

	@$(MAKE) -s -C ./libraries/cJSON libcjson.a



##################################################
# Clean
##################################################

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

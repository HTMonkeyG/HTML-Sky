MAKEFLAGS += -s -j6

DIST_DIR = ./dist
SRC_DIR = ./src

DEF = $(SRC_DIR)/proxy/winhttp-proxy.def

SRC_DIRS = $(SRC_DIR) $(wildcard $(SRC_DIR)/*/)

C_SRC = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c)
CPP_SRC = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp)

C_OBJ = $(addprefix $(DIST_DIR)/, $(notdir $(C_SRC:.c=.o)))
CPP_OBJ = $(addprefix $(DIST_DIR)/, $(notdir $(CPP_SRC:.cpp=.o)))

TARGET = winhttp.dll
BIN_TARGET = $(DIST_DIR)/$(TARGET)

# Compiler paths.
CC = gcc
CXX = g++

# Params.
CFLAGS = -Wall -Wformat -O3 -ffunction-sections -fdata-sections -static -flto -s
CFLAGS += -I./src
# Include ImGui.
CFLAGS += -I./libraries/imgui-1.91.9b -I./libraries/imgui-1.91.9b/backends
# Include MinHook.
CFLAGS += -I./libraries/MinHook/include
# Include Vulkan.
CFLAGS += -I./libraries/vulkan/Include
# Macros.
#CFLAGS += -DNDEBUG

LFLAGS = -Wl,--gc-sections,-O3,--out-implib,$(DIST_DIR)/winhttp.lib,--export-all-symbols
LFLAGS += -lgdi32 -ldwmapi -ld3dcompiler -lstdc++
LFLAGS += -L./libraries/MinHook -lMinHook
LFLAGS += -L./libraries/vulkan/Lib -lvulkan-1
LFLAGS += -L./libraries/imgui-1.91.9b -limgui -limgui_impl_win32 -limgui_impl_vulkan -limgui_demo

vpath %.c $(SRC_DIRS)
vpath %.cpp $(SRC_DIRS)

.PHONY: all clean libs clean_libs clean_all

$(BIN_TARGET): $(C_OBJ) $(CPP_OBJ)
	@echo Linking ...
	@$(CXX) $(CFLAGS) $^ -shared -o $@ $(LFLAGS)
	@echo Done.

$(DIST_DIR)/%.o: %.c
	@echo Compiling file "$<" ...
	@$(CC) $(CFLAGS) -c $< -o $@

$(DIST_DIR)/%.o: %.cpp
	@echo Compiling file "$<" ...
	@$(CXX) $(CFLAGS) -c $< -o $@

clean_all: clean_libs clean

clean:
	-@del .\dist\*.o
	-@del .\dist\*.dll
	-@del .\dist\*.lib

all: libs
	-@$(MAKE) $(BIN_TARGET)

libs:
	@echo Compiling libraries ...
	-@$(MAKE) -s -C ./libraries/imgui-1.91.9b all
	-@$(MAKE) -s -C ./libraries/MinHook libMinHook.a

clean_libs:
	-@$(MAKE) -s -C ./libraries/imgui-1.91.9b clean
	-@$(MAKE) -s -C ./libraries/MinHook clean

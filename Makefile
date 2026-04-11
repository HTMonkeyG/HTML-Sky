MAKEFLAGS += -s -j

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

# Compiler paths.
CC = gcc
CXX = g++

# Params.
CFLAGS = -Wall -Wformat -Wno-unused-function -Wno-stringop-overread -O3 -ffunction-sections -fdata-sections -static -flto=auto -s
CFLAGS += -I./src
LFLAGS = -Wl,--gc-sections,-O3,--version-script,$(SRC_DIR)/exports.txt,--out-implib,$(DIST_DIR)/htmodloader.lib
LFLAGS += -lgdi32 -ldwmapi -ld3dcompiler -lstdc++ -limm32
# Include ImGui.
CFLAGS += -I./libraries/imgui-1.92.2b -I./libraries/imgui-1.92.2b/backends
LFLAGS += -L./libraries/imgui-1.92.2b -limgui -limgui_impl_win32 -limgui_impl_vulkan -limgui_impl_opengl3
# Include MinHook.
CFLAGS += -I./libraries/MinHook/include
LFLAGS += -L./libraries/MinHook -lMinHook
# Include Vulkan.
CFLAGS += -I./libraries/vulkan/Include
LFLAGS += -L./libraries/vulkan/Lib -lvulkan-1
# Include cJSON.
CFLAGS += -I./libraries/cJSON
LFLAGS += -L./libraries/cJSON -lcjson
# Include LevelDB.
CFLAGS += -I./libraries/leveldb/include
LFLAGS += -L./libraries/leveldb/lib -lleveldb -lz
# Macros.
CFLAGS += -DNDEBUG -DHTMLAPIATTR=__declspec(dllexport)

vpath %.c $(SRC_DIRS)
vpath %.cpp $(SRC_DIRS)

.PHONY: all clean libs clean_libs clean_all

$(BIN_TARGET): $(C_OBJ) $(CPP_OBJ)
	@echo Linking ...
	@$(CXX) --std=c++17 $(CFLAGS) $^ -shared -o $@ $(LFLAGS)
	@echo Done.

$(DIST_DIR)/%.o: %.c $(CXX_HEADER)
	@echo Compiling file "$<" ...
	@$(CC) --std=c11 $(CFLAGS) -c $< -o $@

$(DIST_DIR)/%.o: %.cpp $(CXX_HEADER)
	@echo Compiling file "$<" ...
	@$(CXX) $(CFLAGS) -c $< -o $@

$(DIST_DIR):
	-@mkdir dist

clean_all: clean_libs clean

clean:
	-@del .\dist\*.o
	-@del .\dist\*.dll
	-@del .\dist\*.lib

all: $(DIST_DIR) libs
	-@$(MAKE) $(BIN_TARGET)

libs:
	@echo Compiling libraries ...
	-@$(MAKE) -s -C ./libraries/imgui-1.92.2b all
	-@$(MAKE) -s -C ./libraries/MinHook libMinHook.a
	-@$(MAKE) -s -C ./libraries/cJSON libcjson.a

clean_libs:
	-@$(MAKE) -s -C ./libraries/imgui-1.92.2b clean
	-@$(MAKE) -s -C ./libraries/MinHook clean
	-@$(MAKE) -s -C ./libraries/cJSON clean

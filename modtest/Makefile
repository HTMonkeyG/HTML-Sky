MAKEFLAGS += -s -j6

DIST_DIR = ./dist
SRC_DIR = ./src

SRC_DIRS = $(SRC_DIR) $(wildcard $(SRC_DIR)/*/)

C_SRC = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c)
CPP_SRC = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp)

C_OBJ = $(addprefix $(DIST_DIR)/, $(notdir $(C_SRC:.c=.o)))
CPP_OBJ = $(addprefix $(DIST_DIR)/, $(notdir $(CPP_SRC:.cpp=.o)))

TARGET = modtest.dll
BIN_TARGET = $(DIST_DIR)/$(TARGET)

# Compiler paths.
CC = gcc
CXX = g++

# Params.
CFLAGS = -Wall -Wformat -O3 -ffunction-sections -fdata-sections -static -flto -s
CFLAGS += -I./src
LFLAGS = -Wl,--gc-sections,-O3
CFLAGS += -I../libraries/imgui-1.91.9b
CFLAGS += -I../src

LFLAGS += -L../dist -lhtmodloader

vpath %.c $(SRC_DIRS)
vpath %.cpp $(SRC_DIRS)

.PHONY: all clean

$(BIN_TARGET): $(C_OBJ) $(CPP_OBJ)
	@echo Linking ...
	@$(CXX) --std=c++11 $(CFLAGS) $^ -shared -o $@ $(LFLAGS)
	@echo Done.

$(DIST_DIR)/%.o: %.c
	@echo Compiling file "$<" ...
	@$(CC) --std=c11 $(CFLAGS) -c $< -o $@

$(DIST_DIR)/%.o: %.cpp
	@echo Compiling file "$<" ...
	@$(CXX) $(CFLAGS) -c $< -o $@

clean:
	-@del .\dist\*.o
	-@del .\dist\*.dll


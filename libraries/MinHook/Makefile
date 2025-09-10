WINDRES:=$(CROSS_PREFIX)windres
DLLTOOL:=$(CROSS_PREFIX)dlltool
AR:=$(CROSS_PREFIX)ar
CC:=$(CROSS_PREFIX)gcc
CCLD:=$(CC)
SRCS:=$(wildcard src/*.c src/hde/*.c)
OBJS:=$(SRCS:%.c=%.o)
DEPS:=$(SRCS:%.c=%.d)
INCS:=-Isrc -Iinclude
CFLAGS:=-masm=intel -Wall -Werror -std=c11
LDFLAGS:=-Wl,-enable-stdcall-fixup -s -static-libgcc

all: MinHook.dll libMinHook.dll.a libMinHook.a

-include $(DEPS)

libMinHook.a: $(OBJS)
	@$(AR) rcs $@ $^

.rc.res:
	@$(WINDRES) -o $@ --input-format=rc --output-format=coff $<
.c.o:
	@$(CC) -o $@ -c -MMD -MP $(INCS) $(CFLAGS) $<

clean:
	-@del src\*.o
	-@del libMinHook.a

.PHONY: clean
.SUFFIXES: .rc .res

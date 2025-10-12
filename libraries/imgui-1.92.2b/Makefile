CXX = g++
AR = ar

CFLAGS = -std=c++11 -g -Wall -Wformat -I. -I./backends -I../vulkan/Include

all: libimgui.a libimgui_impl_win32.a libimgui_impl_vulkan.a

libimgui.a: ./imgui_draw.o ./imgui_tables.o ./imgui_widgets.o ./imgui.o
	$(AR) rcs $@ $^

libimgui_impl_win32.a: ./imgui_impl_win32.o
	$(AR) rcs $@ $<

libimgui_impl_vulkan.a: ./imgui_impl_vulkan.o
	$(AR) rcs $@ $<

./imgui_draw.o: ./imgui_draw.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

./imgui_tables.o: ./imgui_tables.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

./imgui_widgets.o: ./imgui_widgets.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

./imgui.o: ./imgui.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

./imgui_impl_win32.o: ./backends/imgui_impl_win32.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

./imgui_impl_vulkan.o: ./backends/imgui_impl_vulkan.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	del *.o
	del *.a

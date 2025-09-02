CXX = gcc
AR = ar

CFLAGS = -g -Wall -Wformat -I.
CFLAGS += -I../MinHook/include

all: libskyre.a

libskyre.a: ./setup.o ./log.o ./sigscan.o
	$(AR) rcs $@ $^

./setup.o: ./setup.c
	$(CXX) $(CFLAGS) -c $< -o $@

./log.o: ./log.c
	$(CXX) $(CFLAGS) -c $< -o $@

./sigscan.o: ./sigscan.c
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	del *.o
	del *.a

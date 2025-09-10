CC = gcc
AR = ar

CFLAGS = -std=c11 -g -Wall -Wformat -I.

all: libcjson.a

libcjson.a: ./cJSON.o
	$(AR) rcs $@ $^

./cJSON.o: ./cJSON.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del *.o
	del *.a

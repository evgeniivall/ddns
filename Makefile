CC=gcc
LD=gcc
CFLAGS=-c -Wall
LFLAGS=

TARGET=controller handler agent

.PHONY: all clean

all: $(TARGET)

$(TARGET): $@ net.o
	$(LD) $(LFLAGS) $@.c net.o -o $@

net.o: net.c net.h
	$(CC) $(CFLAGS) net.c
clean:
	rm -rf $(TARGET)

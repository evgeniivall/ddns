CC=gcc
LD=gcc
CFLAGS=-Wall
LFLAGS=

TARGET=controller handler agent

.PHONY: all clean

all: $(TARGET)

controller: controller.c ddos.h net_sockets.o
	$(CC) $(CFLAGS) net_sockets.o controller.c -o $@

handler: handler.c ddos.h net_sockets.o
	$(CC) $(CFLAGS) net_sockets.o handler.c -o $@

agent: agent.c ddos.h net_sockets.o
	$(CC) $(CFLAGS) net_sockets.o agent.c -o $@

net_sockets.o:net_sockets.c net_sockets.h
	$(CC) $(CFLAGS) -c net_sockets.c
clean:
	rm -rf $(TARGET) net_sockets.o

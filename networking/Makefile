CC=gcc
CFLAGS=-Wall -Wextra -O2 -Iinclude
LDFLAGS=
LDLIBS=-lcrypto

SRC_DIR=.
BIN_DIR=.

COMMON=$(SRC_DIR)/common.c

all: server client

server: $(SRC_DIR)/server.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/server $(LDLIBS)

client: $(SRC_DIR)/client.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/client $(LDLIBS)

clean:
	rm -f server client *.o

.PHONY: all clean

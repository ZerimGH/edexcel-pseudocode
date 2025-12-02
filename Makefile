CC=gcc
CFLAGS=-Wall -pedantic
SRCS=src/*.c
OUT_DIR=./build
OUT_EXEC=edxp

linux:
	mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $(SRCS) -o $(OUT_DIR)/$(OUT_EXEC)

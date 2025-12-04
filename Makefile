CC = gcc
CFLAGS = -Wall -pedantic
SRCS = src/*.c
OUT_DIR = ./build
OUT_EXEC = edxp

ifeq ($(OS), Windows_NT)
    MKDIR = mkdir $(OUT_DIR)
    RM    = rmdir /S /Q $(OUT_DIR)
else
    MKDIR = mkdir -p $(OUT_DIR)
    RM    = rm -rf $(OUT_DIR)
endif

all: edxp

edxp:
	$(MKDIR)
	$(CC) $(CFLAGS) $(SRCS) -o $(OUT_DIR)/$(OUT_EXEC)

clean:
	$(RM)	


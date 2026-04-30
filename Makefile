# ENCS4330 Project #1 — Home Furnishing Competition
# Builds on macOS (Apple Clang + Homebrew libomp) and Linux (gcc + libgomp).

UNAME_S := $(shell uname -s)

CC      := cc
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wno-unused-parameter
OPT     := -O2 -g
INC     := -Iinclude

# OpenMP flags differ between Apple Clang and gcc/clang on Linux
ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix libomp 2>/dev/null)
    ifneq ($(BREW_PREFIX),)
        OMP_CFLAGS := -Xpreprocessor -fopenmp -I$(BREW_PREFIX)/include
        OMP_LDLIBS := -L$(BREW_PREFIX)/lib -lomp
    else
        OMP_CFLAGS :=
        OMP_LDLIBS :=
    endif
else
    OMP_CFLAGS := -fopenmp
    OMP_LDLIBS := -fopenmp
endif

CFLAGS  := $(CSTD) $(WARN) $(OPT) $(INC) $(OMP_CFLAGS)
LDFLAGS :=
LDLIBS  := $(OMP_LDLIBS)

SRC_DIR := src
OBJ_DIR := build
BIN     := furnish

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(BIN)
	./$(BIN) config.txt

clean:
	rm -rf $(OBJ_DIR) $(BIN) /tmp/furnish_referee.fifo

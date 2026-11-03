CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

DDS_CFLAGS  := $(shell pkg-config --cflags CycloneDDS 2>/dev/null)
DDS_LIBS    := $(shell pkg-config --libs CycloneDDS 2>/dev/null || echo -lddsc)
SODIUM_LIBS := $(shell pkg-config --libs libsodium 2>/dev/null || echo -lsodium)
GLFW_LIBS   := $(shell pkg-config --libs glfw3 2>/dev/null || echo -lglfw)

BUILD_DIR := build
BIN_DIR   := bin

GEN_SRC    := $(BUILD_DIR)/messages.c
GEN_HDR    := $(BUILD_DIR)/messages.h
COMMON_SRC := src/common/secure.c $(GEN_SRC)
COMMON_HDR := src/common/secure.h src/common/pong_common.h $(GEN_HDR)
INCLUDES   := -Isrc/common -I$(BUILD_DIR) -Ithird_party

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

$(GEN_HDR): idl/messages.idl | $(BUILD_DIR)
	idlc -l c -o $(BUILD_DIR) idl/messages.idl

$(GEN_SRC): $(GEN_HDR)

$(BIN_DIR)/server: src/server/server.c $(COMMON_SRC) $(COMMON_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(DDS_CFLAGS) -o $@ src/server/server.c $(COMMON_SRC) $(DDS_LIBS) $(SODIUM_LIBS) -pthread -lm

$(BIN_DIR)/client: src/client/client.c third_party/stb_easy_font.h $(COMMON_SRC) $(COMMON_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-missing-braces $(INCLUDES) $(DDS_CFLAGS) -o $@ src/client/client.c $(COMMON_SRC) $(GLFW_LIBS) -lGL $(DDS_LIBS) $(SODIUM_LIBS) -pthread -lm

adduser: $(BIN_DIR)/server
	@test -n "$(NAME)" -a -n "$(PASS)" || (echo "Usage: make adduser NAME=<name> PASS=<password>"; exit 1)
	./$(BIN_DIR)/server adduser "$(NAME)" "$(PASS)"

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean adduser

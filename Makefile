CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

DDS_CFLAGS  := $(shell pkg-config --cflags CycloneDDS 2>/dev/null)
DDS_LIBS    := $(shell pkg-config --libs CycloneDDS 2>/dev/null || echo -lddsc)
SODIUM_LIBS := $(shell pkg-config --libs libsodium 2>/dev/null || echo -lsodium)
GLFW_LIBS   := $(shell pkg-config --libs glfw3 2>/dev/null || echo -lglfw)

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := bin

GEN_SRC := $(BUILD_DIR)/messages.c
GEN_HDR := $(BUILD_DIR)/messages.h

COMMON_SRC := $(wildcard src/common/*.c)
SERVER_SRC := $(wildcard src/server/*.c)
CLIENT_SRC := $(wildcard src/client/*.c)

COMMON_OBJ := $(COMMON_SRC:src/%.c=$(OBJ_DIR)/%.o) $(OBJ_DIR)/messages.o
SERVER_OBJ := $(SERVER_SRC:src/%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJ := $(CLIENT_SRC:src/%.c=$(OBJ_DIR)/%.o)

COMMON_INC := -Isrc/common -I$(BUILD_DIR)
SERVER_INC := $(COMMON_INC) -Isrc/server
CLIENT_INC := $(COMMON_INC) -Isrc/client -Ithird_party

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(GEN_HDR): idl/messages.idl
	@mkdir -p $(BUILD_DIR)
	idlc -l c -o $(BUILD_DIR) idl/messages.idl

$(GEN_SRC): $(GEN_HDR)

$(OBJ_DIR)/messages.o: $(GEN_SRC)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DDS_CFLAGS) -I$(BUILD_DIR) -c $< -o $@

$(OBJ_DIR)/common/%.o: src/common/%.c | $(GEN_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pthread $(DDS_CFLAGS) $(COMMON_INC) -MMD -MP -c $< -o $@

$(OBJ_DIR)/server/%.o: src/server/%.c | $(GEN_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pthread $(DDS_CFLAGS) $(SERVER_INC) -MMD -MP -c $< -o $@

$(OBJ_DIR)/client/%.o: src/client/%.c | $(GEN_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pthread -Wno-unused-function -Wno-missing-braces $(DDS_CFLAGS) $(CLIENT_INC) -MMD -MP -c $< -o $@

$(BIN_DIR)/server: $(SERVER_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(DDS_LIBS) $(SODIUM_LIBS) -pthread -lm

$(BIN_DIR)/client: $(CLIENT_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(GLFW_LIBS) -lGL $(DDS_LIBS) $(SODIUM_LIBS) -pthread -lm

adduser: $(BIN_DIR)/server
	@test -n "$(NAME)" -a -n "$(PASS)" || (echo "Usage: make adduser NAME=<name> PASS=<password>"; exit 1)
	./$(BIN_DIR)/server adduser "$(NAME)" "$(PASS)"

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean adduser

-include $(wildcard $(OBJ_DIR)/*/*.d)

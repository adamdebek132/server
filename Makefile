CC := gcc
CFLAGS := -O2 -Wall
LDFLAGS := -I./common
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
# Client
CLIENT_SRC_DIRS := client common
CLIENT_SRCS := $(wildcard $(addsuffix /*.c, $(CLIENT_SRC_DIRS)))
CLIENT_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(CLIENT_SRCS)))
# Server
SERVER_SRC_DIRS := server common
SERVER_SRCS := $(wildcard $(addsuffix /*.c, $(SERVER_SRC_DIRS)))
SERVER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SERVER_SRCS)))

EXE := client server


all: $(EXE)

client: $(CLIENT_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(BUILD_DIR)/bin/$@

server: $(SERVER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(BUILD_DIR)/bin/$@

$(BUILD_DIR)/%.o: client/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: server/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: common/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

clean:
	@rm -rf $(BUILD_DIR)/*

.PHONY: all clean

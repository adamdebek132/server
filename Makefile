CC := gcc
CFLAGS := -O2 -Wall
LDFLAGS := -Isrc/common
BUILD_DIR := build
# Client
CLIENT_SRCS := $(wildcard src/client/*.c) src/common/common.c
CLIENT_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(CLIENT_SRCS)))
# Server
SERVER_SRCS := $(wildcard src/server/*.c) src/common/common.c
SERVER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SERVER_SRCS)))

all: client server

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(BUILD_DIR)/$@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(BUILD_DIR)/$@

$(BUILD_DIR)/%.o: src/client/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/server/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/common/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

install: client server
	install -m 755 $(BUILD_DIR)/client /usr/local/bin/client
	install -m 755 $(BUILD_DIR)/server /usr/local/bin/server

clean:
	@rm -rf $(BUILD_DIR)/*

.PHONY: all clean install

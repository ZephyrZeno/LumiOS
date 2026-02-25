# lumid - LumiOS Init System / LumiOS 初始化系统
# ================================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS  ?= -static
BUILD_DIR ?= ./build
PREFIX   ?= /usr/local

SRCS     := src/main.c \
            src/service.c \
            src/supervisor.c \
            src/config.c \
            src/cgroup.c \
            src/socket.c \
            src/mount.c \
            src/log.c \
            src/util.c

OBJS     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET   := $(BUILD_DIR)/lumid
CTL_SRCS := src/lumictl.c src/socket.c src/util.c
CTL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/ctl_%.o,$(CTL_SRCS))
CTL_TARGET := $(BUILD_DIR)/lumictl

.PHONY: all clean install test

all: $(TARGET) $(CTL_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(BUILD_DIR)/ctl_%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -DLUMICTL_BUILD -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

$(CTL_TARGET): $(CTL_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

install: $(TARGET) $(CTL_TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/sbin/lumid
	install -Dm755 $(CTL_TARGET) $(PREFIX)/usr/bin/lumictl
	install -dm755 $(PREFIX)/etc/lumid/services

clean:
	rm -rf $(BUILD_DIR)

test:
	@echo "Running lumid unit tests..."
	$(CC) $(CFLAGS) -Iinclude -DLUMID_TEST tests/test_config.c src/config.c src/util.c src/log.c -o $(BUILD_DIR)/test_config
	$(BUILD_DIR)/test_config
	$(CC) $(CFLAGS) -Iinclude -DLUMID_TEST tests/test_service.c src/service.c src/config.c src/util.c src/log.c -o $(BUILD_DIR)/test_service
	$(BUILD_DIR)/test_service
	@echo "All tests passed"

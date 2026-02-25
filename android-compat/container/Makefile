# android-container - Android Compatibility Container / Android 兼容容器
# =====================================================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS  ?= -static
BUILD_DIR ?= ./build
PREFIX   ?= /usr/local

SRCS     := src/main.c \
            src/container.c \
            src/namespace.c \
            src/mounts.c \
            src/cgroup.c \
            src/properties.c

OBJS     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET   := $(BUILD_DIR)/android-container

.PHONY: all clean install

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

install: $(TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/usr/bin/android-container

clean:
	rm -rf $(BUILD_DIR)

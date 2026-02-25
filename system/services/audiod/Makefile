# audiod - LumiOS Audio Service / 音频服务
# ==========================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS  ?=
BUILD_DIR ?= ./build
PREFIX   ?= /usr/local

SRCS     := src/main.c src/audiod.c src/pipewire.c src/volume.c
OBJS     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET   := $(BUILD_DIR)/audiod

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
	install -Dm755 $(TARGET) $(PREFIX)/usr/sbin/audiod

clean:
	rm -rf $(BUILD_DIR)

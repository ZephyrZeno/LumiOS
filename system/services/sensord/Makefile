# sensord - LumiOS Sensor Service / 传感器服务
# ===============================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS  += -lm
BUILD_DIR ?= ./build
PREFIX   ?= /usr/local

SRCS     := src/main.c src/sensord.c src/enumerate.c src/reader.c src/fusion.c
OBJS     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET   := $(BUILD_DIR)/sensord

.PHONY: all clean install

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built: $@"

install: $(TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/usr/sbin/sensord

clean:
	rm -rf $(BUILD_DIR)

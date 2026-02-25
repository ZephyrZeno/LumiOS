# telephonyd - LumiOS Telephony Service / 电话服务
# ==================================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS  ?=
BUILD_DIR ?= ./build
PREFIX   ?= /usr/local

SRCS     := src/main.c src/telephonyd.c src/modem.c src/sim.c src/network.c src/call.c src/sms.c src/data.c
OBJS     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET   := $(BUILD_DIR)/telephonyd

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
	install -Dm755 $(TARGET) $(PREFIX)/usr/sbin/telephonyd

clean:
	rm -rf $(BUILD_DIR)

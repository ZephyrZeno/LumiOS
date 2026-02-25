# netd - LumiOS Network Manager / 网络管理服务
# =============================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS  ?=
BUILD_DIR ?= ./build
PREFIX   ?= /usr/local

SRCS     := src/main.c src/netd.c src/wifi.c src/dhcp.c src/dns.c src/monitor.c
OBJS     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET   := $(BUILD_DIR)/netd

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
	install -Dm755 $(TARGET) $(PREFIX)/usr/sbin/netd

clean:
	rm -rf $(BUILD_DIR)

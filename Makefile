# Cross-platform build file for peli

CC ?= gcc
CFLAGS += -Iinclude
INCLUDES := -Iinclude
SOURCES := $(wildcard src/*.c)
DATA_SRC := master_data
PYTHON ?= python3

world-map-sync:
	@echo "Refreshing world map CSV from spreadsheet..."
	@$(PYTHON) tools/generate_world_map_sheet.py || echo "World map sync skipped: Python unavailable or script failed."

ifeq ($(OS),Windows_NT)
OUT_DIR := builds\\build-win
TARGET := $(OUT_DIR)\\peli.exe
RUN_CMD := $(TARGET)
MKDIR_CMD := if not exist "$(OUT_DIR)" mkdir "$(OUT_DIR)"
LDFLAGS += -lwinmm
else
OUT_DIR := builds/build-lin
TARGET := $(OUT_DIR)/peli
RUN_CMD := ./$(TARGET)
MKDIR_CMD := mkdir -p $(OUT_DIR)
CFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -D_XOPEN_SOURCE=700
endif

.PHONY: all build run clean debug sync-data build-windows build-linux

all: build

build: world-map-sync $(TARGET) sync-data

$(TARGET): $(SOURCES)
	$(MKDIR_CMD)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

sync-data:
	@$(PYTHON) tools/sync_runtime_data.py $(OUT_DIR)

run: build
	$(RUN_CMD)

clean:
ifeq ($(OS),Windows_NT)
	-del /Q builds\build-win\peli.exe 2>nul || exit 0
	-if exist builds\build-win\data rmdir /S /Q builds\build-win\data
else
	rm -f builds/build-win/peli.exe builds/build-lin/peli
	rm -rf builds/build-win/data builds/build-lin/data
endif

debug: CFLAGS := -std=c11 -O0 -g -Wall -Wextra
debug: build

# Explicit platform targets
build-windows:
	@echo Building Windows binary...
	@$(MAKE) OS=Windows_NT build

build-linux:
	@echo Building Linux binary...
ifeq ($(OS),Windows_NT)
	@wsl -u root bash -lc 'apt-get update && apt-get install -y build-essential && mkdir -p /mnt/d/projekti/peli\ -main/builds/build-lin && cd /mnt/d/projekti/peli\ -main && gcc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude src/*.c -o builds/build-lin/peli && rm -rf builds/build-lin/data && cp -R master_data builds/build-lin/data && echo "✓ Build complete: builds/build-lin/peli with data folder"'
else
	@$(MAKE) build
endif

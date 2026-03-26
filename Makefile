# Cross-platform build file for peli

CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra
INCLUDES := -Iinclude
SOURCES := $(wildcard src/*.c)
DATA_SRC := data

ifeq ($(OS),Windows_NT)
OUT_DIR := build-win
TARGET := $(OUT_DIR)/peli.exe
RUN_CMD := $(TARGET)
MKDIR_CMD := if not exist $(OUT_DIR) mkdir $(OUT_DIR)
else
OUT_DIR := build-lin
TARGET := $(OUT_DIR)/peli
RUN_CMD := ./$(TARGET)
MKDIR_CMD := mkdir -p $(OUT_DIR)
CFLAGS += -D_POSIX_C_SOURCE=200809L
endif

.PHONY: all build run clean debug sync-data build-windows build-linux

all: build

build: $(TARGET) sync-data

$(TARGET): $(SOURCES)
	$(MKDIR_CMD)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

sync-data:
ifeq ($(OS),Windows_NT)
	@if not exist $(DATA_SRC) (echo ERROR: Missing $(DATA_SRC) templates folder. & exit /B 1)
	@if exist $(OUT_DIR)\data rmdir /S /Q $(OUT_DIR)\data
	@if exist $(OUT_DIR)\template_data rmdir /S /Q $(OUT_DIR)\template_data
	@xcopy $(DATA_SRC) $(OUT_DIR)\data /E /I /Y >nul
	@xcopy $(DATA_SRC) $(OUT_DIR)\template_data /E /I /Y >nul
else
	@test -d $(DATA_SRC) || (echo "ERROR: Missing $(DATA_SRC) templates folder." && exit 1)
	@mkdir -p $(OUT_DIR)
	@rm -rf $(OUT_DIR)/data
	@rm -rf $(OUT_DIR)/template_data
	@cp -R $(DATA_SRC) $(OUT_DIR)/data
	@cp -R $(DATA_SRC) $(OUT_DIR)/template_data
endif

run: build
	$(RUN_CMD)

clean:
ifeq ($(OS),Windows_NT)
	-del /Q build-win\peli.exe 2>nul || exit 0
	-del /Q build-lin\peli 2>nul || exit 0
	-if exist build-win\data rmdir /S /Q build-win\data
	-if exist build-lin\data rmdir /S /Q build-lin\data
else
	rm -f build-win/peli.exe build-lin/peli
	rm -rf build-win/data build-lin/data
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
	@wsl -u root bash -lc 'apt-get update && apt-get install -y build-essential && mkdir -p /mnt/d/projekti/peli\ -main/build-lin && cd /mnt/d/projekti/peli\ -main && gcc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude src/*.c -o build-lin/peli && rm -rf build-lin/data && cp -R build/data build-lin/data && echo "✓ Build complete: build-lin/peli with data folder"'
else
	@$(MAKE) build
endif

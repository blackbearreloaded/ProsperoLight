# ps5-native-app-boilerplate - Linux/WSL build entry points.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

SHELL := /bin/bash
.DEFAULT_GOAL := app

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)

.PHONY: all app build libc deps format format-check tidy lint check ffpkg ffpfsc packages clean distclean help

all: app
build: app

deps:
	@bash tools/setup-native-dependencies.sh

libc:
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@bash tools/rebuild-libc.sh

app: $(RUNTIME)
	@bash tools/build.sh Folder

ffpkg: $(RUNTIME)
	@bash tools/build.sh Ffpkg

ffpfsc: $(RUNTIME)
	@bash tools/build.sh Ffpfsc

packages: $(RUNTIME)
	@bash tools/build.sh All

format:
	@bash tools/run_clang_format.sh

format-check:
	@bash tools/run_clang_format.sh --check

tidy:
	@bash tools/run_clang_tidy.sh

lint:
	@bash tools/lint.sh

check: lint app

clean:
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)

distclean: clean
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make                 Generate libc.prx and build the Hello World folder' \
	  'make deps            Fetch native dependencies into .deps/' \
	  'make libc            Force a deterministic runtime/libc.prx rebuild' \
	  'make format          Apply the shared Clang formatting policy' \
	  'make format-check    Check formatting without modifying files' \
	  'make tidy            Run the shared Clang static-analysis policy' \
	  'make lint            Run format, tidy, metadata, and shell checks' \
	  'make check           Run lint and build the skeleton app' \
	  'make ffpkg           Build the folder and UFS2 .ffpkg image' \
	  'make ffpfsc          Build the folder and compressed .ffpfsc image' \
	  'make packages        Build folder, .ffpkg, and .ffpfsc outputs' \
	  'make clean           Remove build/, dist/, and generated libc.prx' \
	  'make distclean       Also remove the ignored .deps/ cache'

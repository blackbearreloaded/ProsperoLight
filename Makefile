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
	@printf '%s\n' '==> [deps] Fetching native SDK and zlib dependencies'
	@bash tools/setup-native-dependencies.sh

libc:
	@printf '%s\n' '==> [libc] Rebuilding and verifying the clean-room runtime'
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

app: $(RUNTIME)
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder

ffpkg: $(RUNTIME)
	@printf '%s\n' '==> [ffpkg] Building the app folder and UFS2 image'
	@bash tools/build.sh Ffpkg

ffpfsc: $(RUNTIME)
	@printf '%s\n' '==> [ffpfsc] Building the app folder and compressed image'
	@bash tools/build.sh Ffpfsc

packages: $(RUNTIME)
	@printf '%s\n' '==> [packages] Building the app folder and both package formats'
	@bash tools/build.sh All

format:
	@printf '%s\n' '==> [format] Formatting C and C++ sources'
	@bash tools/run_clang_format.sh

format-check:
	@printf '%s\n' '==> [format] Checking C and C++ formatting'
	@bash tools/run_clang_format.sh --check

tidy:
	@printf '%s\n' '==> [tidy] Running Clang static analysis'
	@bash tools/run_clang_tidy.sh

lint:
	@printf '%s\n' '==> [lint] Running source, metadata, and shell checks'
	@bash tools/lint.sh

check: lint app

clean:
	@printf '%s\n' '==> [clean] Removing generated build outputs'
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)

distclean: clean
	@printf '%s\n' '==> [distclean] Removing downloaded dependency caches'
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

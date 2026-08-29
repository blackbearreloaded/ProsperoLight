# ps5-native-app-boilerplate - Linux/WSL build entry points.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

SHELL := /bin/bash
.DEFAULT_GOAL := app

-include .env

LAN_TELEMETRY ?= 0
APP_DEFINITIONS ?= SDL_MAIN_HANDLED SDL_STATIC_LIB USING_GENERATED_CONFIG_H RMLUI_STATIC_LIB
APP_DEFINITIONS += PROSPEROLIGHT_LAN_TELEMETRY=$(LAN_TELEMETRY)
APP_INCLUDE_PATHS ?= vendor/ps5/sdl/include vendor/ps5/rmlui/include include src src/gamestream platform/ps5 third_party/moonlight-common-c/src third_party/moonlight-common-c/enet/include third_party/moonlight-common-c/nanors third_party/moonlight-common-c/nanors/deps third_party/moonlight-common-c/nanors/deps/obl third_party/mbedtls/include third_party/opus/include
APP_STATIC_ARCHIVES ?= vendor/ps5/sdl/lib/libSDL2.a vendor/ps5/rmlui/lib/librmlui.a vendor/ps5/freetype/lib/libfreetype.a build/stream-deps/libmoonlight-common-c.a build/stream-deps/libopus.a build/stream-deps/libmbedtls.a build/stream-deps/libmbedx509.a build/stream-deps/libmbedcrypto.a vendor/ps5/sdk/lib/libunwind.a vendor/ps5/sdk/lib/libcxx.a vendor/ps5/sdk/lib/libcxxabi.a
APP_RUNTIME_MODULES ?=
PACBREW_PACKAGES ?=
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
PS5_HOST ?=
FTP_PORT ?= 2121
DEPLOY_FORMAT ?= folder
PS5_FTP_USER ?= anonymous
PS5_FTP_PASSWORD ?= codex
DEPLOY_DRY_RUN ?= 0
TITLE_ID ?=
APP_NAME ?=
APP_CATEGORY ?= game
CONTENT_SUFFIX ?=
HOST_CXX ?= clang++
HOST_TEST_CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
	-ffunction-sections -fdata-sections
HOST_TEST_LDFLAGS ?= -Wl,--gc-sections
GTEST_ARGS ?=
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_RUNTIME_MODULES
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export PS5_HOST FTP_PORT DEPLOY_FORMAT PS5_FTP_USER PS5_FTP_PASSWORD DEPLOY_DRY_RUN
export TITLE_ID APP_NAME APP_CATEGORY CONTENT_SUFFIX

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)
HOST_UNIT_TEST := build/tests/prosperolight_tests
HOST_RUNTIME_TEST := build/tests/cpp_runtime_tests
STREAM_ARCHIVES := build/stream-deps/libmoonlight-common-c.a \
	build/stream-deps/libopus.a build/stream-deps/libmbedtls.a \
	build/stream-deps/libmbedx509.a build/stream-deps/libmbedcrypto.a
STREAM_INPUTS := tools/build-stream-deps.sh \
	$(wildcard src/gamestream/* platform/ps5/*) \
	$(wildcard third_party/moonlight-common-c/src/* third_party/moonlight-common-c/enet/*) \
	$(wildcard third_party/mbedtls/library/* third_party/mbedtls/include/mbedtls/*) \
	$(wildcard third_party/opus/src/* third_party/opus/include/*)

.PHONY: all app build init doctor test test-deps test-unit test-integration libc deps pacbrew pacbrew-list stream-deps assets-check format format-check tidy lint check ffpkg ffpfsc packages deploy undeploy clean distclean help

all: app
build: app

init:
	@printf '%s\n' '==> [init] Configuring the application identity in sce_sys/param.json'
	@bash tools/init-project.sh sce_sys/param.json

doctor:
	@printf '%s\n' '==> [doctor] Checking the Linux/WSL host without changing it'
	@bash tools/doctor.sh

test: test-unit test-integration

test-deps:
	@printf '%s\n' '==> [test-deps] Fetching the pinned host-only GoogleTest source'
	@bash tools/setup-test-dependencies.sh >/dev/null

test-unit: $(HOST_UNIT_TEST) $(HOST_RUNTIME_TEST)
	@printf '%s\n' '==> [test-unit] Running host-native GoogleTest application tests'
	@$(HOST_UNIT_TEST) $(GTEST_ARGS)
	@printf '%s\n' '==> [test-unit] Running C++ allocation runtime tests'
	@$(HOST_RUNTIME_TEST)

$(HOST_UNIT_TEST): tests/test_prosperolight.cpp include/moonlight_config.hpp \
		include/moonlight_health.hpp \
		include/moonlight_stream_input.hpp src/moonlight_config.cpp \
		include/lan_http_report.hpp src/lan_http_report.cpp \
		tools/setup-test-dependencies.sh | test-deps
	@printf '%s\n' '==> [test-unit] Compiling the host-native GoogleTest binary'
	@mkdir -p -- $(@D)
	@gtest=$$(bash tools/setup-test-dependencies.sh); \
		$(HOST_CXX) -std=c++20 -O2 -pthread \
			-isystem "$$gtest/googletest/include" -I"$$gtest/googletest" \
			-c "$$gtest/googletest/src/gtest-all.cc" -o $(@D)/gtest-all.o; \
		$(HOST_CXX) -std=c++20 -O2 -pthread \
			-isystem "$$gtest/googletest/include" -I"$$gtest/googletest" \
			-c "$$gtest/googletest/src/gtest_main.cc" -o $(@D)/gtest-main.o; \
		$(HOST_CXX) $(HOST_TEST_CXXFLAGS) -pthread -Iinclude \
			-isystem "$$gtest/googletest/include" \
			tests/test_prosperolight.cpp src/moonlight_config.cpp src/lan_http_report.cpp \
			$(@D)/gtest-all.o $(@D)/gtest-main.o \
			$(HOST_TEST_LDFLAGS) -o $@

$(HOST_RUNTIME_TEST): tests/test_cpp_runtime.cpp tooling/native/app_cpp_runtime.cpp
	@printf '%s\n' '==> [test-unit] Compiling the C++ allocation runtime test binary'
	@mkdir -p -- $(@D)
	@$(HOST_CXX) $(HOST_TEST_CXXFLAGS) -fno-exceptions -fno-rtti \
		tests/test_cpp_runtime.cpp tooling/native/app_cpp_runtime.cpp \
		$(HOST_TEST_LDFLAGS) -o $@

test-integration:
	@printf '%s\n' '==> [test-integration] Running host tooling integration tests'
	@python3 -m unittest discover -s tests -p 'test_*.py' -v

deps: test-deps
	@printf '%s\n' '==> [deps] Fetching declared native dependencies'
	@bash tools/setup-native-dependencies.sh
	@bash tools/setup-pacbrew-dependencies.sh --environment

pacbrew:
	@printf '%s\n' '==> [pacbrew] Fetching the pinned prebuilt ports sysroot'
	@bash tools/setup-pacbrew-dependencies.sh --all

pacbrew-list:
	@printf '%s\n' '==> [pacbrew] Listing available pkg-config modules'
	@bash tools/setup-pacbrew-dependencies.sh --list

assets-check:
	@printf '%s\n' '==> [assets] Validating icon, backgrounds, and selection audio'
	@bash tools/validate-assets.sh

libc:
	@printf '%s\n' '==> [libc] Rebuilding and verifying the clean-room runtime'
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

app: $(RUNTIME) $(STREAM_ARCHIVES)
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder

stream-deps: $(STREAM_ARCHIVES)

$(STREAM_ARCHIVES): $(STREAM_INPUTS)
	@printf '%s\n' '==> [stream] Building pinned Moonlight, mbedTLS, and Opus archives'
	@bash tools/build-stream-deps.sh

ffpkg: $(RUNTIME) $(STREAM_ARCHIVES)
	@printf '%s\n' '==> [ffpkg] Building the app folder and UFS2 image'
	@bash tools/build.sh Ffpkg

ffpfsc: $(RUNTIME) $(STREAM_ARCHIVES)
	@printf '%s\n' '==> [ffpfsc] Building the app folder and compressed image'
	@bash tools/build.sh Ffpfsc

packages: $(RUNTIME) $(STREAM_ARCHIVES)
	@printf '%s\n' '==> [packages] Building the app folder and both package formats'
	@bash tools/build.sh All

deploy:
	@printf '%s\n' '==> [deploy] Building and publishing the selected app output over FTP'
	@bash tools/deploy.sh

undeploy:
	@printf '%s\n' '==> [undeploy] Removing staged development files for this title over FTP'
	@bash tools/deploy.sh undeploy

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

check: lint test app

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
	  'make init TITLE_ID=PPSA12345 APP_NAME="My App"  Configure app identity' \
	  'make doctor          Check required and optional Linux/WSL tools' \
	  'make test            Run all host unit and integration tests' \
	  'make test-deps       Fetch verified host-only GoogleTest source' \
	  'make test-unit       Run host-native GoogleTest application tests' \
	  'make test-integration  Run host tooling integration tests' \
	  'make deps            Fetch native dependencies into .deps/' \
	  'make pacbrew         Fetch the pinned PacBrew ports sysroot' \
	  'make pacbrew-list    List PacBrew pkg-config module names' \
	  'make assets-check    Validate the current presentation assets' \
	  'make libc            Force a deterministic runtime/libc.prx rebuild' \
	  'make format          Apply the shared Clang formatting policy' \
	  'make format-check    Check formatting without modifying files' \
	  'make tidy            Run the shared Clang static-analysis policy' \
	  'make lint            Run format, tidy, metadata, and shell checks' \
	  'make check           Run lint and build the skeleton app' \
	  'make ffpkg           Build the folder and UFS2 .ffpkg image' \
	  'make ffpfsc          Build the folder and compressed .ffpfsc image' \
	  'make packages        Build folder, .ffpkg, and .ffpfsc outputs' \
	  'make deploy PS5_HOST=<address>  Build and FTP-deploy the app folder' \
	  'make undeploy PS5_HOST=<address>  Remove this title from /data/homebrew' \
	  'Build variables:     APP_DEFINITIONS, APP_INCLUDE_PATHS, APP_STATIC_ARCHIVES, APP_RUNTIME_MODULES' \
	  'PacBrew variables:   PACBREW_PACKAGES, PACBREW_INCLUDE_PATHS, PACBREW_STATIC_ARCHIVES' \
	  'Diagnostics:         LAN_TELEMETRY=1 enables the optional port-8767 development sink' \
	  'Deploy variables:    FTP_PORT=2121, DEPLOY_FORMAT=folder|ffpfsc|ffpkg, DEPLOY_DRY_RUN=0|1' \
	  'Local defaults:      Copy .env.example to the ignored .env file' \
	  'make clean           Remove build/, dist/, and generated libc.prx' \
	  'make distclean       Also remove the ignored .deps/ cache'

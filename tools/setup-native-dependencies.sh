#!/usr/bin/env bash
# ps5-native-app-boilerplate - Linux/WSL native dependency bootstrapper.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fetches the public PS5 payload SDK and static zlib into the ignored cache.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cache="$root/.deps/native"
sdk="$cache/ps5-payload-sdk"
zlib_root="$cache/zlib/root"
sdk_url="https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip"
sdk_hash="8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da"
skip_sdk=false

if [[ ${1:-} == "--skip-sdk" ]]; then
    skip_sdk=true
elif [[ $# -ne 0 ]]; then
    echo "usage: tools/setup-native-dependencies.sh [--skip-sdk]" >&2
    exit 2
fi

for command in wget unzip sha256sum apt-get dpkg-deb; do
    command -v "$command" >/dev/null || {
        echo "missing required command: $command" >&2
        exit 2
    }
done

mkdir -p "$cache"
if ! $skip_sdk && [[ ! -x "$sdk/bin/prospero-lld" ]]; then
    archive="$cache/ps5-payload-sdk.zip"
    temporary="$archive.download"
    wget -q "$sdk_url" -O "$temporary"
    printf '%s  %s\n' "$sdk_hash" "$temporary" | sha256sum --check --strict
    mv "$temporary" "$archive"
    unzip -q -o "$archive" -d "$cache"
fi
if ! $skip_sdk && [[ ! -x "$sdk/bin/prospero-lld" || ! -d "$sdk/target/include" ]]; then
    echo "the pinned PS5 payload SDK is incomplete" >&2
    exit 2
fi

zlib_archive=$(find "$zlib_root" -type f -name libz.a -print -quit 2>/dev/null || true)
if [[ -z $zlib_archive || ! -f "$zlib_root/usr/include/zlib.h" ]]; then
    mkdir -p "${zlib_root%/root}"
    pushd "${zlib_root%/root}" >/dev/null
    apt-get download zlib1g-dev >/dev/null
    for package in zlib1g-dev_*.deb; do
        dpkg-deb -x "$package" root
    done
    popd >/dev/null
    zlib_archive=$(find "$zlib_root" -type f -name libz.a -print -quit 2>/dev/null || true)
fi
if [[ -z $zlib_archive ]]; then
    echo "the native zlib archive was not found after extraction" >&2
    exit 2
fi

printf 'SDK_ROOT=%s\nZLIB_INCLUDE=%s\nZLIB_ARCHIVE=%s\n' \
    "$sdk" "$zlib_root/usr/include" "$zlib_archive"

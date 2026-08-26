#!/usr/bin/env bash
# ps5-native-app-boilerplate - Linux/WSL package-tool bootstrapper.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fetches makefs or MkPFS into the ignored cache without a global install.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kind=${1:-}

case "$kind" in
    ffpkg)
        for command in apt-get dpkg-deb; do
            command -v "$command" >/dev/null || {
                echo "missing required command: $command" >&2
                exit 2
            }
        done
        cache="$root/.deps/makefs"
        binary="$cache/root/usr/sbin/makefs"
        if [[ ! -x $binary ]]; then
            mkdir -p "$cache"
            pushd "$cache" >/dev/null
            apt-get download makefs >/dev/null
            for package in makefs_*.deb; do
                dpkg-deb -x "$package" root
            done
            popd >/dev/null
        fi
        [[ -x $binary ]] || { echo "makefs bootstrap failed" >&2; exit 2; }
        printf '%s\n' "$binary"
        ;;
    ffpfsc)
        for command in git python3; do
            command -v "$command" >/dev/null || {
                echo "missing required command: $command" >&2
                exit 2
            }
        done
        checkout="$root/.deps/MkPFS"
        revision="6cb8313dfe0c988ac52617794553f343243d3a56"
        if [[ ! -d $checkout/.git ]]; then
            mkdir -p "$checkout"
            git -C "$checkout" init --quiet
            git -C "$checkout" remote add origin https://github.com/PSBrew/MkPFS.git
            git -C "$checkout" fetch --quiet --depth 1 origin "$revision"
            git -C "$checkout" checkout --quiet --detach FETCH_HEAD
        fi
        actual=$(git -C "$checkout" rev-parse HEAD)
        [[ $actual == "$revision" ]] || {
            echo "MkPFS cache is at $actual; expected $revision" >&2
            exit 2
        }
        git -C "$checkout" diff --quiet || {
            echo "MkPFS cache has local changes" >&2
            exit 2
        }
        packages="$checkout/.python-packages"
        stamp="$packages/.boilerplate-revision"
        if [[ ! -f $stamp || $(<"$stamp") != "$revision" ]]; then
            rm -rf -- "$packages"
            mkdir -p "$packages"
            python3 -m pip install --disable-pip-version-check --quiet \
                --target "$packages" "$checkout" >&2
            printf '%s\n' "$revision" > "$stamp"
        fi
        runner="$checkout/.mkpfs-run"
        printf '#!/bin/sh\nPYTHONPATH=%q exec python3 -m mkpfs "$@"\n' \
            "$packages" > "$runner"
        chmod +x "$runner"
        "$runner" --help >/dev/null
        printf '%s\n' "$runner"
        ;;
    *)
        echo "usage: tools/setup-packaging-dependencies.sh <ffpkg|ffpfsc>" >&2
        exit 2
        ;;
esac

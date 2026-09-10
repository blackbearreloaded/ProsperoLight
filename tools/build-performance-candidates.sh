#!/usr/bin/env bash
# ps5-native-app-boilerplate / ProsperoLight - Freeze offline A/B folders, never deploy.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
# A parent make exports its already-expanded definitions. Each child must derive
# these once from its own options, not inherit competing macro definitions.
unset APP_DEFINITIONS
mkdir -p results
out=$(mktemp -d "$root/results/performance-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
title=$(python3 -c 'import json; print(json.load(open("sce_sys/param.json"))["titleId"])')
[[ "$title" =~ ^PPSA[0-9]{5}$ ]] || exit 2
git rev-parse HEAD > "$out/base-commit.txt"
git diff --binary > "$out/tracked-changes.patch"
git ls-files --others --exclude-standard -z -- include src platform tools tests docs .github \
    | tar --null -T - -cf "$out/new-source-files.tar"
printf 'Candidate folders: %s\n' "$out"
# Group compilation by dependency options only to avoid needless rebuilds.
# Hardware comparisons still follow baseline -> FEC -> Opus/audio -> overlap/poll.
while read -r name fec opus audio overlap poll; do
    printf 'Building %s\n' "$name"
    make app FEC_SIMD="$fec" OPUS_SIMD="$opus" AUDIO_MAX_BACKLOG_MS="$audio" \
        PRESENT_OVERLAP="$overlap" FLIP_POLL_US="$poll" LAN_TELEMETRY=0 \
        STREAM_SELF_TEST_FPS=0 STREAM_SELF_TEST_RESOLUTION=0 \
        VIDEO_OUTPUT_SELF_TEST_FPS=0 STOP_ACTIVE_APP_SELF_TEST=0 > "$out/$name-build.log" 2>&1
    mkdir "$out/$name"
    cp -a "dist/$title" "$out/$name/"
    printf 'FEC_SIMD=%s OPUS_SIMD=%s AUDIO_MAX_BACKLOG_MS=%s PRESENT_OVERLAP=%s FLIP_POLL_US=%s\n' \
        "$fec" "$opus" "$audio" "$overlap" "$poll" > "$out/$name/options.txt"
    (cd "$out/$name" && find "$title" -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
    printf '%s PASS\n' "$name"
done <<'CANDIDATES'
00-baseline 0 0 0 0 500
03-audio-catchup 0 0 30 0 500
04-presentation-overlap 0 0 0 1 500
05-flip-poll-200us 0 0 0 0 200
01-fec-simd 1 0 0 0 500
02-opus-simd 0 1 0 0 500
06-combined 1 1 30 1 500
CANDIDATES
# The build always recreates the folder. Finish with default options so a later
# developer's ordinary build/upload cannot accidentally use the combined trial.
make app FEC_SIMD=0 OPUS_SIMD=0 AUDIO_MAX_BACKLOG_MS=0 PRESENT_OVERLAP=1 FLIP_POLL_US=500 \
    LAN_TELEMETRY=0 STREAM_SELF_TEST_FPS=0 STREAM_SELF_TEST_RESOLUTION=0 \
    VIDEO_OUTPUT_SELF_TEST_FPS=0 STOP_ACTIVE_APP_SELF_TEST=0 > "$out/restored-release-build.log" 2>&1
cmp "dist/$title/eboot.bin" "$out/04-presentation-overlap/$title/eboot.bin"
printf 'All offline candidates built; release overlap defaults restored. No console contacted.\n%s\n' "$out"

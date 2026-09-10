#!/usr/bin/env bash
# ps5-native-app-boilerplate / ProsperoLight - Offline streaming dependency checks.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
out="$root/build/tests/stream-performance"
mkdir -p "$out"
common="$root/third_party/moonlight-common-c"
cc=${HOST_CC:-clang}
cxx=${HOST_CXX:-clang++}
for simd in 0 1; do
    variant="$out/$simd"
    mkdir -p "$variant"
    flags=(-O2 -D_POSIX_C_SOURCE=200809L "-I$common/nanors" "-I$common/nanors/deps/obl" "-I$common/nanors/deps"
        "-I$root/platform/ps5" "-DPROSPEROLIGHT_FEC_SIMD=$simd"
        -include "$root/platform/ps5/ps5_fec_cpu.h")
    for file in rs deps/obl/oblas_common deps/obl/oblas_lite; do
        "$cc" -std=c11 "${flags[@]}" -c "$common/nanors/$file.c" -o "$variant/$(basename "$file").o"
    done
    "$cxx" -std=c++20 "${flags[@]}" "$root/tests/test_fec_dispatch.cpp" "$variant"/*.o -o "$variant/fec"
    "$variant/fec" | tee "$variant/fec.txt"
    disabled=ON
    [[ "$simd" == 0 ]] || disabled=OFF
    cmake -S "$root/third_party/opus" -B "$variant/opus" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$cc" \
        -DOPUS_BUILD_SHARED_LIBRARY=OFF -DOPUS_BUILD_TESTING=ON -DOPUS_BUILD_PROGRAMS=OFF \
        -DOPUS_DISABLE_INTRINSICS="$disabled" -DOPUS_X86_MAY_HAVE_AVX2=OFF \
        -DOPUS_X86_PRESUME_AVX2=OFF -DOPUS_X86_PRESUME_SSE4_1=OFF > "$variant/opus-config.log"
    cmake --build "$variant/opus" --parallel 4 > "$variant/opus-build.log" 2>&1
    ctest --test-dir "$variant/opus" --output-on-failure > "$variant/opus-tests.log" 2>&1
    "$cxx" -std=c++20 -O2 -I"$root/third_party/opus/include" \
        "$root/tests/test_opus_pcm.cpp" "$variant/opus/libopus.a" -lm -o "$variant/pcm"
done
diff <(tail -1 "$out/0/fec.txt") <(tail -1 "$out/1/fec.txt")
for channels in 2 6; do
    corpus="$out/$channels.packets"
    "$out/0/pcm" encode "$channels" "$corpus" unused
    for simd in 0 1; do
        "$out/$simd/pcm" decode "$channels" "$corpus" "$out/$simd/$channels.pcm"
    done
done
python3 - "$out" <<'PY'
import array
import math
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
for channels in (2, 6):
    decoded = []
    for simd in (0, 1):
        pcm = array.array('h')
        pcm.frombytes((root / str(simd) / f'{channels}.pcm').read_bytes())
        decoded.append(pcm)
    a, b = decoded
    assert len(a) == len(b) == 150 * (240 + 480) * channels
    delta = max(abs(x - y) for x, y in zip(a, b))
    error_energy = sum((x - y) ** 2 for x, y in zip(a, b))
    snr = 10 * math.log10(sum(x * x for x in a) / max(1, error_energy))
    # Floating-point Opus paths are not bit-exact. Bound both isolated error
    # (<0.1% of signed 16-bit full scale) and total signal-to-difference energy.
    assert delta <= 32 and snr >= 75, f'Scalar/SIMD PCM diverged: {delta} LSB, {snr} dB'
    for c in range(channels):
        assert max(abs(x) for x in b[c::channels]) > 1000, f'Silent channel {c}'
    print(f'Opus channels={channels} samples={len(a)} max_delta={delta} difference_snr_db={snr:.2f} PASS')
PY
printf 'Offline stream dependency checks PASS (host only; not PS5 performance evidence)\n'

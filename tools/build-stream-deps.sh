#!/usr/bin/env bash
# ps5-native-app-boilerplate / ProsperoLight - Pinned native streaming dependency build.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the C ABI dependencies used by the modern C++ application layer.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
bash "$root/tools/setup-native-dependencies.sh" >/dev/null
sdk=${PS5_PAYLOAD_SDK:-$root/.deps/native/ps5-payload-sdk}
cc="$sdk/bin/prospero-clang"
ar="$sdk/bin/llvm-ar"
output="$root/build/stream-deps"
common="$root/third_party/moonlight-common-c"
mbedtls="$root/third_party/mbedtls"
opus="$root/third_party/opus"
mbedtls_build="$output/mbedtls"
opus_build="$output/opus"

[[ -x "$cc" && -x "$ar" ]] || { echo "PS5 payload SDK is unavailable" >&2; exit 1; }
[[ -f "$common/src/Limelight.h" && -f "$mbedtls/Makefile" && \
   -f "$opus/CMakeLists.txt" ]] || {
    echo "Run: git submodule update --init --recursive" >&2
    exit 1
}
[[ "$output" == "$root/build/stream-deps" ]] || exit 1
rm -rf -- "$output"
mkdir -p "$output/obj"
cp -a -- "$mbedtls" "$mbedtls_build"

config='-DMBEDTLS_CONFIG_FILE="mbedtls_ps5_config.h"'
make_config='-DMBEDTLS_CONFIG_FILE=\"mbedtls_ps5_config.h\"'
make -C "$mbedtls_build" clean >/dev/null
make -C "$mbedtls_build" lib \
    CC="$cc" AR="$ar" \
    CFLAGS="-O2 -ffunction-sections -fdata-sections $make_config -I$root/platform/ps5" \
    WARNING_CFLAGS='-Wall -Wextra -Wno-unused-parameter' >/dev/null
cp "$mbedtls_build/library/libmbedcrypto.a" "$output/libmbedcrypto.a"
cp "$mbedtls_build/library/libmbedx509.a" "$output/libmbedx509.a"
cp "$mbedtls_build/library/libmbedtls.a" "$output/libmbedtls.a"

cmake -S "$opus" -B "$opus_build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$cc" \
    -DCMAKE_AR="$ar" \
    -DCMAKE_RANLIB="$sdk/bin/llvm-ranlib" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_C_FLAGS_RELEASE='-O2 -DNDEBUG -ffunction-sections -fdata-sections' \
    -DOPUS_BUILD_SHARED_LIBRARY=OFF \
    -DOPUS_BUILD_TESTING=OFF \
    -DOPUS_BUILD_PROGRAMS=OFF \
    -DOPUS_DISABLE_INTRINSICS=ON \
    -DOPUS_STACK_PROTECTOR=OFF \
    -DOPUS_FORTIFY_SOURCE=OFF >/dev/null
cmake --build "$opus_build" --target opus --parallel >/dev/null
cp "$opus_build/libopus.a" "$output/libopus.a"

includes=(
    "-I$common/src"
    "-I$common/enet/include"
    "-I$common/nanors"
    "-I$common/nanors/deps"
    "-I$common/nanors/deps/obl"
    "-I$mbedtls/include"
    "-I$root/platform/ps5"
    "-I$root/src/gamestream"
)
flags=(
    -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Werror
    -ffunction-sections -fdata-sections
    -DHAS_SOCKLEN_T -DNO_MSGAPI -DNDEBUG -DUSE_PSA_CRYPTO
    "$config"
    -include "$root/platform/ps5/ps5_compat.h"
)
sources=(
    "$common"/src/*.c
    "$common"/enet/*.c
    "$common/nanors/rs.c"
    "$common/nanors/deps/obl/oblas_common.c"
    "$common/nanors/deps/obl/oblas_lite.c"
    "$root"/src/gamestream/*.c
    "$root/platform/ps5/ps5_entropy.c"
    "$root/platform/ps5/ps5_sockets.c"
)
objects=()
index=0
for source in "${sources[@]}"; do
    object="$output/obj/$(printf '%03d' "$index")-$(basename "${source%.c}").o"
    source_flags=()
    if [[ "$source" == "$root/platform/ps5/ps5_sockets.c" ]]; then
        source_flags=(-DPS5_SOCKET_ADAPTER_IMPLEMENTATION)
    elif [[ "$source" == "$common/nanors/deps/obl/oblas_lite.c" ]]; then
        # Clang 18 hides these declarations for the PS5 target. The upstream
        # functions remain target-attributed and runtime-dispatched.
        source_flags=(
            -D__AVX512F__ -D__AVX512BW__ -D__AVX512DQ__
            -D__AVX512VL__ -D__GFNI__
        )
    fi
    "$cc" "${flags[@]}" "${source_flags[@]}" "${includes[@]}" -c "$source" -o "$object"
    objects+=("$object")
    index=$((index + 1))
done

"$ar" rcs "$output/libmoonlight-common-c.a" "${objects[@]}"
printf 'Built %s (%d objects)\n' "$output/libmoonlight-common-c.a" "${#objects[@]}"
printf 'Built %s\n' "$output/libmbedtls.a"
printf 'Built %s\n' "$output/libmbedx509.a"
printf 'Built %s\n' "$output/libmbedcrypto.a"
printf 'Built %s\n' "$output/libopus.a"

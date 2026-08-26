#!/usr/bin/env bash
# ps5-native-app-boilerplate - Native Linux/WSL application build.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compiles, links, signs, validates, and assembles the root skeleton app.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
format=${1:-Folder}
format=${format,,}
case "$format" in folder|ffpkg|ffpfsc|all) ;; *)
    echo "usage: tools/build.sh [Folder|Ffpkg|Ffpfsc|All]" >&2
    exit 2
esac

for command in python3 sha256sum; do
    command -v "$command" >/dev/null || {
        echo "missing required command: $command" >&2
        exit 2
    }
done
bash "$root/tools/setup-native-dependencies.sh" >/dev/null

project="$root/project.json"
json_scalar() {
    python3 - "$project" "$1" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as source:
    value = json.load(source)[sys.argv[2]]
if isinstance(value, (dict, list)):
    raise SystemExit(f"{sys.argv[2]} must be a scalar")
print(value)
PY
}
json_array() {
    python3 - "$project" "$1" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as source:
    values = json.load(source)[sys.argv[2]]
if not isinstance(values, list):
    raise SystemExit(f"{sys.argv[2]} must be an array")
for value in values:
    print(value)
PY
}

title_name=$(json_scalar titleName)
title_id=$(json_scalar titleId)
concept_id=$(json_scalar conceptId)
content_id=$(json_scalar contentId)
module_sdk=$(json_scalar moduleSdkVersion)
companion_sdk=$(json_scalar companionSdkVersion)
fself_magic=$(json_scalar fselfMagic)
download_size=$(json_scalar downloadDataSize)
[[ $title_id =~ ^PPSA[0-9]{5}$ ]] || { echo "invalid titleId" >&2; exit 2; }
[[ $concept_id =~ ^[0-9]{5}$ ]] || { echo "invalid conceptId" >&2; exit 2; }
[[ $content_id =~ ^[A-Z]{2}[0-9]{4}-PPSA[0-9]{5}_00-[A-Z0-9]{16}$ && $content_id == *"$title_id"* ]] || {
    echo "invalid contentId" >&2; exit 2;
}
[[ $module_sdk =~ ^0x[0-9A-Fa-f]{8}$ && $companion_sdk =~ ^0x[0-9A-Fa-f]{8}$ ]] || {
    echo "invalid SDK version" >&2; exit 2;
}
[[ $fself_magic == 0x1D3D154F || $fself_magic == 0xEEF51454 ]] || {
    echo "unsupported FSELF magic" >&2; exit 2;
}
[[ $download_size =~ ^[0-9]+$ && -n $title_name ]] || {
    echo "invalid application metadata" >&2; exit 2;
}

python3 - "$root/sce_sys" <<'PY'
from pathlib import Path
import struct, sys

root = Path(sys.argv[1])
icon = (root / "icon0.png").read_bytes()
if icon[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", icon[16:24]) != (512, 512):
    raise SystemExit("sce_sys/icon0.png must be a 512x512 PNG")

pics = [root / "pic0.dds", root / "pic1.dds"]
if pics[0].exists() != pics[1].exists():
    raise SystemExit("supply both pic0.dds and pic1.dds, or neither")
for path in pics if pics[0].exists() else []:
    data = path.read_bytes()
    if len(data) < 148 or data[:4] != b"DDS ":
        raise SystemExit(f"{path} is not a DDS file")
    height, width = struct.unpack_from("<II", data, 12)
    mipmaps = struct.unpack_from("<I", data, 28)[0]
    expected = 148 + width // 4 * (height // 4) * 16
    if (width, height, mipmaps, data[84:88], struct.unpack_from("<I", data, 128)[0], len(data)) != (3840, 2160, 1, b"DX10", 98, expected):
        raise SystemExit(f"{path} is not the supported 4K BC7 profile")

sound = root / "snd0.at9"
if sound.exists():
    data = sound.read_bytes()
    if len(data) < 128 or data[:4] != b"RIFF" or data[8:12] != b"WAVE" or struct.unpack_from("<I", data, 4)[0] + 8 != len(data) or len(data) > 360616:
        raise SystemExit("sce_sys/snd0.at9 is not the supported selection-audio profile")
PY

sdk_root="$root/.deps/native/ps5-payload-sdk"
zlib_root="$root/.deps/native/zlib/root"
zlib_archive=$(find "$zlib_root" -type f -name libz.a -print -quit)
cxx=${CXX:-}
if [[ -z $cxx ]]; then
    cxx=$(command -v clang++-18 || command -v clang++)
fi
[[ -n $cxx ]] || { echo "Clang++ was not found" >&2; exit 2; }

build="$root/build"
dist="$root/dist"
native="$root/tooling/native"
tool="$build/host/ps5-native-tool"
mkdir -p "$build/host" "$build/obj" "$dist"
"$cxx" -std=c++20 -O2 -Wall -Wextra -Werror \
    -I "$zlib_root/usr/include" \
    "$native/native_app_builder.cpp" "$native/self_container.cpp" \
    "$native/elf_object.cpp" "$native/sce_module_writer.cpp" \
    "$zlib_archive" -o "$tool"

mapfile -t sources < <(json_array sources)
mapfile -t definitions < <(json_array compileDefinitions)
mapfile -t includes < <(json_array includePaths)
mapfile -t archives < <(json_array staticArchives)
(( ${#sources[@]} > 0 )) || { echo "project has no sources" >&2; exit 2; }

objects=()
for source in "${sources[@]}"; do
    [[ $source =~ ^src/[A-Za-z0-9_./-]+\.(c|cc|cpp)$ && -f $root/$source ]] || {
        echo "invalid source: $source" >&2; exit 2;
    }
    object="$build/obj/${source//\//_}.o"
    if [[ $source == *.c ]]; then standard=-std=c11; else standard=-std=c++20; fi
    args=("$standard" -O2 -Wall -Wextra -ffunction-sections -fdata-sections)
    [[ $source == *.c ]] || args+=(-fno-exceptions -fno-rtti)
    for definition in "${definitions[@]}"; do
        [[ $definition =~ ^[A-Za-z_][A-Za-z0-9_]*(=[A-Za-z0-9_]+)?$ ]] || {
            echo "invalid compile definition: $definition" >&2; exit 2;
        }
        args+=("-D$definition")
    done
    for include in "${includes[@]}"; do
        [[ $include =~ ^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$ && -d $root/$include ]] || {
            echo "invalid include path: $include" >&2; exit 2;
        }
        args+=("-I$root/$include")
    done
    PS5_PAYLOAD_SDK="$sdk_root" sh "$root/tooling/prospero-clang18" \
        "${args[@]}" -c "$root/$source" -o "$object"
    objects+=("$object")
done

PS5_PAYLOAD_SDK="$sdk_root" sh "$root/tooling/prospero-clang18" \
    -std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections \
    -c "$native/app_crt.c" -o "$build/obj/app_crt.o"

PS5_PAYLOAD_SDK="$sdk_root" sh "$root/tooling/prospero-clang18" \
    -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti \
    -ffunction-sections -fdata-sections \
    -c "$native/app_cpp_runtime.cpp" -o "$build/obj/app_cpp_runtime.o"

link_inputs=("$build/obj/app_crt.o" "$build/obj/app_cpp_runtime.o" "${objects[@]}")
for archive in "${archives[@]}"; do
    [[ $archive =~ ^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*\.a$ && -f $root/$archive ]] || {
        echo "invalid static archive: $archive" >&2; exit 2;
    }
    link_inputs+=("$root/$archive")
done
"$sdk_root/bin/prospero-lld" -T "$native/ps5-pie.ld" --eh-frame-hdr \
    --version-script "$native/app-symbols.map" \
    -e _start -o "$build/llvm-pie.elf" "${link_inputs[@]}" \
    --as-needed "$sdk_root"/target/lib/*.so
"$tool" link --in "$build/llvm-pie.elf" --out "$build/eboot.elf" \
    --stub-dir "$sdk_root/target/lib" --module-sdk "$module_sdk" \
    --companion-sdk "$companion_sdk" --file-name eboot.elf

app="$dist/$title_id"
rm -rf -- "$app"
mkdir -p "$app/sce_sys" "$app/sce_module"
"$tool" self --sign --in "$build/eboot.elf" --out "$app/eboot.bin" \
    --magic "$fself_magic"

python3 - "$root/sce_sys/param.json" "$app/sce_sys/param.json" \
    "$title_name" "$title_id" "$concept_id" "$content_id" "$download_size" <<'PY'
import json, sys
source, output, title, title_id, concept_id, content_id, size = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    value = json.load(stream)
value["titleId"] = title_id
value["conceptId"] = concept_id
value["contentId"] = content_id
value["downloadDataSize"] = int(size)
value["localizedParameters"]["en-US"]["titleName"] = title
with open(output, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(value, stream, indent=2)
    stream.write("\n")
PY
for asset in icon0.png pic0.dds pic1.dds snd0.at9; do
    [[ -f $root/sce_sys/$asset ]] && cp "$root/sce_sys/$asset" "$app/sce_sys/$asset"
done
[[ ! -d $root/assets ]] || cp -a "$root/assets" "$app/assets"

python3 - "$project" <<'PY' > "$build/runtime-modules.tsv"
import json, sys
with open(sys.argv[1], encoding="utf-8") as source:
    modules = json.load(source)["runtimeModules"]
for module in modules:
    print(module["source"], module["name"], module.get("sha256", ""), sep="\t")
PY
while IFS=$'\t' read -r source name expected; do
    [[ $source =~ ^(runtime|\.local/runtime)/[A-Za-z0-9._-]+\.prx$ && $name =~ ^[A-Za-z0-9._-]+\.prx$ ]] || {
        echo "invalid runtime module: $source" >&2; exit 2;
    }
    input="$root/$source"
    [[ -f $input ]] || { echo "missing runtime module: $source" >&2; exit 2; }
    actual=$(sha256sum "$input" | cut -d ' ' -f 1)
    [[ -z $expected || ${actual^^} == ${expected^^} ]] || {
        echo "runtime hash mismatch: $source" >&2; exit 2;
    }
    magic=$(python3 - "$input" <<'PY'
import struct, sys
with open(sys.argv[1], "rb") as stream:
    print(f"{struct.unpack('<I', stream.read(4))[0]:08x}")
PY
)
    if [[ $magic == 1d3d154f || $magic == eef51454 ]]; then
        cp "$input" "$app/sce_module/$name"
    else
        "$tool" self --sign --in "$input" --out "$app/sce_module/$name"
    fi
    "$tool" self --inspect --file "$app/sce_module/$name"
done < "$build/runtime-modules.tsv"
"$tool" self --inspect --file "$app/eboot.bin"

if [[ $format == ffpkg || $format == all ]]; then
    makefs=$(bash "$root/tools/setup-packaging-dependencies.sh" ffpkg)
    rm -f -- "$dist/$title_id.ffpkg"
    "$makefs" -S 4096 -b 20% -t ffs \
        -o version=2,bsize=32768,fsize=4096,minfree=0,optimization=space \
        "$dist/$title_id.ffpkg" "$app"
    python3 - "$dist/$title_id.ffpkg" <<'PY'
import struct, sys
with open(sys.argv[1], "rb") as stream:
    stream.seek(0x255c)
    if struct.unpack("<I", stream.read(4))[0] != 0x19540119:
        raise SystemExit("FFPKG is missing the UFS2 superblock magic")
PY
fi
if [[ $format == ffpfsc || $format == all ]]; then
    mkpfs=$(bash "$root/tools/setup-packaging-dependencies.sh" ffpfsc)
    rm -f -- "$dist/$title_id.ffpfsc"
    "$mkpfs" pack folder --no-adjust-output-file-extension \
        --version PS5 --verify "$app" "$dist/$title_id.ffpfsc"
fi

printf 'Build complete.\nApp folder: %s\n' "$app"
[[ $format != ffpkg && $format != all ]] || printf 'FFPKG:     %s\n' "$dist/$title_id.ffpkg"
[[ $format != ffpfsc && $format != all ]] || printf 'FFPFSC:    %s\n' "$dist/$title_id.ffpfsc"

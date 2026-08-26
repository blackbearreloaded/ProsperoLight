#!/usr/bin/env bash
# ps5-native-app-boilerplate - Linux/WSL source and metadata checks.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the portable checks used by the Make workflow.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
bash tools/setup-native-dependencies.sh >/dev/null
bash tools/run_clang_format.sh --check
bash tools/run_clang_tidy.sh

mapfile -t repository_files < <(git ls-files --cached --others --exclude-standard)
checked=0
for file in "${repository_files[@]}"; do
    [[ -f $file ]] || continue
    case "$file" in
        *.c|*.cc|*.cpp|*.h|*.hpp|*.ld|*.ps1|*.sh|*.yml|*.yaml|Makefile|.clang-format|.clang-tidy)
            header=$(head -n 20 "$file")
            grep -Fq ps5-native-app-boilerplate <<<"$header"
            grep -Fq 'Copyright (C) 2026 BlackBearReloaded' <<<"$header"
            grep -Fq 'SPDX-License-Identifier: GPL-3.0-or-later' <<<"$header"
            ((checked += 1))
            ;;
        *.cs|*.csproj)
            echo "managed source is not allowed: $file" >&2
            exit 2
            ;;
    esac
done

while IFS= read -r script; do
    bash -n "$script"
done < <(find tools -maxdepth 1 -type f -name '*.sh' -print)

python3 - <<'PY'
from pathlib import Path
import json, subprocess
for name in subprocess.check_output(["git", "ls-files", "*.json"], text=True).splitlines():
    if Path(name).is_file():
        with open(name, encoding="utf-8") as source:
            json.load(source)
PY

if git grep -n -E 'C:\\Users\\|/home/denis|/mnt/c/Users/denis|\bDenis\b' -- . \
    ':(exclude)tools/lint.sh'; then
    echo "repository contains a local path or personal-name leak" >&2
    exit 2
fi
git diff --check
printf 'Lint passed: %d attributed code and tooling files.\n' "$checked"

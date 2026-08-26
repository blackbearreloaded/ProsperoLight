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
import json, re, subprocess
for name in subprocess.check_output(["git", "ls-files", "*.json"], text=True).splitlines():
    if Path(name).is_file():
        with open(name, encoding="utf-8") as source:
            json.load(source)

with open("project.json", encoding="utf-8") as source:
    project = json.load(source)
if project.get("applicationCategory") not in {"game", "media"}:
    raise SystemExit("project.json applicationCategory must be game or media")
if not re.fullmatch(r"\d{2}\.\d{3}\.\d{3}", project.get("contentVersion", "")):
    raise SystemExit("project.json contentVersion must use NN.NNN.NNN")
if not re.fullmatch(r"\d{2}\.\d{2}", project.get("masterVersion", "")):
    raise SystemExit("project.json masterVersion must use NN.NN")
patterns = {
    "pacbrewPackages": r"[A-Za-z0-9_.+-]+",
    "pacbrewIncludePaths": r"[A-Za-z0-9_.+-]+(?:/[A-Za-z0-9_.+-]+)*",
    "pacbrewStaticArchives": r"[A-Za-z0-9_.+-]+(?:/[A-Za-z0-9_.+-]+)*\.a",
}
for field, pattern in patterns.items():
    values = project.get(field)
    if not isinstance(values, list) or not all(
            isinstance(value, str) and re.fullmatch(pattern, value) for value in values):
        raise SystemExit(f"project.json {field} contains an invalid value")
PY

if git grep -n -E 'C:\\Users\\|/home/denis|/mnt/c/Users/denis|\bDenis\b' -- . \
    ':(exclude)tools/lint.sh'; then
    echo "repository contains a local path or personal-name leak" >&2
    exit 2
fi
git diff --check
printf 'Lint passed: %d attributed code and tooling files.\n' "$checked"

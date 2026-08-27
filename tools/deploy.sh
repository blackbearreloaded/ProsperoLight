#!/usr/bin/env bash
# ps5-native-app-boilerplate - FTP development deployment and cleanup.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the selected output and publishes it below /data/homebrew, or removes
# only the current title's staged files. Folder files are uploaded under
# ignored temporary names and promoted individually.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
action=${1:-deploy}
format=${DEPLOY_FORMAT:-folder}
format=${format,,}
host=${PS5_HOST:-}
port=${FTP_PORT:-2121}
user=${PS5_FTP_USER:-anonymous}
password=${PS5_FTP_PASSWORD:-codex}

[[ $# -le 1 && ($action == deploy || $action == undeploy) ]] || {
    echo "usage: tools/deploy.sh [undeploy]" >&2
    exit 2
}
[[ $action == undeploy || $format == folder || $format == ffpfsc || $format == ffpkg ]] || {
    echo "DEPLOY_FORMAT must be folder, ffpfsc, or ffpkg" >&2
    exit 2
}
[[ $host =~ ^[A-Za-z0-9][A-Za-z0-9.-]*$ ]] || {
    echo "PS5_HOST must be an IPv4 address or hostname" >&2
    exit 2
}
[[ $port =~ ^[0-9]+$ ]] && (( 10#$port >= 1 && 10#$port <= 65535 )) || {
    echo "FTP_PORT must be between 1 and 65535" >&2
    exit 2
}
command -v make >/dev/null || { echo "missing required command: make" >&2; exit 2; }
command -v python3 >/dev/null || { echo "missing required command: python3" >&2; exit 2; }

title_id=$(python3 - "$root/sce_sys/param.json" <<'PY'
import json, re, sys
with open(sys.argv[1], encoding="utf-8") as source:
    title_id = json.load(source)["titleId"]
if not re.fullmatch(r"PPSA\d{5}", title_id):
    raise SystemExit("param.json contains an invalid titleId")
print(title_id)
PY
)
base_url="ftp://$host:$port/data/homebrew"

if [[ $action == undeploy ]]; then
    printf '==> [undeploy] Title: %s\n' "$title_id"
    printf '==> [undeploy] Folder: %s/%s/\n' "$base_url" "$title_id"
    printf '==> [undeploy] Images: %s/%s.{ffpkg,ffpfsc}\n' "$base_url" "$title_id"

    if [[ ${DEPLOY_DRY_RUN:-0} == 1 ]]; then
        echo "==> [undeploy] Dry run complete; no network request was sent"
        exit 0
    fi

    python3 - "$host" "$port" "$user" "$password" "$title_id" <<'PY'
from ftplib import FTP, error_perm
from posixpath import join
import sys

host, port, user, password, title_id = sys.argv[1:]
homebrew_root = "/data/homebrew"


def reply_code(error):
    return str(error).split(maxsplit=1)[0]


def list_names(ftp, path):
    previous = ftp.pwd()
    ftp.cwd(path)
    try:
        return [name for name, _ in ftp.mlsd() if name not in {".", ".."}]
    finally:
        ftp.cwd(previous)


def validate_name(name):
    if not name or name in {".", ".."} or "/" in name or "\r" in name or "\n" in name:
        raise RuntimeError(f"unsafe FTP entry name: {name!r}")


def remove_entry(ftp, path):
    try:
        # ftpsrv implements DELE with POSIX remove(), which safely unlinks files,
        # symlinks, and empty directories before recursion is considered.
        ftp.delete(path)
        print(f"Removed: {path}")
        return True
    except error_perm as error:
        message = str(error)
        if reply_code(error) == "550" and "No such file or directory" in message:
            return False
        if reply_code(error) != "550" or "Directory not empty" not in message:
            raise

    for name in list_names(ftp, path):
        validate_name(name)
        remove_entry(ftp, join(path, name))
    ftp.rmd(path)
    print(f"Removed directory: {path}")
    return True


with FTP() as ftp:
    ftp.connect(host, int(port), timeout=15)
    ftp.login(user, password)
    removed = remove_entry(ftp, join(homebrew_root, title_id))
    for suffix in ("ffpkg", "ffpfsc"):
        removed |= remove_entry(ftp, join(homebrew_root, f"{title_id}.{suffix}"))
        removed |= remove_entry(ftp, join(homebrew_root, f".{title_id}.{suffix}.upload"))
    try:
        ftp.quit()
    except (EOFError, OSError):
        pass

if removed:
    print(f"Undeployment complete: {title_id}")
else:
    print(f"Nothing staged for {title_id}")
PY
    exit 0
fi

build_target=$format
[[ $format == folder ]] && build_target=app
echo "==> [deploy] Building $format"
make -C "$root" --no-print-directory "$build_target"

if [[ $format == folder ]]; then
    artifact="$root/dist/$title_id"
    [[ -d $artifact ]] || {
        echo "missing deployment folder: $artifact" >&2
        exit 2
    }
    [[ -s $artifact/eboot.bin && -s $artifact/sce_sys/param.json ]] || {
        echo "deployment folder is missing eboot.bin or sce_sys/param.json" >&2
        exit 2
    }
    mapfile -d '' files < <(
        find "$artifact" -type f \
            ! -path "$artifact/eboot.bin" \
            ! -path "$artifact/sce_sys/param.json" \
            -print0 | sort -z
    )
    files+=("$artifact/eboot.bin" "$artifact/sce_sys/param.json")
    total=${#files[@]}
    printf '==> [deploy] Target: %s/%s/\n' "$base_url" "$title_id"
else
    artifact="$root/dist/$title_id.$format"
    [[ -s $artifact ]] || {
        echo "missing deployment artifact: $artifact" >&2
        exit 2
    }
    remote_name="$title_id.$format"
    temporary_name=".$remote_name.upload"
    printf '==> [deploy] Target: %s/%s\n' "$base_url" "$remote_name"
fi

if [[ ${DEPLOY_DRY_RUN:-0} == 1 ]]; then
    if [[ $format == folder ]]; then
        echo "==> [deploy] Would publish $total files; eboot.bin and param.json are last"
    fi
    echo "==> [deploy] Dry run complete; no network request was sent"
    exit 0
fi

command -v curl >/dev/null || { echo "missing required command: curl" >&2; exit 2; }
common=(--fail --show-error --disable-epsv --user "$user:$password")

urlencode_path() {
    python3 - "$1" <<'PY'
import sys
from urllib.parse import quote
print(quote(sys.argv[1], safe="/-._~"))
PY
}

if [[ $format == folder ]]; then
    echo "==> [deploy] Publishing $total files; eboot.bin and param.json are last"
    index=0
    for file in "${files[@]}"; do
        ((index += 1))
        relative=${file#"$artifact/"}
        case $relative in
        *$'\n'* | *$'\r'*)
            echo "deployment paths cannot contain newlines: $relative" >&2
            exit 2
            ;;
        esac

        directory=$(dirname -- "$relative")
        filename=$(basename -- "$relative")
        if [[ $directory == . ]]; then
            temporary_relative=".$filename.upload"
        else
            temporary_relative="$directory/.$filename.upload"
        fi

        encoded_temporary=$(urlencode_path "$temporary_relative")
        remote_path="/data/homebrew/$title_id/$relative"
        temporary_path="/data/homebrew/$title_id/$temporary_relative"
        printf '==> [deploy] [%d/%d] %s\n' "$index" "$total" "$relative"
        curl "${common[@]}" --progress-bar --ftp-create-dirs \
            --upload-file "$file" "$base_url/$title_id/$encoded_temporary"
        curl "${common[@]}" --silent --list-only \
            --quote "*DELE $remote_path" \
            --quote "RNFR $temporary_path" \
            --quote "RNTO $remote_path" "ftp://$host:$port/" >/dev/null
    done

    root_listing=$(curl "${common[@]}" --silent --list-only \
        "$base_url/$title_id/")
    metadata_listing=$(curl "${common[@]}" --silent --list-only \
        "$base_url/$title_id/sce_sys/")
    grep -Fqx "eboot.bin" <<< "${root_listing//$'\r'/}" &&
        grep -Fqx "param.json" <<< "${metadata_listing//$'\r'/}" || {
        echo "FTP upload completed but required files are not listed" >&2
        exit 2
    }
    printf 'Deployment complete: %s/%s/\n' "$base_url" "$title_id"
    exit 0
fi

listing=$(curl "${common[@]}" --silent --list-only "$base_url/")
echo "==> [deploy] Uploading complete image under a temporary name"
curl "${common[@]}" --progress-bar --ftp-create-dirs \
    --upload-file "$artifact" "$base_url/$temporary_name"

for old_name in "$title_id.ffpfsc" "$title_id.ffpkg"; do
    if grep -Fqx "$old_name" <<< "${listing//$'\r'/}"; then
        echo "==> [deploy] Removing the previous $old_name image"
        curl "${common[@]}" --silent --list-only \
            --quote "DELE /data/homebrew/$old_name" "ftp://$host:$port/" >/dev/null
    fi
done

echo "==> [deploy] Publishing the completed image"
curl "${common[@]}" --silent --list-only \
    --quote "RNFR /data/homebrew/$temporary_name" \
    --quote "RNTO /data/homebrew/$remote_name" "ftp://$host:$port/" >/dev/null

listing=$(curl "${common[@]}" --silent --list-only "$base_url/")
grep -Fqx "$remote_name" <<< "${listing//$'\r'/}" || {
    echo "FTP upload completed but the final image is not listed" >&2
    exit 2
}
printf 'Deployment complete: %s/%s\n' "$base_url" "$remote_name"

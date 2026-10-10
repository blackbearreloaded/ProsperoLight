#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Stands in for meson (MESON) while RADV is built with USE_CCACHE=1. PS5_Vulkan's
# cross file names the SDK compilers: a cross setup reads ccache-cross.ini after
# it, which puts ccache in front of them. Every other call is passed on as it is.
here=$(cd -- "$(dirname -- "$0")" && pwd)
meson=$(command -v meson || echo "$HOME/.local/bin/meson")
cross=0
for argument in "$@"; do
    [ "$argument" = --cross-file ] && cross=1
done
if [ "${1:-}" = setup ] && [ "$cross" = 1 ]; then
    exec "$meson" "$@" --cross-file "$here/ccache-cross.ini"
fi
exec "$meson" "$@"

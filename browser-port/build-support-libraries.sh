#!/usr/bin/env bash
# Build components only. This does NOT produce a runnable browser NRO.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/.local/browser-engine/src"
test -f "$src/buildsystem/makefiles/Makefile.tools" || { echo 'Run prepare-engine.py first.'; exit 1; }
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PATH="$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$DEVKITPRO/portlibs/switch/bin:$PATH"
cd "$src"
prefix="$PWD/inst-framebuffer"
export CFLAGS="-mcpu=cortex-a78c+crypto -mtp=soft -fPIE -D__SWITCH__ -D__SWITCH2__ -I$DEVKITPRO/libounce/include -I$DEVKITPRO/portlibs/switch/include -O2"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-mcpu=cortex-a78c+crypto -mtp=soft -fPIE -L$DEVKITPRO/libounce/lib -L$DEVKITPRO/portlibs/switch/lib"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig:$DEVKITPRO/portlibs/switch/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
export PKG_CONFIG=pkg-config
for lib in buildsystem libnslog libwapcaplet libparserutils libcss libhubbub libdom libnsbmp libnsgif librosprite libnsutils libutf8proc libnspsl libsvgtiny libnsfb; do
 make -C "$lib" install HOST=aarch64-none-elf PREFIX="$prefix" NSSHARED="$PWD/buildsystem" WARNFLAGS='-Wall -Wextra -Wno-error' DESTDIR= -j4
done
echo 'Browser component libraries compiled. No browser NRO was produced.'

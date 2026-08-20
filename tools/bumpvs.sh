#!/bin/sh
set -eu

major="$1"
minor="$2"
patch="$3"
ver="${major}.${minor}.${patch}"

perl -0pi -e "s/#define RK_VERSION_MAJOR .*/#define RK_VERSION_MAJOR ${major}/;
              s/#define RK_VERSION_MINOR .*/#define RK_VERSION_MINOR ${minor}/;
              s/#define RK_VERSION_PATCH .*/#define RK_VERSION_PATCH ${patch}/" \
     core/inc/kversion.h

perl -0pi -e "s/V[0-9]+\\.[0-9]+\\.[0-9]+/V${ver}/g" \
    $(find app arch core -type f \( -name '*.c' -o -name '*.h' -o -name '*.S' \))

perl -0pi -e "s/version-[0-9]+\\.[0-9]+\\.[0-9]+-blue/version-${ver}-blue/" README.md

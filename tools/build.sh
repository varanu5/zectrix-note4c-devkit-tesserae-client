#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 varanu5 <https://github.com/varanu5>
set -eu
cd "$(dirname "$0")/../firmware"
if ! command -v idf.py >/dev/null 2>&1; then
    echo 'Activate ESP-IDF 6.1 first: . "$HOME/esp/esp-idf-v6.1/export.sh"' >&2
    exit 1
fi
if ! command -v xtensa-esp32s3-elf-gcc >/dev/null 2>&1; then
    echo 'Install the S3 toolchain: python3 "$IDF_PATH/tools/idf_tools.py" install xtensa-esp-elf' >&2
    exit 1
fi
case "$(idf.py --version)" in
    'ESP-IDF v6.1'|'ESP-IDF v6.1.0')
        export IDF_COMPONENT_MANAGER=0
        ;;
    *) echo 'Use ESP-IDF 6.1 for this project.' >&2; exit 1 ;;
esac
if [ "$#" -gt 1 ] || [ "${1:-rest}" != rest ]; then
    echo 'Usage: sh tools/build.sh [rest]' >&2
    exit 2
fi
NOTE4C_DEFAULTS=sdkconfig.defaults
NOTE4C_BUILD="$PWD/build"
NOTE4C_CONFIG="$PWD/sdkconfig"
idf.py -B "$NOTE4C_BUILD" -D "SDKCONFIG=$NOTE4C_CONFIG" \
    -D "SDKCONFIG_DEFAULTS=$NOTE4C_DEFAULTS" -D EXTRA_COMPONENT_DIRS= build

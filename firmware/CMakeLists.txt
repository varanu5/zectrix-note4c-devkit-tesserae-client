# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 varanu5 <https://github.com/varanu5>
cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET esp32s3)
set(PROJECT_VER "1.1.1")
set(ENV{IDF_COMPONENT_MANAGER} "0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(note4c_tesserae)

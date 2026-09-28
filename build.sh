#!/bin/sh
set -eu

git submodule update --init
cmake -S atari800 -B build -G Ninja \
    -DPICO_PLATFORM=rp2350-arm-s \
    -DHW_CONFIG=8
cmake --build build

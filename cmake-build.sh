#!/usr/bin/env bash
set -e

BUILD_DIR="build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release

time cmake --build . -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

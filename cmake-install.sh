#!/usr/bin/env bash
set -e

BUILD_DIR="build"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory '$BUILD_DIR' does not exist."
    echo "Run ./cmake-build.sh first."
    exit 1
fi

cd "$BUILD_DIR"
cmake --install .

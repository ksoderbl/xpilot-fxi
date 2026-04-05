#!/usr/bin/env bash
set -e

BUILD_DIR="build"

if [ -z "$BUILD_DIR" ] || [ "$BUILD_DIR" = "/" ]; then
    echo "Refusing to delete unsafe BUILD_DIR"
    exit 1
fi

if [ -d "$BUILD_DIR" ]; then
    echo "Removing build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
else
    echo "Nothing to clean."
fi

echo "Clean complete."

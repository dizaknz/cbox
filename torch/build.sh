#!/bin/bash

quit() {
    local err=$1

    echo "ERROR: ${err}"
    exit 1
}

[ -d build ] || mkdir build

conan install . --output-folder=build --build=missing || {
    quit "Failed to install dependencies"
}

cmake -B build -S . -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./build/build/Release/generators/conan_toolchain.cmake || {
    quit "Failed to prepare build files"
}

cmake --build build -j4 || {
    quit "Build failed"
}

ctest --test-dir build || {
    quit "Tests failed"
}

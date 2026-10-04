#!/bin/bash

cmake -B build -S .
cmake --build build
cd build && ctest --output-on-failure

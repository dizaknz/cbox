#!/bin/bash

for f in *.cpp; do
    out=${f%.cpp}
    g++ $f -o $out -std=c++11
done

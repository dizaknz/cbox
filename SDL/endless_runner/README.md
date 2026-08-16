# Endless Runner

# Requirements

## Conan

Install [conan](https://conan.io/)

```
pip install conan
```

Setup profile for build system

```
conan profile detect --force
```

# Setup

Install project depedencies

```
mkdir build
conan install . --output-folder=build --build=missing
```

Setup cmake

```
cmake -B build -S . -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./build/conan_provider.cmake
```

# Build

```
cd build
cmake --build .
```

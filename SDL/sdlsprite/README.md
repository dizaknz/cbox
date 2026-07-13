# Sprite animation demo

Basic demo of animating sprite speaking

## Event loop

Switch modes using the following key presses:

* `q` - be quiet
* `l` - pretent to listen
* `s` - animate as if it's speaking
* `p` - previous sprite in series, hold to animate series in reverse
* `n` - next sprite in series, hold to animate sprites at full tick
* `x` - quits

# Setup steps

Requires

* `cmake`
* `conan`

## Conan

Install [conan](https://conan.io/)

```
pip install conan
```

Setup profile for build system

```
conan profile detect --force
```

## Project setup

Install project depedencies

```
mkdir build
conan install . --output-folder=build --build=missing
```

Setup cmake

```
cmake -B build -S . -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./build/conan_provider.cmake
```


# Build steps

```
cd build
cmake --build .
```

# Run it

```
build/Debug/sdlsprite
```

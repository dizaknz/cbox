# Sprite animation demo

Basic demo of animating sprite speaking

## Event loop

Switch modes using the following key presses:

* `q` - be quiet
* `l` - pretent to listen
* `s` - animate as if it's speaking
* `x` - quits

# Build steps

Requires

* `cmake`
* `conan`

```
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

# Run it

```
bin/sdlsprite
```

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

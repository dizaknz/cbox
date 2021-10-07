# amqpuv

Demo project using [AMQP-CPP](https://github.com/CopernicaMarketingSoftware/AMQP-CPP) with [libuv](https://github.com/libuv/libuv) event loop

## Build it

```
conan profile new default --detect
conan profile update settings.compiler.libcxx=libstdc++11 default
cmake . -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Run demo

```
docker-compose up -d
bin/amqpuv_demo
docker-compose down -v
```

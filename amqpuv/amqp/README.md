# amqp

AMQP C++ messaging wrapper library

## build

```
cmake . -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## package

Export package to local conan repo

```
conan export-pkg . amqp/0.1@
```


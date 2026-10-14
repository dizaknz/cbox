cmake_minimum_required(VERSION 3.20)
project(ota_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Sources ───────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    main.cpp
    ota_handler.cpp
    simulated_ota_port.cpp
)

# ── Compiler flags ────────────────────────────────────────────────────────────
# Clang: requires -fcoroutines for coroutine support
# GCC 13+: requires -fcoroutines
# MSVC: /await or /std:c++20 with coroutine support

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fcoroutines)
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /std:c++20 /await:strict)
endif()

# ── Threading ─────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# ── Warnings ──────────────────────────────────────────────────────────────────
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4>
)

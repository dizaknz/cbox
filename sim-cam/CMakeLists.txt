cmake_minimum_required(VERSION 3.24)
project(camera-fleet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Fetch FlatBuffers headers (winget only ships flatc.exe) ───────
include(FetchContent)
FetchContent_Declare(flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG v25.12.19
  GIT_SHALLOW ON
)
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(flatbuffers)
set(FLATBUFFERS_INCLUDE_DIR "${flatbuffers_SOURCE_DIR}/include")

# ── Global warnings (MSVC vs GCC/Clang) ──────────────────────────
if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wnon-virtual-dtor -Wold-style-cast
                      -Wcast-align -Woverloaded-virtual -Wconversion -Wsign-conversion)
endif()

# ── Subdirectories ────────────────────────────────────────────────
add_subdirectory(device/camera)

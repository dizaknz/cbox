project(torchdemo)
cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan_provider.cmake")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/c22bbf0af0b73d5f0def24a9cdf4ce503ae79e5d/conan_provider.cmake"
         "${CMAKE_BINARY_DIR}/conan_provider.cmake"
         EXPECTED_HASH SHA256=796E17C1240D7B2167388B9F68411208084A641ED25DF172D64D80EF92F8DA7A
         TLS_VERIFY ON)
endif()
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_BINARY_DIR}/conan_provider.cmake)
 
set(CMAKE_CXX_STANDARD 20)

find_package(Torch REQUIRED)

add_library(tlib SHARED
    dcgan.hpp
    net.hpp
    loader.hpp
)
set_target_properties(tlib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(tlib
    "${TORCH_LIBRARIES}"
    spdlog 
    fmt
)
add_executable(torchdemo train.cpp)
target_link_libraries(torchdemo
    "${TORCH_LIBRARIES}"
    spdlog 
    fmt)

add_executable(demo main.cpp)
target_link_libraries(demo
    "${TORCH_LIBRARIES}"
    spdlog 
    fmt)

cmake_minimum_required(VERSION 3.24)
project(desert_ruunner LANGUAGES CXX)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan_provider.cmake")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/c22bbf0af0b73d5f0def24a9cdf4ce503ae79e5d/conan_provider.cmake"
         "${CMAKE_BINARY_DIR}/conan_provider.cmake"
         EXPECTED_HASH SHA256=796E17C1240D7B2167388B9F68411208084A641ED25DF172D64D80EF92F8DA7A
         TLS_VERIFY ON)
endif()
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_BINARY_DIR}/conan_provider.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

add_executable(desert_runner main.cpp)
target_include_directories(desert_runner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR})
target_link_libraries(desert_runner PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

cmake_minimum_required(VERSION 3.24)
project(detect CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan_provider.cmake")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/c22bbf0af0b73d5f0def24a9cdf4ce503ae79e5d/conan_provider.cmake"
         "${CMAKE_BINARY_DIR}/conan_provider.cmake"
         EXPECTED_HASH SHA256=796E17C1240D7B2167388B9F68411208084A641ED25DF172D64D80EF92F8DA7A
         TLS_VERIFY ON)
endif()
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_BINARY_DIR}/conan_provider.cmake)


find_package(OpenCV REQUIRED)
find_package(tensorflowlite REQUIRED)

add_executable(detect main.cpp detector.cpp)
target_compile_definitions(detect PRIVATE DETECT_WITH_OPENCV)
target_include_directories(detect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR})
target_link_libraries(detect PRIVATE tensorflow::tensorflowlite opencv::opencv)

# Geometry and label tests -- no OpenCV or TFLite needed.
enable_testing()
add_executable(test_letterbox test_letterbox.cpp)
add_executable(test_labels test_labels.cpp)
target_include_directories(test_letterbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(test_labels PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME letterbox COMMAND test_letterbox)
add_test(NAME labels COMMAND test_labels)

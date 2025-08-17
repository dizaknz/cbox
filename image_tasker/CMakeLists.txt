cmake_minimum_required(VERSION 3.24)
project(image_tasker_app LANGUAGES CXX)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan_provider.cmake")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/c22bbf0af0b73d5f0def24a9cdf4ce503ae79e5d/conan_provider.cmake"
         "${CMAKE_BINARY_DIR}/conan_provider.cmake"
         EXPECTED_HASH SHA256=796E17C1240D7B2167388B9F68411208084A641ED25DF172D64D80EF92F8DA7A
         TLS_VERIFY ON)
endif()
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_BINARY_DIR}/conan_provider.cmake)

set(CMAKE_CXX_STANDARD 20)
set(SOURCE_DIR src)
set(INCLUDE_DIR include)
set(APP_DIR app)
set(TEST_DIR tests)

find_package(stb REQUIRED)
find_package(spdlog REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
add_executable(image_tasker_demo
    ${SOURCE_DIR}/image_data.cpp
    ${SOURCE_DIR}/image_task_manager.cpp
    ${SOURCE_DIR}/image_cache.cpp
    ${SOURCE_DIR}/image_manager.cpp
    ${APP_DIR}/demo.cpp)
target_include_directories(image_tasker_demo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR})
target_link_libraries(image_tasker_demo PUBLIC stb::stb spdlog::spdlog)
target_link_libraries(image_tasker_demo PRIVATE CLI11::CLI11)

find_package(GTest 1.16.0 CONFIG REQUIRED)
add_executable(image_tasker_test
    ${SOURCE_DIR}/image_data.cpp
    ${SOURCE_DIR}/image_task_manager.cpp
    ${SOURCE_DIR}/image_cache.cpp
    ${SOURCE_DIR}/image_manager.cpp
    ${TEST_DIR}/image_task_test.cpp
)
target_include_directories(image_tasker_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR})
target_link_libraries(image_tasker_test PUBLIC stb::stb spdlog::spdlog)
target_link_libraries(image_tasker_test PRIVATE GTest::gtest GTest::gtest_main)
target_link_libraries(image_tasker_test PRIVATE CLI11::CLI11)

enable_testing()
include(GoogleTest)
gtest_discover_tests(image_tasker_test)
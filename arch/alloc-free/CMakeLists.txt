cmake_minimum_required(VERSION 3.24)
project(AllocationFreeStateMachine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(spdlog Catch2)

add_library(state_machine INTERFACE)
target_include_directories(state_machine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(state_machine INTERFACE spdlog::spdlog_header_only)

enable_testing()

add_executable(unit_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_state_machine.cpp)
target_link_libraries(unit_tests PRIVATE 
    state_machine 
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(unit_tests)


cmake_minimum_required(VERSION 3.24)

# ── FlatBuffers compiler (find or fetch) ──────────────────────────
find_program(FLATC flatc)
if(NOT FLATC)
  message(FATAL_ERROR "flatc not found. Install FlatBuffers compiler.")
endif()

set(SCHEMA_DIR "${CMAKE_SOURCE_DIR}/schemas")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/generated")

# Generate C++ headers from schemas
file(GLOB SCHEMAS "${SCHEMA_DIR}/*.fbs")
foreach(fbs ${SCHEMAS})
  get_filename_component(fname ${fbs} NAME_WE)
  list(APPEND GEN_HEADERS "${GENERATED_DIR}/${fname}_generated.h")
endforeach()

add_custom_command(
  OUTPUT ${GEN_HEADERS}
  COMMAND ${FLATC} --cpp -o "${GENERATED_DIR}" ${SCHEMAS}
  DEPENDS ${SCHEMAS}
  COMMENT "Generating FlatBuffers C++ headers"
  VERBATIM
)

# schema_gen is a UTILITY target — can't link against it, use add_dependencies instead
add_custom_target(schema_gen DEPENDS ${GEN_HEADERS})

# ── Include directories (shared across all targets) ───────────────
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/libs
  ${CMAKE_CURRENT_SOURCE_DIR}/adapters/sim
  ${CMAKE_CURRENT_SOURCE_DIR}/services
  ${GENERATED_DIR}
  ${FLATBUFFERS_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/sim
)

# ── Value library ─────────────────────────────────────────────────
add_library(value STATIC
  libs/value/result.cpp
  libs/value/uuid.cpp
  libs/value/clock_helpers.cpp
)
add_dependencies(value schema_gen)

# ── Reactor library ───────────────────────────────────────────────
add_library(reactor STATIC
  libs/reactor/reactor.cpp
)
add_dependencies(reactor schema_gen)
if(WIN32)
  target_compile_definitions(reactor PRIVATE REACTOR_WIN32)
else()
  target_link_libraries(reactor PUBLIC pthread rt)
endif()

# ── Bus library ───────────────────────────────────────────────────
add_library(bus STATIC
  libs/bus/endpoint.cpp
)
add_dependencies(bus schema_gen)
target_link_libraries(bus PUBLIC reactor value)

# ── Simulation adapters ───────────────────────────────────────────
add_library(sim_adapters STATIC
  adapters/sim/event_source.cpp
  adapters/sim/clip_source.cpp
  adapters/sim/clock.cpp
  adapters/sim/slot_backend.cpp
)
add_dependencies(sim_adapters schema_gen)
target_link_libraries(sim_adapters PUBLIC value reactor)

# ── Services ──────────────────────────────────────────────────────
add_executable(eventd services/eventd/main.cpp services/eventd/eventd.cpp)
add_dependencies(eventd schema_gen)
target_link_libraries(eventd PRIVATE bus sim_adapters value)
set_target_properties(eventd PROPERTIES OUTPUT_NAME eventd)

add_executable(sensord services/sensord/main.cpp services/sensord/sensord.cpp)
add_dependencies(sensord schema_gen)
target_link_libraries(sensord PRIVATE bus sim_adapters value)
set_target_properties(sensord PROPERTIES OUTPUT_NAME sensord)

# ── Simulation supervisor ─────────────────────────────────────────
add_executable(sim-supervisor
  "${CMAKE_SOURCE_DIR}/sim/supervisor/main.cpp"
  "${CMAKE_SOURCE_DIR}/sim/supervisor/scenario.cpp"
  "${CMAKE_SOURCE_DIR}/sim/supervisor/device.cpp"
)
add_dependencies(sim-supervisor schema_gen)
target_link_libraries(sim-supervisor PRIVATE value reactor sim_adapters)
if(WIN32)
  target_compile_definitions(sim-supervisor PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ── Sanitizer flags for debug builds (GCC/Clang only, not MSVC) ───
# clang-cl on Windows supports sanitizers even though MSVC=ON
set(_SUPPORTS_SANITIZERS OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(_SUPPORTS_SANITIZERS ON)
endif()

if(CAMERA_SANITIZERS AND _SUPPORTS_SANITIZERS)
  # ASan + UBSan on Windows clang-cl requires MSVC's ASan STL runtime libraries
  # (stl_asan.lib, ucrtd_asan.lib) which are only available via VS generator.
  # Ninja + clang-cl cannot resolve these. Recommend WSL/Linux for sanitizers.
  if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Sanitizers disabled on Windows clang-cl (requires VS generator or WSL/Linux)")
  else()
    message(STATUS "Enabling sanitizers (ASan + UBSan)")
    set(SAN_FLAGS "-fsanitize=address,undefined")
    target_compile_options(value PRIVATE ${SAN_FLAGS})
    target_link_options(value PRIVATE ${SAN_FLAGS})
    target_compile_options(reactor PRIVATE ${SAN_FLAGS})
    target_link_options(reactor PRIVATE ${SAN_FLAGS})
    target_compile_options(bus PRIVATE ${SAN_FLAGS})
    target_link_options(bus PRIVATE ${SAN_FLAGS})
    target_compile_options(sim_adapters PRIVATE ${SAN_FLAGS})
    target_link_options(sim_adapters PRIVATE ${SAN_FLAGS})
    target_compile_options(eventd PRIVATE ${SAN_FLAGS})
    target_link_options(eventd PRIVATE ${SAN_FLAGS})
    target_compile_options(sensord PRIVATE ${SAN_FLAGS})
    target_link_options(sensord PRIVATE ${SAN_FLAGS})
    target_compile_options(sim-supervisor PRIVATE ${SAN_FLAGS})
    target_link_options(sim-supervisor PRIVATE ${SAN_FLAGS})
  endif()
elseif(CAMERA_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT _SUPPORTS_SANITIZERS)
  message(STATUS "Sanitizers not supported on MSVC (use clang-cl or WSL)")
endif()

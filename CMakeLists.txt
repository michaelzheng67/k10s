cmake_minimum_required(VERSION 3.25)
project(k10s VERSION 0.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd / IDE tooling

option(K10S_SANITIZE    "Enable ASan + UBSan"  OFF)
option(K10S_TSAN        "Enable ThreadSanitizer (not combinable with ASan)" OFF)
option(K10S_BUILD_TESTS "Build unit tests"     ON)
set(K10S_VERSION "${PROJECT_VERSION}" CACHE STRING "Version string reported by --version")

# ---------------------------------------------------------------------------
# Dependencies (pinned tags -> reproducible builds; SYSTEM -> no warnings
# from third-party headers)
# ---------------------------------------------------------------------------
include(FetchContent)

# CMake 4 rejects projects declaring cmake_minimum_required < 3.5, which some
# dependencies still do (yaml-cpp 0.8.0). Older CMake ignores this variable.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.5
  GIT_SHALLOW    TRUE
  SYSTEM)
FetchContent_MakeAvailable(raylib)

set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        0.8.0
  GIT_SHALLOW    TRUE
  SYSTEM)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(nlohmann_json
  URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  SYSTEM)
FetchContent_MakeAvailable(nlohmann_json)

# System libcurl (macOS SDK ships 8.x with a LibreSSL backend).
find_package(CURL REQUIRED)

# ---------------------------------------------------------------------------
# Project-wide settings applied per target (never globally, so third-party
# code isn't compiled with our strict flags)
# ---------------------------------------------------------------------------
function(k10s_configure_target target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
  if(K10S_SANITIZE)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target}    PRIVATE -fsanitize=address,undefined)
  endif()
  if(K10S_TSAN)
    target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target}    PRIVATE -fsanitize=thread)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# k10s_core: everything that isn't main().
# ---------------------------------------------------------------------------
add_library(k10s_core STATIC
  src/model/cluster_state.cpp
  src/sim/fake_cluster.cpp
  src/source/fake_source.cpp
  src/source/kube_source.cpp
  src/kube/kubeconfig.cpp
  src/kube/http.cpp
  src/ui/command_ui.cpp
  src/render/person.cpp
  src/render/appearance.cpp
  src/world/villagers.cpp
  src/world/movement.cpp
  src/world/layout.cpp
  src/render/house.cpp
  src/render/scenery.cpp
  src/render/camera.cpp
  src/theme/theme.cpp
  src/theme/village.cpp
  src/theme/city.cpp
  src/theme/beach.cpp
  src/util/config.cpp)
target_include_directories(k10s_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(k10s_core PUBLIC raylib yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json CURL::libcurl)
k10s_configure_target(k10s_core)

# ---------------------------------------------------------------------------
# The app
# ---------------------------------------------------------------------------
add_executable(k10s src/main.cpp)
target_link_libraries(k10s PRIVATE k10s_core)
target_compile_definitions(k10s PRIVATE K10S_VERSION="${K10S_VERSION}")
k10s_configure_target(k10s)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(K10S_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
    SYSTEM)
  FetchContent_MakeAvailable(Catch2)

  enable_testing()
  add_executable(k10s_tests
    tests/smoke_test.cpp
    tests/cluster_state_test.cpp
    tests/fake_cluster_test.cpp
    tests/appearance_test.cpp
    tests/villagers_test.cpp
    tests/spsc_queue_test.cpp
    tests/spsc_stress_test.cpp
    tests/kubeconfig_test.cpp
    tests/session_test.cpp
    tests/kube_parse_test.cpp
    tests/layout_test.cpp
    tests/camera_test.cpp
    tests/config_test.cpp)
  target_link_libraries(k10s_tests PRIVATE k10s_core Catch2::Catch2WithMain)
  k10s_configure_target(k10s_tests)

  include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
  catch_discover_tests(k10s_tests)
endif()

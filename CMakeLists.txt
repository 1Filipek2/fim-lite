cmake_minimum_required(VERSION 3.20)

project(fim_lite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14
)

FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(json)

find_package(OpenSSL REQUIRED)

add_library(fimlite_core
    src/hasher.cpp
    src/baseline.cpp
    src/scanner.cpp
    src/cli.cpp
    src/paths.cpp
    src/fsync.cpp
    src/hmac.cpp
)

if (WIN32)
    target_sources(fimlite_core PRIVATE src/console.cpp)
endif()

target_include_directories(fimlite_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fimlite_core PUBLIC
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
)

if (MSVC)
    target_compile_options(fimlite_core PUBLIC /utf-8)
endif()

add_executable(fim_lite
    src/main.cpp
)

target_link_libraries(fim_lite PRIVATE
    fimlite_core
)

add_executable(fim_lite_tests
    tests/test_baseline.cpp
    tests/test_cli.cpp
    tests/test_fsync.cpp
    tests/test_hmac.cpp
    tests/test_scanner.cpp
)

target_link_libraries(fim_lite_tests PRIVATE
    fimlite_core
    Catch2::Catch2WithMain
)

enable_testing()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(fim_lite_tests)
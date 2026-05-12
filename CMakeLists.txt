cmake_minimum_required(VERSION 3.22)
project(OpenSmartOffice VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Build Options ----
option(OSO_BUILD_GUI      "Build Qt desktop GUI"        OFF)
option(OSO_BUILD_SERVICE  "Build HTTP service"           OFF)
option(OSO_BUILD_CLI      "Build CLI tool"               OFF)
option(OSO_BUILD_TESTS    "Build unit tests"             ON)
option(OSO_USE_SYSTEM_QT  "Use system-installed Qt"      ON)
option(OSO_LINK_STATIC    "Statically link dependencies" ON)

# ---- CMake Modules ----
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(OsoModule)

# ---- Third-party dependencies ----
# 这些配置必须放在根作用域下，才能让导入的目标在所有子目录中可见
# 导入的目标作用域仅限于调用 find_package 的目录及其子目录
# 如果需要FetchContent导入则在third_party中处理

# Qt
find_package(Qt6 REQUIRED COMPONENTS Core Concurrent)
if(OSO_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
endif()

# System libs
find_package(LibXml2 REQUIRED)
find_package(libzip REQUIRED MODULE)

# spdlog (header-only logging)
find_package(spdlog REQUIRED)

# nlohmann_json (header-only JSON)
find_package(nlohmann_json REQUIRED)

# cpp-httplib (header-only HTTP)
find_package(httplib REQUIRED MODULE)

# ---- FetchContent deps ----
include(FetchContent)
add_subdirectory(third_party)

# ---- Layers (bottom-up) ----
add_subdirectory(platform)
add_subdirectory(infra)
add_subdirectory(core)
if(OSO_BUILD_CLI OR OSO_BUILD_GUI OR OSO_BUILD_SERVICE)
    add_subdirectory(app)
endif()

# ---- Tests ----
if(OSO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

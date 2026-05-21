cmake_minimum_required(VERSION 3.16)
project(mullvad_socks5_probe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(USE_SYSTEM_DEPS "Prefer system-installed dependencies (fmt, libcurl, nlohmann_json)" ON)

if(USE_SYSTEM_DEPS)
  find_package(fmt CONFIG QUIET)
  find_package(CURL QUIET)
  find_package(nlohmann_json CONFIG QUIET)
endif()

include(FetchContent)

if(NOT fmt_FOUND)
  message(STATUS "fmt not found on system, fetching from source")
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.0.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(fmt)
endif()

if(NOT nlohmann_json_FOUND)
  message(STATUS "nlohmann_json not found on system, fetching from source")
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

if(NOT CURL_FOUND)
  message(STATUS "CURL not found on system, fetching from source")
  FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_4_0
    GIT_SHALLOW TRUE
  )
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(curl)
endif()

find_package(Threads REQUIRED)

add_executable(mullvad_socks5_probe
  src/main.cpp
  src/relay_probe.cpp
)

target_include_directories(mullvad_socks5_probe PRIVATE include)

target_link_libraries(mullvad_socks5_probe PRIVATE
  fmt::fmt
  nlohmann_json::nlohmann_json
  Threads::Threads
)

if(TARGET CURL::libcurl)
  target_link_libraries(mullvad_socks5_probe PRIVATE CURL::libcurl)
else()
  target_link_libraries(mullvad_socks5_probe PRIVATE libcurl)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mullvad_socks5_probe PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
  target_compile_options(mullvad_socks5_probe PRIVATE /W4)
endif()

install(TARGETS mullvad_socks5_probe RUNTIME DESTINATION bin)

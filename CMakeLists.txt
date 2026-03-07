cmake_minimum_required(VERSION 3.16)
project(MullvadRelayChecker VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mullvad_relay_checker
  main.cpp
  MullvadRelayChecker.cpp
)

include(FetchContent)

# Fetch fmt
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        10.0.0
)
FetchContent_MakeAvailable(fmt)

# Fetch nlohmann_json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# Fetch cURL
FetchContent_Declare(
  curl
  GIT_REPOSITORY https://github.com/curl/curl.git
  GIT_TAG        curl-8_4_0
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(curl)

find_package(Threads REQUIRED)

target_link_libraries(mullvad_relay_checker PRIVATE
  fmt::fmt
  libcurl
  nlohmann_json::nlohmann_json
  Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mullvad_relay_checker PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(mullvad_relay_checker PRIVATE /W4)
endif()

install(TARGETS mullvad_relay_checker
  RUNTIME DESTINATION bin
)

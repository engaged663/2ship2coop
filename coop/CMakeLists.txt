# Co-op mod: shared library, dedicated server, test bot and tests.
# Works both inside the game build (add_subdirectory from the root CMakeLists) and standalone:
#   cmake -S coop -B build/coop -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build/coop --config Release
cmake_minimum_required(VERSION 3.16)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(coop LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Same static runtime as the game; also makes the server exe self-contained on Windows.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# The sources hold UTF-8 texts (accents, Chinese, Russian): keep them byte for byte.
if (MSVC)
    add_compile_options(/utf-8)
endif()

include(FetchContent)

# --- ENet (UDP transport). Compiled with our own target: its CMakeLists targets CMake 2.x. ---
FetchContent_Declare(coop_enet_src
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.18
    SOURCE_SUBDIR do-not-add-subdirectory)
FetchContent_MakeAvailable(coop_enet_src)
set(COOP_ENET_DIR ${coop_enet_src_SOURCE_DIR})
add_library(coop_enet STATIC
    ${COOP_ENET_DIR}/callbacks.c ${COOP_ENET_DIR}/compress.c ${COOP_ENET_DIR}/host.c
    ${COOP_ENET_DIR}/list.c ${COOP_ENET_DIR}/packet.c ${COOP_ENET_DIR}/peer.c
    ${COOP_ENET_DIR}/protocol.c ${COOP_ENET_DIR}/unix.c ${COOP_ENET_DIR}/win32.c)
target_include_directories(coop_enet PUBLIC ${COOP_ENET_DIR}/include)
if (WIN32)
    target_link_libraries(coop_enet PUBLIC ws2_32 winmm)
    target_compile_options(coop_enet PRIVATE /W0)
else()
    target_compile_definitions(coop_enet PRIVATE HAS_SOCKLEN_T=1 HAS_POLL=1 HAS_FCNTL=1 HAS_MSGHDR_FLAGS=1
        HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1 HAS_INET_PTON=1 HAS_INET_NTOP=1)
endif()

# --- nlohmann-json (vcpkg in the game build, downloaded when building standalone). ---
find_package(nlohmann_json 3 QUIET)
if (NOT nlohmann_json_FOUND)
    FetchContent_Declare(coop_json_src URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
    FetchContent_MakeAvailable(coop_json_src)
endif()

# --- coop_common: protocol + transport shared by server, bot, tests and the game. ---
file(GLOB COOP_COMMON_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp)
add_library(coop_common STATIC ${COOP_COMMON_SOURCES})
target_include_directories(coop_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(coop_common PUBLIC cxx_std_20)
target_link_libraries(coop_common PUBLIC coop_enet nlohmann_json::nlohmann_json)

# --- server core as an OBJECT library so self-registering handlers/commands are never dropped by the linker. ---
file(GLOB_RECURSE COOP_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp)
list(FILTER COOP_SERVER_SOURCES EXCLUDE REGEX ".*/server/main\\.cpp$")
if (COOP_SERVER_SOURCES)
    add_library(coop_server_core OBJECT ${COOP_SERVER_SOURCES})
    target_link_libraries(coop_server_core PUBLIC coop_common)
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)
    add_executable(2ship-coop-server ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)
    target_link_libraries(2ship-coop-server PRIVATE coop_server_core)
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CoopBot.cpp)
    add_executable(2ship-coop-bot ${CMAKE_CURRENT_SOURCE_DIR}/tools/CoopBot.cpp)
    target_link_libraries(2ship-coop-bot PRIVATE coop_common)
endif()

option(COOP_BUILD_TESTS "Build the co-op test runner" ON)
if (COOP_BUILD_TESTS)
    file(GLOB COOP_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    add_executable(coop-tests ${COOP_TEST_SOURCES})
    if (TARGET coop_server_core)
        target_link_libraries(coop-tests PRIVATE coop_server_core)
    else()
        target_link_libraries(coop-tests PRIVATE coop_common)
    endif()
endif()

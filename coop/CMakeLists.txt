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

# --- Lua 5.4 (scripts of the mod system). Built as C++: a Lua error unwinds our C++ frames like an exception. ---
FetchContent_Declare(coop_lua_src
    URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
    URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
    SOURCE_SUBDIR do-not-add-subdirectory)
FetchContent_MakeAvailable(coop_lua_src)
set(COOP_LUA_DIR ${coop_lua_src_SOURCE_DIR}/src)
file(GLOB COOP_LUA_SOURCES ${COOP_LUA_DIR}/*.c)
list(FILTER COOP_LUA_SOURCES EXCLUDE REGEX ".*/(lua|luac)\\.c$")
add_library(coop_lua STATIC ${COOP_LUA_SOURCES})
set_source_files_properties(${COOP_LUA_SOURCES} PROPERTIES LANGUAGE CXX)
target_include_directories(coop_lua PUBLIC ${COOP_LUA_DIR})
target_compile_features(coop_lua PUBLIC cxx_std_17)
if (MSVC)
    target_compile_options(coop_lua PRIVATE /W0)
else()
    target_compile_options(coop_lua PRIVATE -w)
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
    target_link_libraries(coop_server_core PUBLIC coop_common coop_lua ${CMAKE_DL_LIBS})
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)
    add_executable(2ship-coop-server ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)
    target_link_libraries(2ship-coop-server PRIVATE coop_server_core)
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CoopBot.cpp)
    add_executable(2ship-coop-bot ${CMAKE_CURRENT_SOURCE_DIR}/tools/CoopBot.cpp)
    target_link_libraries(2ship-coop-bot PRIVATE coop_common)
endif()

# --- Plugins (server/Mods/PluginMod.h): the SDK's example and, with the tests, the plugins they load. ---
function(coop_plugin target source)
    add_library(${target} SHARED ${source})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
    target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json)
    target_compile_features(${target} PRIVATE cxx_std_17)
    set_target_properties(${target} PROPERTIES PREFIX "")
endfunction()
coop_plugin(coop_example_plugin ${CMAKE_CURRENT_SOURCE_DIR}/sdk/example/ejemplo_plugin.cpp)
set_target_properties(coop_example_plugin PROPERTIES OUTPUT_NAME "ejemplo-plugin")

# What a plugin's author needs, gathered in <build>/sdk to ship next to the server: our two headers, nlohmann's
# json (the single header when we downloaded it, the installed headers otherwise) and the example.
find_path(COOP_JSON_INCLUDE NAMES nlohmann/json.hpp
    HINTS ${coop_json_src_SOURCE_DIR}/single_include ${coop_json_src_SOURCE_DIR}/include)
if (COOP_JSON_INCLUDE)
    add_custom_target(coop_sdk ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/sdk ${CMAKE_CURRENT_BINARY_DIR}/sdk
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${COOP_JSON_INCLUDE}/nlohmann
                ${CMAKE_CURRENT_BINARY_DIR}/sdk/nlohmann
        COMMENT "Plugin SDK -> ${CMAKE_CURRENT_BINARY_DIR}/sdk")
endif()

option(COOP_BUILD_TESTS "Build the co-op test runner" ON)
if (COOP_BUILD_TESTS)
    file(GLOB COOP_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    add_executable(coop-tests ${COOP_TEST_SOURCES})
    # TestModsDocs.cpp reads the repository's copies: coop/docs/mods/*.md and coop/mods/ejemplo.lua.
    target_compile_definitions(coop-tests PRIVATE COOP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if (TARGET coop_server_core)
        target_link_libraries(coop-tests PRIVATE coop_server_core)
    else()
        target_link_libraries(coop-tests PRIVATE coop_common)
    endif()
    # The plugins TestModsPlugin.cpp loads: built next to coop-tests, which finds them by its own folder.
    coop_plugin(coop_test_plugin ${CMAKE_CURRENT_SOURCE_DIR}/tests/plugin/TestPlugin.cpp)
    coop_plugin(coop_test_plugin_badabi ${CMAKE_CURRENT_SOURCE_DIR}/tests/plugin/TestPlugin.cpp)
    target_compile_definitions(coop_test_plugin_badabi PRIVATE COOP_TEST_BAD_ABI)
    add_dependencies(coop-tests coop_test_plugin coop_test_plugin_badabi coop_example_plugin)
endif()

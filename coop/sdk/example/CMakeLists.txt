# Builds the example plugin on its own, with the SDK folder that comes with the server:
#   sdk/coop_plugin.h  sdk/coop_plugin.hpp  sdk/nlohmann/json.hpp  sdk/example/ (this folder)
#
#   cmake -S example -B build
#   cmake --build build --config Release        -> build/Release/ejemplo-plugin.dll
#
# For your own plugin: copy this folder, change the name and the .cpp files, and keep the path to the SDK's .hpp.
cmake_minimum_required(VERSION 3.16)
project(ejemplo_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The same static runtime as the server: the DLL needs nothing else installed on the server's machine.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()

add_library(ejemplo-plugin SHARED ejemplo_plugin.cpp)
target_include_directories(ejemplo-plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..) # coop_plugin.hpp, nlohmann/
set_target_properties(ejemplo-plugin PROPERTIES PREFIX "")

# Compila el plugin de ejemplo por su cuenta, con la carpeta del SDK que acompaña al servidor:
#   sdk/coop_plugin.h  sdk/coop_plugin.hpp  sdk/nlohmann/json.hpp  sdk/example/ (esta carpeta)
#
#   cmake -S example -B build
#   cmake --build build --config Release        -> build/Release/ejemplo-plugin.dll
#
# Para tu propio plugin: copia esta carpeta, cambia el nombre y los .cpp, y deja la ruta a los .hpp del SDK.
cmake_minimum_required(VERSION 3.16)
project(ejemplo_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# El mismo runtime estático que el servidor: la DLL no necesita instalar nada más en la máquina del servidor.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()

add_library(ejemplo-plugin SHARED ejemplo_plugin.cpp)
target_include_directories(ejemplo-plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..) # coop_plugin.hpp, nlohmann/
set_target_properties(ejemplo-plugin PROPERTIES PREFIX "")

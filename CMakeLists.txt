cmake_minimum_required(VERSION 3.20)
project(CHEngine)

# Установка политик
cmake_policy(SET CMP0091 NEW) # Для MSVC_RUNTIME_LIBRARY

# Опции сборки
option(CHE_BUILD_SANDBOX "Build Sandbox project" ON)
option(CHE_BUILD_OPENGL "Build OpengGL render" ON)

# Директории исходников/интерфейсов/зависимостей
set(CHE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/src")
set(CHE_INTERFACES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Core/Interfaces")
set(CHE_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/vendor")

# Директории вывода
set(OUTPUT_DIR_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR_PREFIX})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR_PREFIX})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR_PREFIX})

#Директкория модулей движка
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Modules")

if(WIN32)
    set(OUTPUT_SYSTEM_PREFIX "-windows-x64")
    add_compile_definitions(CHE_PLATFORM_WINDOWS)
elseif(APPLE)
    set(OUTPUT_SYSTEM_PREFIX "-macos-x64")
    add_compile_definitions(CHE_PLATFORM_APPLE CHE_PLATFORM_UNIX)
elseif(UNIX)
    set(OUTPUT_SYSTEM_PREFIX "-linux-x64")
    add_compile_definitions(CHE_PLATFORM_LINUX CHE_PLATFORM_UNIX)
endif()

# Режимы сборки
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose build type: Debug, Release, Dist" FORCE)
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "" FORCE)

add_subdirectory(${CHE_VENDOR_DIR}/GLM)

# ImGui vendor path — used by RendererOGL to compile GLFW/OpenGL3 backends
set(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Core/vendor/imgui-docking/imgui" CACHE PATH "" FORCE)

# Добавление подпроектов
add_subdirectory(Core)      # Ядро
add_subdirectory(CHEngine)  # Основной движок

if(CHE_BUILD_OPENGL)
    add_subdirectory(${MODULES_DIR}/Rendering/RendererOGL)
endif()

if(CHE_BUILD_SANDBOX)
    add_subdirectory(Sandbox)  # Песочница
endif()

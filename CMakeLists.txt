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

# ImGui — объявляем FetchContent до RendererOGL
#include(FetchContent)
#FetchContent_Declare(
#    imgui_dep
#    GIT_REPOSITORY https://github.com/ocornut/imgui.git
#    GIT_TAG        v1.91.6
#)
#FetchContent_MakeAvailable(imgui_dep)
#set(IMGUI_INCLUDE_DIR "${imgui_dep_SOURCE_DIR}" CACHE PATH "" FORCE)
#

#

# imgui core как SHARED lib — только ядро, без GLFW/OpenGL бекендов
# Бекенды (impl_glfw, impl_opengl3) компилируются в RendererOGL.dylib,
# где уже есть GLFW и GLAD, чтобы избежать дублирования ObjC-классов.
#add_library(imgui SHARED
#    ${imgui_dep_SOURCE_DIR}/imgui.cpp
#    ${imgui_dep_SOURCE_DIR}/imgui_draw.cpp
#   ${imgui_dep_SOURCE_DIR}/imgui_tables.cpp
#    ${imgui_dep_SOURCE_DIR}/imgui_widgets.cpp
#    ${imgui_dep_SOURCE_DIR}/imgui_demo.cpp
#)
#target_include_directories(imgui PUBLIC
#    ${imgui_dep_SOURCE_DIR}
#    ${imgui_dep_SOURCE_DIR}/backends
#)
#
#set_target_properties(imgui PROPERTIES
#    POSITION_INDEPENDENT_CODE ON
#    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
#    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
#    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
#)
#
#add_custom_command(TARGET imgui POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy
#        $<TARGET_FILE:imgui>
#        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:imgui>"
#    COMMENT "Copying imgui to Sandbox directory"
#)

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

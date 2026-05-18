cmake_minimum_required(VERSION 3.20)
if(APPLE)
    project(CHEngine LANGUAGES C CXX OBJC OBJCXX)
else()
    project(CHEngine LANGUAGES C CXX)
endif()

# Установка политик
cmake_policy(SET CMP0091 NEW) # Для MSVC_RUNTIME_LIBRARY

# Опции сборки
option(CHE_BUILD_SANDBOX "Build Sandbox project" ON)
option(CHE_BUILD_PLAYER  "Build standalone Game Player" ON)
option(CHE_BUILD_OPENGL "Build OpengGL render" ON)
option(CHE_BUILD_VULKAN "Build Vulkan render" OFF)
option(CHE_BUILD_PHYSICS "Build PhysX physics module" ON)
option(CHE_BUILD_UI_NATIVE "Build native in-game UI (MSDF text, quad batching)" ON)
if(APPLE)
    option(CHE_BUILD_METAL "Build Metal render" ON)
endif()

# Директории исходников/интерфейсов/зависимостей
set(CHE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/src")
set(CHE_INTERFACES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Core/Interfaces")
set(CHE_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/vendor")
set(CHE_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Core/src")

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

add_subdirectory(${CHE_VENDOR_DIR}/lua)

# ImGui vendor path — used by ImGuiOGL to compile GLFW/OpenGL3 backends
set(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Core/vendor/imgui-docking/imgui" CACHE PATH "" FORCE)

# Shared GLFW — собирается как SHARED dylib, используется всеми модулями
# Это ключевое для разделения состояния GLFW между WindowGLFW и ImGuiOGL
set(GLFW_VENDOR_DIR "${MODULES_DIR}/Rendering/RendererOGL/vendor/GLFW")
set(GLAD_VENDOR_DIR "${MODULES_DIR}/Rendering/RendererOGL/vendor/GLAD")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE "SHARED" CACHE STRING "" FORCE)
add_subdirectory(${GLFW_VENDOR_DIR} vendors/GLFW)

# Shared GLAD — shared dylib, инициализируется один раз в RendererOGL,
# функциональные указатели GL видны во всех модулях через общий libglad.dylib
# GLAD: собирается как SHARED, vendor CMakeLists настроен с WINDOWS_EXPORT_ALL_SYMBOLS
# и корректными output директориями для всех платформ
add_subdirectory(${GLAD_VENDOR_DIR})

# FreeType + msdf-atlas-gen (нужны до CHEngine, чтобы таргеты были доступны)
if(CHE_BUILD_UI_NATIVE)
    # FreeType — только TTF/OTF, без PNG/zlib/harfbuzz/brotli
    set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
    add_subdirectory(${CHE_VENDOR_DIR}/freetype vendors/freetype)

    # msdf-atlas-gen: без vcpkg, без Skia, без PNG в msdfgen
    set(MSDF_ATLAS_USE_VCPKG          OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_USE_SKIA           OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_BUILD_STANDALONE   ON  CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_NO_ARTERY_FONT     OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_INSTALL            OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_DYNAMIC_RUNTIME    ON  CACHE BOOL "" FORCE)  # DLL CRT — согласовано с CHEngine SHARED
    set(MSDFGEN_DISABLE_SVG           ON  CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_PNG           ON  CACHE BOOL "" FORCE)
    set(MSDFGEN_BUILD_STANDALONE      OFF CACHE BOOL "" FORCE)
    # msdfgen-ext использует find_package(Freetype) — предоставляем наш таргет
    add_library(Freetype::Freetype ALIAS freetype)
    add_subdirectory(${CHE_VENDOR_DIR}/msdf-atlas-gen vendors/msdf-atlas-gen)
endif()

# Добавление подпроектов
add_subdirectory(Core)      # Ядро
add_subdirectory(CHEngine)  # Основной движок

# SlangBackend — общий wrapper над Slang. Линкуется PRIVATE только render-модулями;
# движок и Core про slang не знают. Подключаем до render-модулей.
if(CHE_BUILD_OPENGL OR CHE_BUILD_VULKAN OR CHE_BUILD_METAL)
    add_subdirectory(${MODULES_DIR}/Rendering/Common/SlangBackend)
endif()

if(CHE_BUILD_OPENGL)
    add_subdirectory(${MODULES_DIR}/Rendering/RendererOGL)
    add_subdirectory(${MODULES_DIR}/Window/WindowGLFW)
    add_subdirectory(${MODULES_DIR}/UI/ImGuiOGL)
endif()

if(CHE_BUILD_VULKAN)
    find_package(Vulkan REQUIRED)
    if(NOT CHE_BUILD_OPENGL)
        # WindowGLFW нужен и для Vulkan — GLFW поддерживает GLFW_NO_API
        add_subdirectory(${MODULES_DIR}/Window/WindowGLFW)
    endif()
    add_subdirectory(${MODULES_DIR}/Rendering/RendererVulkan)
    add_subdirectory(${MODULES_DIR}/UI/ImGuiVK)
endif()

if(CHE_BUILD_METAL AND APPLE)
    if(NOT CHE_BUILD_OPENGL AND NOT CHE_BUILD_VULKAN)
        add_subdirectory(${MODULES_DIR}/Window/WindowGLFW)
    endif()
    add_subdirectory(${MODULES_DIR}/Rendering/RendererMetal)
    add_subdirectory(${MODULES_DIR}/UI/ImGuiMTL)
endif()

if(CHE_BUILD_PHYSICS)
    if(WIN32 OR UNIX)
        if(APPLE)
            # Use o3de fork which has macOS ARM64/Intel support
            set(PHYSX_ROOT_DIR "${CHE_VENDOR_DIR}/PhysX/o3de_physx/physx" CACHE PATH "PhysX root path" FORCE)
            set(TARGET_BUILD_PLATFORM "mac" CACHE STRING "PhysX target platform" FORCE)
        else()
            # Windows and Linux: original NVIDIA PhysX SDK cmake
            set(PHYSX_ROOT_DIR "${CHE_VENDOR_DIR}/PhysX/physx" CACHE PATH "PhysX root path" FORCE)
            if(WIN32)
                set(TARGET_BUILD_PLATFORM "windows" CACHE STRING "PhysX target platform" FORCE)
            else()
                set(TARGET_BUILD_PLATFORM "linux" CACHE STRING "PhysX target platform" FORCE)
            endif()
        endif()

        set(PUBLIC_RELEASE 1 CACHE BOOL "Build PhysX as embedded dependency" FORCE)

        if(APPLE)
            # macOS (o3de fork): unified build — CMakeLists.txt lives OUTSIDE the submodule
            add_subdirectory(${CHE_VENDOR_DIR}/PhysX/build_physx_o3de vendors/PhysX)
        else()
            # Windows / Linux: original NVIDIA PhysX cmake (worked at commit a051c106)
            set(PX_BUILDSNIPPETS OFF CACHE BOOL "" FORCE)
            set(PX_BUILDPVDRUNTIME OFF CACHE BOOL "" FORCE)
            set(PX_GENERATE_GPU_PROJECTS OFF CACHE BOOL "" FORCE)
            set(PX_GENERATE_GPU_PROJECTS_ONLY OFF CACHE BOOL "" FORCE)
            set(PX_CMAKE_SUPPRESS_REGENERATION ON CACHE BOOL "" FORCE)
            set(NV_USE_DEBUG_WINCRT ON CACHE BOOL "" FORCE)
            set(PX_OUTPUT_LIB_DIR "${CMAKE_BINARY_DIR}/physx" CACHE PATH "PhysX output lib directory" FORCE)
            set(PX_OUTPUT_BIN_DIR "${CMAKE_BINARY_DIR}/physx" CACHE PATH "PhysX output bin directory" FORCE)
            list(APPEND CMAKE_MODULE_PATH "${PHYSX_ROOT_DIR}/source/compiler/cmake/modules")

            # PhysX rewrites configuration types in its own CMake.
            # Save and restore project configs to keep CHEngine's Dist profile.
            set(_CHE_SAVED_CONFIG_TYPES "${CMAKE_CONFIGURATION_TYPES}")
            set(_CHE_SAVED_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

            add_subdirectory(${PHYSX_ROOT_DIR}/source/compiler/cmake vendors/PhysX)

            set(CMAKE_CONFIGURATION_TYPES "${_CHE_SAVED_CONFIG_TYPES}" CACHE STRING "" FORCE)
            set(CMAKE_BUILD_TYPE "${_CHE_SAVED_BUILD_TYPE}" CACHE STRING "Choose build type: Debug, Release, Dist" FORCE)
        endif()

        set(CHE_PHYSX_TARGETS
            PhysXFoundation LowLevel LowLevelAABB LowLevelDynamics PhysX
            PhysXCharacterKinematic PhysXCommon PhysXCooking PhysXExtensions
            PhysXVehicle2 SceneQuery SimulationController PhysXPvdSDK PhysXTask
        )
        foreach(physx_target IN LISTS CHE_PHYSX_TARGETS)
            if(TARGET ${physx_target})
                get_target_property(_imported ${physx_target} IMPORTED)
                if(_imported)
                    continue()
                endif()
                get_target_property(_aliasof ${physx_target} ALIASED_TARGET)
                if(_aliasof)
                    set(_real_target ${_aliasof})
                else()
                    set(_real_target ${physx_target})
                endif()
                target_compile_definitions(${_real_target} PRIVATE
                    $<$<CONFIG:Debug>:_DEBUG;PX_DEBUG=1;PX_CHECKED=1;PX_SUPPORT_PVD=1;PX_SUPPORT_OMNI_PVD=1>
                    $<$<OR:$<CONFIG:Release>,$<CONFIG:Dist>>:NDEBUG;PX_SUPPORT_PVD=0;PX_SUPPORT_OMNI_PVD=0>
                )
            endif()
        endforeach()

        add_subdirectory(${MODULES_DIR}/Physics/PhysicsPhysX)
    else()
        message(WARNING "CHE_BUILD_PHYSICS is ON, but PhysX backend is unavailable on this platform.")
    endif()
else()
    message(STATUS "CHE_BUILD_PHYSICS is OFF, skipping PhysX")
endif()

if(CHE_BUILD_SANDBOX)
    add_subdirectory(Sandbox)
endif()

if(CHE_BUILD_PLAYER)
    add_subdirectory(Player)
endif()

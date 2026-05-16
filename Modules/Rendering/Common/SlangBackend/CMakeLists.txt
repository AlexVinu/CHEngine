# SlangBackend/CMakeLists.txt
# Shared library wrapping the Slang shader compiler. Linked PRIVATE only by
# render modules (RendererOGL, RendererVulkan, RendererMetal). The engine
# itself never sees Slang.
project(SlangBackend LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

include(FetchContent)

# ── Slang SDK ────────────────────────────────────────────────────────────────
# On Windows we use the prebuilt release binary to avoid building Slang from
# source (the source build has broken submodule paths in shallow clones).
# On other platforms we build from source as before.
set(_SLANG_VERSION "v2026.7.1")
set(_SLANG_VER_BARE "2026.7.1")

if(WIN32)
    # ── Windows: prebuilt binary ─────────────────────────────────────────────
    # Use file(DOWNLOAD) + file(ARCHIVE_EXTRACT) instead of FetchContent to
    # avoid FetchContent subbuild cmake projects which break when VS-bundled
    # cmake (3.31) is used for ZERO_CHECK regeneration instead of system cmake 4.x
    set(_SLANG_URL "https://github.com/shader-slang/slang/releases/download/${_SLANG_VERSION}/slang-${_SLANG_VER_BARE}-windows-x86_64.zip")
    set(_SLANG_ZIP "${CMAKE_BINARY_DIR}/slang-prebuilt.zip")
    set(_SLANG_ROOT "${CMAKE_BINARY_DIR}/slang-prebuilt")

    if(NOT EXISTS "${_SLANG_ROOT}/include/slang.h")
        if(NOT EXISTS "${_SLANG_ZIP}")
            message(STATUS "Downloading Slang prebuilt ${_SLANG_VER_BARE}...")
            file(DOWNLOAD "${_SLANG_URL}" "${_SLANG_ZIP}" SHOW_PROGRESS
                STATUS _dl_status TLS_VERIFY ON)
            list(GET _dl_status 0 _dl_code)
            if(NOT _dl_code EQUAL 0)
                list(GET _dl_status 1 _dl_err)
                message(FATAL_ERROR "Failed to download Slang: ${_dl_err}")
            endif()
        endif()
        message(STATUS "Extracting Slang prebuilt...")
        file(ARCHIVE_EXTRACT INPUT "${_SLANG_ZIP}" DESTINATION "${_SLANG_ROOT}")
    endif()

    add_library(slang SHARED IMPORTED GLOBAL)
    set_target_properties(slang PROPERTIES
        IMPORTED_IMPLIB             "${_SLANG_ROOT}/lib/slang.lib"
        IMPORTED_LOCATION           "${_SLANG_ROOT}/bin/slang.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${_SLANG_ROOT}/include"
    )

    set(_SLANG_INCLUDE_DIR "${_SLANG_ROOT}/include")

else()
    # ── macOS / Linux: build from source ────────────────────────────────────
    FetchContent_Declare(
        slang
        GIT_REPOSITORY https://github.com/shader-slang/slang.git
        GIT_TAG        ${_SLANG_VERSION}
        GIT_SHALLOW    TRUE
    )

    set(SLANG_ENABLE_TESTS        OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_EXAMPLES     OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_GFX          OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_SLANGD       OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_SLANGRT      OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_DXIL         OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_AGILITY_SDK  OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_D3D12        OFF CACHE BOOL "" FORCE)

    foreach(_che_slang_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generators/${_che_slang_config}")
    endforeach()
    if(CMAKE_BUILD_TYPE)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generators/${CMAKE_BUILD_TYPE}")
    endif()

    # Optional local DXC archive to avoid network issues with GitHub release assets.
    set(CHE_LOCAL_DXC_ARCHIVE "" CACHE FILEPATH
        "Path to local dxc_2026_02_20.zip archive for Slang FetchDXC override")
    set(_che_default_dxc_archive "${CMAKE_SOURCE_DIR}/third_party/cache/dxc_2026_02_20.zip")
    if(NOT CHE_LOCAL_DXC_ARCHIVE AND EXISTS "${_che_default_dxc_archive}")
        set(CHE_LOCAL_DXC_ARCHIVE "${_che_default_dxc_archive}")
    endif()
    if(CHE_LOCAL_DXC_ARCHIVE AND EXISTS "${CHE_LOCAL_DXC_ARCHIVE}" AND NOT DEFINED SLANG_DXC_BINARY_URL)
        file(TO_CMAKE_PATH "${CHE_LOCAL_DXC_ARCHIVE}" _che_local_dxc_archive_norm)
        set(SLANG_DXC_BINARY_URL "file:///${_che_local_dxc_archive_norm}"
            CACHE STRING "" FORCE)
        message(STATUS "Using local DXC archive: ${CHE_LOCAL_DXC_ARCHIVE}")
    endif()

    FetchContent_MakeAvailable(slang)
    set(_SLANG_INCLUDE_DIR "${slang_SOURCE_DIR}/include")
endif()
# ─────────────────────────────────────────────────────────────────────────────

add_library(${PROJECT_NAME} SHARED
    src/SlangBackend.cpp
    include/SlangBackend/SlangBackend.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CHE_BUILD_SLANG_BACKEND_DLL
    CHE_ENABLE_ASSERTS
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
    PRIVATE
        ${_SLANG_INCLUDE_DIR}
)

# CHEngine_CORE provides String, ERenderAPI (RenderData.h), and the export macros.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        CHEngine_CORE
    PRIVATE
        slang
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8 /W4
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Dist>:/O2>
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -fvisibility=hidden
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Dist>:-O2>
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
)

# Copy SlangBackend.dll and slang.dll next to Sandbox so they load at runtime.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:slang>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:slang>"
    COMMENT "Copying ${PROJECT_NAME} and slang to Sandbox directory"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

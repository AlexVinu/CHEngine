# SlangBackend/CMakeLists.txt
# Shared library wrapping the Slang shader compiler. Linked PRIVATE only by
# render modules (RendererOGL, RendererVulkan, RendererMetal). The engine
# itself never sees Slang.
project(SlangBackend LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

# ── Slang via FetchContent ───────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    GIT_TAG        v2026.7.1
    GIT_SHALLOW    TRUE
)

set(SLANG_ENABLE_TESTS    OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_GFX      OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGD   OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGRT  OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(slang)
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
        ${slang_SOURCE_DIR}/include
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

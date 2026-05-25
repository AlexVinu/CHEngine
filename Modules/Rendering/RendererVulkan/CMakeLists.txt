# RendererVulkan/CMakeLists.txt
if(NOT CHE_BUILD_VULKAN)
    return()
endif()

project(RendererVK LANGUAGES CXX)

# ── VulkanMemoryAllocator (header-only, single file) ─────────────────────────
set(_VMA_VERSION "v3.1.0")
set(_VMA_HEADER "${CMAKE_CURRENT_BINARY_DIR}/vma/vk_mem_alloc.h")

if(NOT EXISTS "${_VMA_HEADER}")
    message(STATUS "Downloading VulkanMemoryAllocator ${_VMA_VERSION}...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/refs/tags/${_VMA_VERSION}/include/vk_mem_alloc.h"
        "${_VMA_HEADER}"
        SHOW_PROGRESS TLS_VERIFY ON
        STATUS _vma_status)
    list(GET _vma_status 0 _vma_code)
    if(NOT _vma_code EQUAL 0)
        list(GET _vma_status 1 _vma_err)
        message(FATAL_ERROR "Failed to download VMA: ${_vma_err}")
    endif()
endif()
# ─────────────────────────────────────────────────────────────────────────────

file(GLOB CHE_SOURCES "src/*.cpp")
file(GLOB CHE_HEADERS "src/*.h")

add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8 /W4
        # VMA generates lots of warnings — suppress for the impl unit only via pragmas in code
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

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CHE_BUILD_MODULE_DLL
    CHE_ENABLE_ASSERTS
    NOMINMAX                          # prevent Windows.h min/max macros
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/vma"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CHEngine_CORE
        Vulkan::Vulkan
        glfw
        glm
        SlangBackend
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "Copying ${PROJECT_NAME} to Sandbox directory"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

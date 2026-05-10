# RendererMetal/CMakeLists.txt — macOS only
project(RendererMTL LANGUAGES CXX OBJCXX)

file(GLOB CHE_CPP_SOURCES "src/*.cpp")
file(GLOB CHE_MM_SOURCES  "src/*.mm")
file(GLOB CHE_HEADERS     "src/*.h")

add_library(${PROJECT_NAME} SHARED ${CHE_CPP_SOURCES} ${CHE_MM_SOURCES} ${CHE_HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# .mm файлы: пропустить PCH (конфликт ARC) и компилировать как Objective-C++
set_source_files_properties(${CHE_MM_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra
    -fvisibility=hidden
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Dist>:-O2>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CHE_BUILD_MODULE_DLL
    CHE_ENABLE_ASSERTS
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CHE_SRC_DIR}"   # access to CHEngine/src headers (UIFacade etc.)
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CHEngine_CORE
        CHEngine
        glfw
        glm
        SlangBackend
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Cocoa"
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

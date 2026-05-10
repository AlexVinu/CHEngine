cmake_minimum_required(VERSION 3.20)
project(glad)

add_library(glad SHARED)
target_sources(glad PRIVATE src/glad.c)
target_include_directories(glad PUBLIC include)

target_compile_definitions(glad PRIVATE
GLAD_GLAPI_EXPORT 
GLAD_GLAPI_EXPORT_BUILD)

if(MSVC)
    set_target_properties(glad PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

# Вывод DLL в общую bin-папку движка
set_target_properties(glad PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")
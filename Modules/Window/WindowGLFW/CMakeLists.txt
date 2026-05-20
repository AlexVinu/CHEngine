project(WindowGLFW LANGUAGES CXX)

file(GLOB_RECURSE CHE_SOURCES "src/*.cpp")
file(GLOB_RECURSE CHE_HEADERS "src/*.h")

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
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Dist>:/O2>
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
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
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CHE_INTERFACES_DIR}
    ${CHE_CORE_DIR}
)

# glfw и glad — shared libs из root CMakeLists
# Это ключевое: все модули разделяют одну libglfw.dylib и libglad.dylib
target_link_libraries(${PROJECT_NAME} PRIVATE
    CHEngine_CORE
    glfw
)

# macOS: нужен Cocoa для glfwGetCocoaWindow (Metal рендерер)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
    )
endif()

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

# Копируем libglfw в Sandbox, чтобы модули могли его найти в runtime
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:glfw>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:glfw>"
    COMMENT "Copying libglfw to Sandbox directory"
)

# Создаём симлинк для soname: платформенное расширение отличается
if(APPLE)
    # macOS: install name использует soname без patch-версии (libglfw.3.dylib)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            $<TARGET_FILE_NAME:glfw>
            "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/libglfw.3.dylib"
        COMMENT "Creating libglfw.3.dylib symlink (macOS)"
    )
elseif(UNIX)
    # Linux: soname — libglfw.so.3 (без patch-версии)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            $<TARGET_FILE_NAME:glfw>
            "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_SONAME_FILE_NAME:glfw>"
        COMMENT "Creating libglfw soname symlink (Linux)"
    )
endif()

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

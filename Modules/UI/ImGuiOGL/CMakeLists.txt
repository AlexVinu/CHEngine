project(ImGuiOGL LANGUAGES CXX)

file(GLOB_RECURSE CHE_SOURCES "src/*.cpp")
file(GLOB_RECURSE CHE_HEADERS "src/*.h")

add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS})

# imgui backends компилируются здесь: у нас есть shared GLFW и shared GLAD
target_sources(${PROJECT_NAME} PRIVATE
    ${IMGUI_INCLUDE_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_INCLUDE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /W4
        $<$<CONFIG:Debug>:/Od /Zi> $<$<CONFIG:Release>:/O2>)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra
        -fvisibility=hidden
        $<$<CONFIG:Debug>:-g -O0> $<$<CONFIG:Release>:-O2>)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CHE_BUILD_MODULE_DLL
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CHE_INTERFACES_DIR}
    ${CHE_CORE_DIR}
    ${IMGUI_INCLUDE_DIR}
)

# Линкуемся с shared glfw и shared glad — те же инстансы, что и в WindowGLFW
# Это гарантирует что imgui_impl_glfw видит то же GLFW-состояние (то же окно)
target_link_libraries(${PROJECT_NAME} PRIVATE
    CHEngine_CORE
    imgui
    glfw
    glad
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

# Копируем shared glad в Sandbox
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:glad>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:glad>"
    COMMENT "Copying libglad to Sandbox directory"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

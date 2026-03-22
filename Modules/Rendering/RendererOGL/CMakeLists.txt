# RendererOGL/CMakeLists.txt
project(RendererOGL LANGUAGES CXX)

# Берём только OGL-специфичные файлы (без ImGuiLayerOGL — он переехал в ImGuiOGL)
file(GLOB CHE_SOURCES "src/*.cpp")
file(GLOB CHE_HEADERS "src/*.h" "src/*.hpp" "src/*.inl")

# GLFW и GLAD — теперь shared libs из root CMakeLists, НЕ добавляем add_subdirectory
# imgui backends — переехали в ImGuiOGL модуль

add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS} "src/IsRenderAvailable.cpp" "src/IsRenderAvailable.h")

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
    GLAD_GLAPI_EXPORT
    CHE_ENABLE_ASSERTS
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CHEngine_CORE
        glad
        OpenGL::GL
        glm
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "Copying ${PROJECT_NAME} to Sandbox directory"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

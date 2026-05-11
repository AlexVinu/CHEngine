# Sandbox/CMakeLists.txt
project(Sandbox LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

# Исходники по подпапкам src/ (без рекурсивного ** — совместимо с CMake 3.20)
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*/*.cpp"
)

file(GLOB_RECURSE SANDBOX_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*/*.hpp"
)

set(SANDBOX_IMGUIZMO_SRC   "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo/ImGuizmo.cpp")
set(SANDBOX_TEXTEDITOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/TextEditor/TextEditor.cpp")

# Создание консольного приложения
add_executable(${PROJECT_NAME} ${SANDBOX_SOURCES} ${SANDBOX_HEADERS} ${SANDBOX_IMGUIZMO_SRC} ${SANDBOX_TEXTEDITOR_SRC})

# ImGuizmo/TextEditor don't work with PCH
set_source_files_properties(${SANDBOX_IMGUIZMO_SRC}   PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(${SANDBOX_TEXTEDITOR_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# ImGuizmo visibility
if(NOT MSVC)
    set_source_files_properties(${SANDBOX_IMGUIZMO_SRC}
        PROPERTIES COMPILE_FLAGS "-fvisibility=default"
    )
endif()

# Директории включения (плоские #include "…" внутри Sandbox)
set(SANDBOX_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CHE_CORE_DIR}
    ${CHE_SRC_DIR}
    ${MODULES_DIR}
    "${CHE_VENDOR_DIR}/spdlog/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/TextEditor"
    "${SANDBOX_SRC_ROOT}/App"
    "${SANDBOX_SRC_ROOT}/Core"
    "${SANDBOX_SRC_ROOT}/Editor"
    "${SANDBOX_SRC_ROOT}/SceneView"
    "${SANDBOX_SRC_ROOT}/Play"
    "${SANDBOX_SRC_ROOT}/UI"
    "${SANDBOX_SRC_ROOT}/UI/Panels"
)

# C++ стандарт и настройки
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Определения препроцессора
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

# Компиляторные флаги
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
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Dist>:-O2>
    )
endif()

# Подключение CHEngine
target_link_libraries(${PROJECT_NAME} PRIVATE
    CHEngine CHEngine_CORE
)

# libcurl для AI HTTP запросов
find_package(CURL)
if(CURL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHE_HAS_CURL)
else()
    find_library(CURL_LIB curl)
    if(CURL_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIB})
        target_compile_definitions(${PROJECT_NAME} PRIVATE CHE_HAS_CURL)
    endif()
endif()

# Линковка dl на Unix
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

# Настройка выходных директорий
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}"
)

# Копируем шейдеры в директорию с исполняемым файлом
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}/shaders"
    COMMENT "Copying shaders to output directory"
)

# Копируем editor defaults (default texture, fallback material и т.д.)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/editor_assets"
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}/editor_assets"
    COMMENT "Copying editor defaults to output directory"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

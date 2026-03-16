# Sandbox/CMakeLists.txt
project(Sandbox LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

# Автоматический поиск всех исходных файлов
file(GLOB_RECURSE SANDBOX_SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE SANDBOX_HEADERS
    "src/*.h"
    "src/*.hpp"
)

set(SANDBOX_IMGUIZMO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo/ImGuizmo.cpp")

# Создание консольного приложения
add_executable(${PROJECT_NAME} ${SANDBOX_SOURCES} ${SANDBOX_HEADERS} ${SANDBOX_IMGUIZMO_SRC})

# ImGuizmo doesn't work with PCH
set_source_files_properties(${SANDBOX_IMGUIZMO_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# ImGuizmo visibility
if(NOT MSVC)
    set_source_files_properties(${SANDBOX_IMGUIZMO_SRC}
        PROPERTIES COMPILE_FLAGS "-fvisibility=default"
    )
endif()

# Директории включения
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CHE_SRC_DIR}
    ${MODULES_DIR}
    "${CHE_VENDOR_DIR}/spdlog/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo"
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

# Копируем ассеты (шрифты и т.д.)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}/assets"
    COMMENT "Copying assets to output directory"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

# CHEngine/CMakeLists.txt
project(CHEngine_CORE LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

# Добавляем подпроект
add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/imgui-docking)

# Автоматический поиск всех исходных файлов
file(GLOB_RECURSE CHE_SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE CHE_HEADERS
    "src/*.h"
    "src/*.hpp"
)

# Создание динамической библиотеки
add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS} "src/Timestep.h" "src/EnumWrapper.h")

# Директории включения
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "Interfaces"
        "src"
        "vendor/spdlog/include"
)

target_precompile_headers(${PROJECT_NAME} PUBLIC
    src/chepch.h
)

# C++ стандарт и настройки
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
)

# Определения препроцессора
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CHE_BUILD_DLL
    CHE_BUILD_CORE_DLL
    CHE_ENABLE_ASSERTS
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
        -fvisibility=hidden
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Dist>:-O2>
    )
endif()

# Линковка dl на Unix
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

target_link_libraries(${PROJECT_NAME} 
    PUBLIC 
    imgui)

# Настройка выходных директорий
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

# RendererOGL/CMakeLists.txt
project(RendererOGL LANGUAGES CXX)

# Автоматический поиск всех исходных файлов
file(GLOB_RECURSE CHE_SOURCES 
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE CHE_HEADERS
    "src/*.h"
    "src/*.hpp"
    "src/*.inl"
)

add_subdirectory(vendor/GLAD)
add_subdirectory(vendor/GLFW)
# Создание динамической библиотеки
add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS}  "src/RenderFactoryOGL.h" "src/RenderFactoryOGL.cpp")

    # C++ стандарт и настройки
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )

# Флаги компиляции
target_compile_options(${PROJECT_NAME} PRIVATE
    /utf-8
    /W4
    $<$<CONFIG:Debug>:/Od /Zi>
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Dist>:/O2>
)

# Определения препроцессора
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CHE_BUILD_MODULE_DLL
    $<$<CONFIG:Debug>:CHE_DEBUG>
    $<$<CONFIG:Release>:CHE_RELEASE>
    $<$<CONFIG:Dist>:CHE_DIST>
)

# Настройка выходных директорий
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
)

# Подключаемые библиотеки
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        CHEngine_CORE
        glad
        
        glfw
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${PROJECT_NAME}>
    "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
COMMENT "Копирование DLL в целевую директорию"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")
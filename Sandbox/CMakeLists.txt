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

# Создание консольного приложения
add_executable(${PROJECT_NAME} ${SANDBOX_SOURCES} ${SANDBOX_HEADERS})

# Директории включения
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CHE_SRC_DIR}
    ${MODULES_DIR}
    "${CHE_VENDOR_DIR}/spdlog/include"
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
    CHEngine CHEngine_CORE glad glfw
)

# Линковка dl на Unix
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

# Настройка выходных директорий
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}"
)

message(STATUS "✅ ${PROJECT_NAME} configured successfully")

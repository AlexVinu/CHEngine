# CHEngine/CMakeLists.txt
project(CHEngine LANGUAGES CXX C)

message(STATUS "Configuring ${PROJECT_NAME}...")

# Автоматический поиск всех исходных файлов
file(GLOB_RECURSE CHE_SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE CHE_HEADERS
    "src/*.h"
    "src/*.hpp"
)

# Vendor sources (compiled into CHEngine)
set(CHE_VENDOR_SOURCES
    "${CHE_VENDOR_DIR}/tinyfiledialogs/tinyfiledialogs.c"
)

# Создание динамической библиотеки
add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS} ${CHE_VENDOR_SOURCES})

# Skip precompiled headers for vendor sources
set_source_files_properties(${CHE_VENDOR_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Disable strict warnings for vendor C file
set_source_files_properties(
    "${CHE_VENDOR_DIR}/tinyfiledialogs/tinyfiledialogs.c"
    PROPERTIES COMPILE_FLAGS "-w"
)
# Директории включения
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "src"
        ${IMGUI_INCLUDE_DIR}
        "${CHE_VENDOR_DIR}"
        ${CHE_INTERFACES_DIR}   # PUBLIC: Sandbox и другие клиенты видят <Input/KeyCodes.h> и т.д.
    PRIVATE
        ${CHE_CORE_DIR}
        "${CHE_VENDOR_DIR}/tinyobjloader"
        "${CHE_VENDOR_DIR}/tinygltf"
        "${CHE_VENDOR_DIR}/tinyfiledialogs"
        "${CHE_VENDOR_DIR}/nlohmann"
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

# Подключаемые библиотеки
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        CHEngine_CORE imgui glm 
)

# Линковка dl на Unix
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

# macOS frameworks (needed by tinyfiledialogs)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework AppKit")
endif()

# Windows system libs (needed by tinyfiledialogs: file dialogs, COM, shell)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        comdlg32 ole32 oleaut32 shell32 user32
    )
endif()

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

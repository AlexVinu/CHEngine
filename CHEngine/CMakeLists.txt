# CHEngine/CMakeLists.txt
project(CHEngine LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

# Добавляем GLFW как подпроект
#add_subdirectory(vendor/GLFW)
#add_subdirectory(vendor/GLAD)

# Автоматический поиск всех исходных файлов
file(GLOB_RECURSE CHE_SOURCES 
    "src/*.cpp"
    "src/*.cpp"
)

file(GLOB_RECURSE CHE_HEADERS
    "src/*.h"
    "src/*.hpp"
)

# Создание динамической библиотеки
add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES} ${CHE_HEADERS})

# Настройки Windows
if(WIN32)
    # Директории включения
    target_include_directories(${PROJECT_NAME} 
        PUBLIC
            "src"  # Основная директория с заголовками
        PRIVATE
            ${CHE_INTERFACES_DIR}
            ${CHE_CORE_DIR}
    )
    
    # C++ стандарт и настройки
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        PREFIX ""
    )
    
    if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
    endif()

    # Определения препроцессора
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CHE_BUILD_DLL
        CHE_ENABLE_ASSERTS
        $<$<CONFIG:Debug>:CHE_DEBUG>
        $<$<CONFIG:Release>:CHE_RELEASE>
        $<$<CONFIG:Dist>:CHE_DIST>
    )

    # Флаги компиляции
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8
        /W4
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Dist>:/O2>
    )

    # Подключаемые библиотеки
    target_link_libraries(${PROJECT_NAME}  
    PUBLIC
        CHEngine_CORE CHEngine_INTERFACES
    )
    
    # Настройка выходных директорий
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib"
    )

    set(DESTINATION_DIR "${CMAKE_BINARY_DIR}/")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "Копирование DLL в целевую директорию"
)
endif()

message(STATUS "✅ ${PROJECT_NAME} configured successfully")
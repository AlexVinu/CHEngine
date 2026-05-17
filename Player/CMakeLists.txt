project(CHEngine_Player LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME} (runtime player)...")

set(PLAYER_SOURCES
    src/PlayerApp.cpp
)

add_executable(${PROJECT_NAME} ${PLAYER_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "GamePlayer"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CHEngine
    CHEngine_CORE
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

# Output next to Sandbox for now; export tool will copy from here
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}"
)

# Copy all engine dylibs so the player is self-contained
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/Sandbox"
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}"
    COMMENT "Copying engine dylibs to Player directory"
)

# Copy shaders (player needs them too — or they'll be in the pak at export time)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../Sandbox/shaders"
        "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/${PROJECT_NAME}/shaders"
    COMMENT "Copying shaders to Player directory"
    VERBATIM
)

message(STATUS "✅ ${PROJECT_NAME} configured")

cmake_minimum_required(VERSION 3.16)
project(PhysX_Build C CXX)

# ─── Standalone PhysX build for macOS ──────────────────────────────────
set(PHYSX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../o3de_physx/physx")
set(PHYSX_INCLUDE_DIR "${PHYSX_SOURCE_DIR}/include")
set(PX_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../prebuilt/mac")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── Common compile flags ──────────────────────────────────────────────
set(COMMON_FLAGS "-fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -fvisibility=hidden")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Source files (excluding GPU, test, platform-specific) ────────────
function(physx_sources name_out)
    set(_all)
    foreach(_root ${ARGN})
        file(GLOB_RECURSE _srcs "${_root}/*.cpp")
        list(APPEND _all ${_srcs})
    endforeach()
    list(FILTER _all EXCLUDE REGEX "cuda|Cuda|gpu|Gpu|optix|OptiX|windows|Windows|WIN32|android|Android|ios|IOS|ExtParticleExt")
    set(${name_out} ${_all} PARENT_SCOPE)
endfunction()

# ─── Build each target ──────────────────────────────────────────────────
function(make_physx_lib name)
    physx_sources(srcs ${ARGN})
    if(NOT srcs)
        message(WARNING "No sources for ${name}, creating empty target")
        add_library(${name} SHARED IMPORTED GLOBAL)
        return()
    endif()
    add_library(${name} SHARED ${srcs})
    target_include_directories(${name}
        PUBLIC  ${PHYSX_INCLUDE_DIR}
        PRIVATE ${PHYSX_SOURCE_DIR}/source
    )
    target_compile_definitions(${name} PRIVATE
        $<$<CONFIG:Debug>:PX_DEBUG=1;PX_CHECKED=1;>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        PUBLIC_RELEASE=1
        PX_PHYSX_STATIC_LIB
        PX_PHYSX_FOUNDATION_EXPORTS
        PX_PHYSX_COMMON_EXPORTS
        PX_PHYSX_CORE_EXPORTS
    )
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        LIBRARY_OUTPUT_DIRECTORY "${PX_OUTPUT_DIR}/$<CONFIG>"
    )
endfunction()

# Collect include directories from all source subdirs
file(GLOB_RECURSE ALL_HEADERS ${PHYSX_SOURCE_DIR}/source/*.h ${PHYSX_SOURCE_DIR}/source/*.hpp)
foreach(h ${ALL_HEADERS})
    get_filename_component(h_dir ${h} DIRECTORY)
    list(APPEND ALL_INCLUDE_DIRS ${h_dir})
endforeach()
list(REMOVE_DUPLICATES ALL_INCLUDE_DIRS)

# Add include dirs as a global property
function(physx_target name)
    physx_sources(sources ${ARGN})
    if(NOT sources)
        add_library(${name} SHARED IMPORTED GLOBAL)
        return()
    endif()
    add_library(${name} SHARED ${sources})
    target_include_directories(${name}
        PUBLIC  ${PHYSX_INCLUDE_DIR}
                ${PHYSX_INCLUDE_DIR}/foundation
                ${PHYSX_INCLUDE_DIR}/foundation/unix
                ${PHYSX_INCLUDE_DIR}/foundation/unix/neon
        PRIVATE ${PHYSX_SOURCE_DIR}/source
                ${ALL_INCLUDE_DIRS}
    )
    target_compile_definitions(${name} PRIVATE
        $<$<CONFIG:Debug>:PX_DEBUG=1;PX_CHECKED=1>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        PUBLIC_RELEASE=1
        PX_PHYSX_STATIC_LIB=0
        PX_PHYSX_FOUNDATION_EXPORTS
        PX_PHYSX_COMMON_EXPORTS
        PX_PHYSX_CORE_EXPORTS
        PX_SUPPORT_GPU_PHYSX=0
        PX_PHYSX_GPU_EXPORTS=
        DISABLE_CUDA_PHYSX
    )
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        LIBRARY_OUTPUT_DIRECTORY "${PX_OUTPUT_DIR}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${PX_OUTPUT_DIR}/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${PX_OUTPUT_DIR}/$<CONFIG>"
    )
endfunction()

# ─── All PhysX in one shared library (avoids cross-component linker) ──
physx_target(PhysX
    ${PHYSX_SOURCE_DIR}/source/foundation
    ${PHYSX_SOURCE_DIR}/source/common/src
    ${PHYSX_SOURCE_DIR}/source/geomutils/src
    ${PHYSX_SOURCE_DIR}/source/task/src
    ${PHYSX_SOURCE_DIR}/source/lowlevel
    ${PHYSX_SOURCE_DIR}/source/lowlevelaabb/src
    ${PHYSX_SOURCE_DIR}/source/lowleveldynamics/src
    ${PHYSX_SOURCE_DIR}/source/lowleveldynamics/include
    ${PHYSX_SOURCE_DIR}/source/physx/src
    ${PHYSX_SOURCE_DIR}/source/physxcooking/src
    ${PHYSX_SOURCE_DIR}/source/physxextensions/src
    ${PHYSX_SOURCE_DIR}/source/scenequery/src
    ${PHYSX_SOURCE_DIR}/source/simulationcontroller/src
    ${PHYSX_SOURCE_DIR}/source/physxcharacterkinematic/src
    ${PHYSX_SOURCE_DIR}/source/physxvehicle/src
    ${PHYSX_SOURCE_DIR}/source/pvd/src
    ${PHYSX_SOURCE_DIR}/source/filebuf/src
    ${PHYSX_SOURCE_DIR}/source/physxmetadata
    ${PHYSX_SOURCE_DIR}/source/immediatemode/src
    ${PHYSX_SOURCE_DIR}/source/fastxml/src
)
set_target_properties(PhysX PROPERTIES OUTPUT_NAME "PhysXUnified")

# Create IMPORTED targets that all resolve to the same dylib.
# PhysicsPhysX links against specific target names like PhysXFoundation, etc.
# All these are now fulfilled by the unified library.
foreach(_n PhysXFoundation PhysXCommon PhysXTask LowLevel LowLevelAABB LowLevelDynamics PhysXCooking PhysXExtensions PhysXCharacterKinematic PhysXPvdSDK PhysXVehicle2)
    add_library(${_n} SHARED IMPORTED GLOBAL)
    set_target_properties(${_n} PROPERTIES
        IMPORTED_LOCATION "$<TARGET_FILE:PhysX>"
        IMPORTED_SONAME "$<TARGET_SONAME_FILE:PhysX>"
    )
    add_dependencies(${_n} PhysX)
endforeach()

message(STATUS "PhysX build configured for: ${PX_OUTPUT_DIR}")

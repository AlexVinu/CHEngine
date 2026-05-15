cmake_minimum_required(VERSION 3.16)

# ─── Unified PhysX build for macOS (o3de fork) ────────────────────────────
set(O3DE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../o3de_physx/physx")
set(PHYSX_SRC "${O3DE_ROOT}/source")

# Recursively find ALL .cpp files, excluding GPU/Windows/etc.
function(_px_srcs outvar)
    set(_all)
    foreach(_d ${ARGN})
        file(GLOB_RECURSE _s "${_d}/*.cpp")
        list(APPEND _all ${_s})
    endforeach()
    list(FILTER _all EXCLUDE REGEX "cuda|Cuda|gpu|Gpu|optix|OptiX|windows|Windows|WIN32|android|Android|ios|IOS|ExtParticleExt")
    set(${outvar} ${_all} PARENT_SCOPE)
endfunction()

# Collect all source directories for include path
file(GLOB_RECURSE _ALL_H ${PHYSX_SRC}/*.h ${PHYSX_SRC}/*.hpp)
foreach(_h ${_ALL_H})
    get_filename_component(_hd ${_h} DIRECTORY)
    list(APPEND _INC ${_hd})
endforeach()
list(REMOVE_DUPLICATES _INC)
list(LENGTH _INC _INC_COUNT)

# Create unified PhysX library
_px_srcs(PX_SOURCES
    ${PHYSX_SRC}/foundation
    ${PHYSX_SRC}/common/src
    ${PHYSX_SRC}/geomutils/src
    ${PHYSX_SRC}/task/src
    ${PHYSX_SRC}/lowlevel
    ${PHYSX_SRC}/lowlevelaabb/src
    ${PHYSX_SRC}/lowleveldynamics/src
    ${PHYSX_SRC}/lowleveldynamics/include
    ${PHYSX_SRC}/physx/src
    ${PHYSX_SRC}/physxcooking/src
    ${PHYSX_SRC}/physxextensions/src
    ${PHYSX_SRC}/scenequery/src
    ${PHYSX_SRC}/simulationcontroller/src
    ${PHYSX_SRC}/physxcharacterkinematic/src
    ${PHYSX_SRC}/physxvehicle/src
    ${PHYSX_SRC}/pvd/src
    ${PHYSX_SRC}/filebuf/src
    ${PHYSX_SRC}/physxmetadata
    ${PHYSX_SRC}/immediatemode/src
    ${PHYSX_SRC}/fastxml/src
)

list(LENGTH PX_SOURCES PX_SRC_COUNT)
message(STATUS "PhysX: ${_INC_COUNT} include directories, ${PX_SRC_COUNT} source files")

add_library(PhysX SHARED ${PX_SOURCES})
target_include_directories(PhysX
    PUBLIC  ${O3DE_ROOT}/include
            ${O3DE_ROOT}/include/foundation
            ${O3DE_ROOT}/include/foundation/unix
            ${O3DE_ROOT}/include/foundation/unix/neon
    PRIVATE ${PHYSX_SRC}
            ${_INC}
            ${O3DE_ROOT}/pvdruntime/include
            ${PHYSX_SRC}/common/src
            ${PHYSX_SRC}/geomutils/src
            ${PHYSX_SRC}/physxextensions/src
            ${PHYSX_SRC}/physxcooking/src
            ${PHYSX_SRC}/physxcharacterkinematic/src
            ${PHYSX_SRC}/scenequery/src
            ${PHYSX_SRC}/simulationcontroller/src
            ${PHYSX_SRC}/lowleveldynamics/src
            ${PHYSX_SRC}/lowleveldynamics/include
            ${PHYSX_SRC}/lowlevel/api/include
)
target_compile_definitions(PhysX PRIVATE
    $<$<CONFIG:Debug>:PX_DEBUG=1;PX_CHECKED=1>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    PUBLIC_RELEASE=1
    PX_PHYSX_FOUNDATION_EXPORTS
    PX_PHYSX_COMMON_EXPORTS
    PX_PHYSX_CORE_EXPORTS
    PX_SUPPORT_GPU_PHYSX=0
    DISABLE_CUDA_PHYSX
)
set_target_properties(PhysX PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Alias targets for PhysicsPhysX module compatibility
foreach(_t PhysXFoundation PhysXCommon PhysXTask LowLevel LowLevelAABB
           LowLevelDynamics PhysXCooking PhysXExtensions PhysXCharacterKinematic
           PhysXPvdSDK PhysXVehicle2 SceneQuery SimulationController)
    add_library(${_t} ALIAS PhysX)
endforeach()

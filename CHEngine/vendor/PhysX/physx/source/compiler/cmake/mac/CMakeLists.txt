## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##  * Neither the name of NVIDIA CORPORATION nor the names of its
##    contributors may be used to endorse or promote products derived
##    from this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
## Copyright (c) 2008-2025 NVIDIA Corporation. All rights reserved.

STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWERCASE)

#TODO: Fix warnings
SET(CLANG_WARNINGS "-ferror-limit=0 -Wall -Wextra -Werror -Weverything\
    -Wno-unused-but-set-variable \
    -Wno-switch-default \
    -Wno-cast-qual \
    -Wno-invalid-offsetof \
    -Wno-unsafe-buffer-usage \
    -Wno-alloca \
    -Wno-atomic-implicit-seq-cst \
    -Wno-c++98-compat-pedantic \
    -Wno-c++98-compat \
    -Wno-cast-align \
    -Wno-conversion \
    -Wno-covered-switch-default \
    -Wno-deprecated \
    -Wno-documentation-deprecated-sync \
    -Wno-documentation-unknown-command \
    -Wno-exit-time-destructors \
    -Wno-extra-semi-stmt \
    -Wno-float-equal \
    -Wno-format-nonliteral \
    -Wno-global-constructors \
    -Wno-implicit-fallthrough \
    -Wno-inconsistent-missing-destructor-override \
    -Wno-inconsistent-missing-override \
    -Wno-missing-noreturn \
    -Wno-missing-prototypes \
    -Wno-missing-variable-declarations \
    -Wno-newline-eof \
    -Wno-non-virtual-dtor \
    -Wno-old-style-cast \
    -Wno-padded \
    -Wno-reserved-id-macro \
    -Wno-suggest-destructor-override \
    -Wno-suggest-override \
    -Wno-switch-enum \
    -Wno-undef \
    -Wno-undefined-reinterpret-cast \
    -Wno-unknown-warning-option \
    -Wno-unreachable-code \
    -Wno-unused-function \
    -Wno-unused-macros \
    -Wno-unused-member-function \
    -Wno-unused-private-field \
    -Wno-used-but-marked-unused \
    -Wno-weak-template-vtables \
    -Wno-weak-vtables \
    -Wno-zero-as-null-pointer-constant \
    -Wno-reserved-identifier \
    -Wno-undefined-func-template \
")

SET(GCC_WARNINGS "-Wall -Werror \
	-Wno-address \
	-Wno-aligned-new \
	-Wno-array-bounds \
	-Wno-class-memaccess \
	-Wno-conversion-null \
	-Wno-format \
	-Wno-format-overflow \
	-Wno-invalid-offsetof \
	-Wno-misleading-indentation \
	-Wno-mismatched-new-delete \
	-Wno-nonnull \
	-Wno-nonnull-compare \
	-Wno-pragmas \
	-Wno-restrict \
	-Wno-stringop-overflow \
	-Wno-stringop-overread \
	-Wno-subobject-linkage \
	-Wno-template-id-cdtor \
	-Wno-uninitialized \
	-Wno-unused-but-set-variable \
	-Wno-unused-function \
	-Wno-unused-result \
	-Wno-unknown-pragmas \
	-Wno-use-after-free \
")

# Define PhysX source directory tree
SET(PHYSX_SOURCE_DIR "${PHYSX_ROOT_DIR}/source")
SET(LL_SOURCE_DIR "${PHYSX_SOURCE_DIR}/foundation")
SET(PX_SOURCE_DIR "${PHYSX_SOURCE_DIR}/physx/src")

# macOS always uses Apple Clang on both x86_64 and arm64
SET(AARCH64_FLAGS "")
SET(COMMON_CXX_FLAGS "-std=c++14 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -fvisibility=hidden")

# Apple Clang flags
IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	SET(PHYSX_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS} -fstrict-aliasing -Wstrict-aliasing=2 ${CLANG_WARNINGS}" CACHE INTERNAL "PhysX CXX")
ELSE()
	SET(PHYSX_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS}" CACHE INTERNAL "PhysX CXX")
ENDIF()

IF (PX_USE_MOLD_LINKER)
	SET (PHYSX_CXX_FLAGS "${PHYSX_CXX_FLAGS} -Wno-unused-command-line-argument -fuse-ld=mold " CACHE INTERNAL "PhysX CXX")
ENDIF()

# Build debug info for all configurations
SET(PHYSX_CXX_FLAGS_DEBUG   "-O0 -g" CACHE INTERNAL "PhysX Debug CXX Flags")
SET(PHYSX_CXX_FLAGS_CHECKED "-O2 -g" CACHE INTERNAL "PhysX Checked CXX Flags")
SET(PHYSX_CXX_FLAGS_PROFILE "-O2" CACHE INTERNAL "PhysX Profile CXX Flags")
SET(PHYSX_CXX_FLAGS_RELEASE "-O2" CACHE INTERNAL "PhysX Release CXX Flags")

# These flags are local to the directory the CMakeLists.txt is in, so don't get carried over to OTHER CMakeLists.txt (thus the CACHE variables above)
SET(CMAKE_CXX_FLAGS ${PHYSX_CXX_FLAGS})

SET(CMAKE_CXX_FLAGS_DEBUG   ${PHYSX_CXX_FLAGS_DEBUG})
SET(CMAKE_CXX_FLAGS_CHECKED ${PHYSX_CXX_FLAGS_CHECKED})
SET(CMAKE_CXX_FLAGS_PROFILE ${PHYSX_CXX_FLAGS_PROFILE})
SET(CMAKE_CXX_FLAGS_RELEASE ${PHYSX_CXX_FLAGS_RELEASE})

IF(PUBLIC_RELEASE)
	SET(PUBLIC_RELEASE_FLAG "PX_PUBLIC_RELEASE=1")
ELSE()
	SET(PUBLIC_RELEASE_FLAG "PX_PUBLIC_RELEASE=0")
ENDIF()

# Controls PX_NVTX for all projects
SET(NVTX_FLAG "PX_NVTX=0")
IF(${CMAKE_CROSSCOMPILING} AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
	SET(PX_SUPPORT_OMNI_PVD_FLAG "PX_SUPPORT_OMNI_PVD=0")
ELSE()
	SET(PX_SUPPORT_OMNI_PVD_FLAG "PX_SUPPORT_OMNI_PVD=1")
ENDIF()

# Set DISABLE_CUDA_PHYSX when GPU projects are not enabled
IF(NOT PX_GENERATE_GPU_PROJECTS_ONLY AND NOT PX_GENERATE_GPU_PROJECTS)
	# CMake automatically adds the appropriate prefix (-D for GCC/Clang) when the definition
	# is added to the target via target_compile_definitions or add_definitions
	SET(DISABLE_CUDA_DEF "DISABLE_CUDA_PHYSX")
ENDIF()

SET(PHYSX_MAC_COMPILE_DEFS "${PHYSX_AUTOBUILD};${PUBLIC_RELEASE_FLAG};${DISABLE_CUDA_DEF}" CACHE INTERNAL "Base PhysX preprocessor definitions")
SET(PHYSX_MAC_DEBUG_COMPILE_DEFS   "PX_DEBUG=1;PX_CHECKED=1;${NVTX_FLAG};PX_SUPPORT_PVD=1;${PX_SUPPORT_OMNI_PVD_FLAG}"  CACHE INTERNAL "Debug PhysX preprocessor definitions")
SET(PHYSX_MAC_CHECKED_COMPILE_DEFS "NDEBUG;PX_CHECKED=1;${NVTX_FLAG};PX_SUPPORT_PVD=1;${PX_SUPPORT_OMNI_PVD_FLAG}" CACHE INTERNAL "Checked PhysX preprocessor definitions")
SET(PHYSX_MAC_PROFILE_COMPILE_DEFS "NDEBUG;PX_PROFILE=1;${NVTX_FLAG};PX_SUPPORT_PVD=1;${PX_SUPPORT_OMNI_PVD_FLAG}"  CACHE INTERNAL "Profile PhysX preprocessor definitions")
SET(PHYSX_MAC_RELEASE_COMPILE_DEFS "NDEBUG;PX_SUPPORT_PVD=0;PX_SUPPORT_OMNI_PVD=0" CACHE INTERNAL "Release PhysX preprocessor definitions")

# IF(PUBLIC_RELEASE)
# 	IF(NOT PHYSX_PHYSXGPU_PATH)
# 		SET(PHYSX_PHYSXGPU_PATH "$ENV{PM_PhysXGpu_PATH}/bin" CACHE INTERNAL "PhysXGpu copy path")
# 	ENDIF()
# 	IF(NOT GPU_LIB_COPIED)
# 		IF(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
# 			SET(GPU_LIB_COPIED 1 CACHE INTERNAL "PhysX GPU so files copied")
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.aarch64/checked/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_DEBUG})
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.aarch64/checked/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_CHECKED})
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.aarch64/profile/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_PROFILE})
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.aarch64/release/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_RELEASE})
# 			INSTALL(PROGRAMS ${PHYSX_PHYSXGPU_PATH}/linux.aarch64/$<$<CONFIG:debug>:checked>$<$<CONFIG:release>:release>$<$<CONFIG:checked>:checked>$<$<CONFIG:profile>:profile>/libPhysXGpu_64.so
# 				DESTINATION $<$<CONFIG:debug>:${PX_ROOT_LIB_DIR}/debug>$<$<CONFIG:release>:${PX_ROOT_LIB_DIR}/release>$<$<CONFIG:checked>:${PX_ROOT_LIB_DIR}/checked>$<$<CONFIG:profile>:${PX_ROOT_LIB_DIR}/profile> OPTIONAL)
# 		ELSEIF(UNIX)
# 			SET(GPU_LIB_COPIED 1 CACHE INTERNAL "PhysX GPU so files copied")
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.x86_64/checked/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_DEBUG})
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.x86_64/checked/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_CHECKED})
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.x86_64/profile/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_PROFILE})
# 			FILE(COPY ${PHYSX_PHYSXGPU_PATH}/linux.x86_64/release/libPhysXGpu_64.so DESTINATION ${PX_EXE_OUTPUT_DIRECTORY_RELEASE})
# 			INSTALL(PROGRAMS ${PHYSX_PHYSXGPU_PATH}/linux.x86_64/$<$<CONFIG:debug>:checked>$<$<CONFIG:release>:release>$<$<CONFIG:checked>:checked>$<$<CONFIG:profile>:profile>/libPhysXGpu_64.so
# 				DESTINATION $<$<CONFIG:debug>:${PX_ROOT_LIB_DIR}/debug>$<$<CONFIG:release>:${PX_ROOT_LIB_DIR}/release>$<$<CONFIG:checked>:${PX_ROOT_LIB_DIR}/checked>$<$<CONFIG:profile>:${PX_ROOT_LIB_DIR}/profile> OPTIONAL)
# 		ENDIF()
# 	ENDIF()
# ENDIF()

# Include all of the projects — defines source variables
# Note: INCLUDE without path searches CMAKE_CURRENT_SOURCE_DIR (source/compiler/cmake/).
# We explicitly reference the mac/ subdirectory to get the platform-specific files.
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXFoundation.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXCommon.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXTask.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/LowLevel.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/LowLevelAABB.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/LowLevelDynamics.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/SceneQuery.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/SimulationController.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysX.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXCharacterKinematic.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXCooking.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXExtensions.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXVehicle.cmake)
INCLUDE(${CMAKE_CURRENT_LIST_DIR}/PhysXPvdSDK.cmake)

# ── GLOB source file helper ───────────────────────────────────────────────
macro(_physx_sources name_out)
    file(GLOB_RECURSE ${name_out}
        ${ARGN}
    )
    # Remove GPU-related files (CUDA, OptiX)
    list(FILTER ${name_out} EXCLUDE REGEX "cuda|Cuda|gpu|Gpu|optix|OptiX|omnipvd|OmniPvd|windows|Windows|WIN32|_WIN32")
endmacro()

# ── Target creation ──────────────────────────────────────────────────────
function(_create_physx_target name libtype sources includes compile_defs link_libs)
    add_library(${name} ${libtype} ${sources})
    # Collect all PhysX source directories for header resolution
    # (PhysX #includes headers by relative name from the source tree)
    if(NOT DEFINED _PHYSX_ALL_SRC_DIRS)
        set(_PHYSX_ALL_SRC_DIRS)
        file(GLOB_RECURSE _ALL_PHYSX_DIRS
            ${PHYSX_SOURCE_DIR}/*/
        )
        foreach(_d ${_ALL_PHYSX_DIRS})
            get_filename_component(_dn "${_d}" REALPATH)
            list(APPEND _PHYSX_ALL_SRC_DIRS "${_dn}")
        endforeach()
        set(_PHYSX_ALL_SRC_DIRS ${_PHYSX_ALL_SRC_DIRS} CACHE INTERNAL "")
    endif()
    target_include_directories(${name}
        PUBLIC  ${PHYSX_ROOT_DIR}/include
                ${PHYSX_ROOT_DIR}/include/foundation
                ${PHYSX_ROOT_DIR}/include/foundation/unix
                ${PHYSX_ROOT_DIR}/include/foundation/unix/neon
        PRIVATE ${PHYSX_SOURCE_DIR}
                ${PHYSX_SOURCE_DIR}/lowlevel/api/include
                ${_PHYSX_ALL_SRC_DIRS}
                ${includes}
    )
    if(compile_defs)
        target_compile_definitions(${name} PRIVATE ${compile_defs})
    endif()
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )
    if(link_libs)
        target_link_libraries(${name} PRIVATE ${link_libs})
    endif()
endfunction()

# ── Sources ──────────────────────────────────────────────────────────────
_physx_sources(FOUNDATION_SRC
    ${PHYSX_SOURCE_DIR}/foundation/*.cpp
    ${PHYSX_SOURCE_DIR}/foundation/unix/*.cpp
    ${PHYSX_SOURCE_DIR}/foundation/include/*.cpp
)
_physx_sources(TASK_SRC ${PHYSX_SOURCE_DIR}/task/src/*.cpp)
_physx_sources(LOWLEVEL_SRC
    ${PHYSX_SOURCE_DIR}/lowlevel/software/src/*.cpp
    ${PHYSX_SOURCE_DIR}/lowlevel/common/src/**/*.cpp
)
_physx_sources(LOWLEVELAABB_SRC ${PHYSX_SOURCE_DIR}/lowlevelaabb/src/*.cpp)
_physx_sources(LOWLEVELDYNAMICS_SRC ${PHYSX_SOURCE_DIR}/lowleveldynamics/src/*.cpp)
_physx_sources(SCENEQUERY_SRC ${PHYSX_SOURCE_DIR}/scenequery/src/*.cpp)
_physx_sources(SIMULATION_SRC ${PHYSX_SOURCE_DIR}/simulationcontroller/src/*.cpp)
_physx_sources(PHYSX_SRC ${PHYSX_SOURCE_DIR}/physx/src/**/*.cpp)
_physx_sources(CKINEMATIC_SRC ${PHYSX_SOURCE_DIR}/physxcharacterkinematic/src/*.cpp)
_physx_sources(COOKING_SRC
    ${PHYSX_SOURCE_DIR}/physxcooking/src/*.cpp
    ${PHYSX_SOURCE_DIR}/physxcooking/src/*/*.cpp
)
_physx_sources(COMMON_SRC
    ${PHYSX_SOURCE_DIR}/common/src/*.cpp
    ${PHYSX_SOURCE_DIR}/geomutils/src/**/*.cpp
)
_physx_sources(EXTENSIONS_SRC ${PHYSX_SOURCE_DIR}/physxextensions/src/**/*.cpp)
_physx_sources(VEHICLE_SRC ${PHYSX_SOURCE_DIR}/physxvehicle/src/**/*.cpp)
_physx_sources(PVD_SRC ${PHYSX_SOURCE_DIR}/pvd/src/*.cpp)
_physx_sources(TASK_SRC ${PHYSX_SOURCE_DIR}/task/src/*.cpp)

# ── Target creation ──────────────────────────────────────────────────────
_create_physx_target(PhysXFoundation ${PHYSXFOUNDATION_LIBTYPE}
    "${FOUNDATION_SRC}"
    "${PHYSXFOUNDATION_PLATFORM_INCLUDES}"
    "${PHYSXFOUNDATION_COMPILE_DEFS}"
    "${PHYSXFOUNDATION_PLATFORM_LINKED_LIBS}"
)

_create_physx_target(PhysXCommon ${PHYSXCOMMON_LIBTYPE}
    "${COMMON_SRC}"
    "${PXCOMMON_PLATFORM_INCLUDES}"
    "${PXCOMMON_COMPILE_DEFS}"
    "PhysXFoundation"
)

_create_physx_target(PhysXTask ${PHYSXTASK_LIBTYPE}
    "${TASK_SRC}"
    ""
    "${PHYSXTASK_COMPILE_DEFS}"
    "PhysXFoundation"
)

_create_physx_target(LowLevel ${LOWLEVEL_LIBTYPE}
    "${LOWLEVEL_SRC}"
    "${LOWLEVEL_PLATFORM_INCLUDES}"
    "${LOWLEVEL_COMPILE_DEFS}"
    "PhysXFoundation"
)

_create_physx_target(LowLevelAABB ${LOWLEVELAABB_LIBTYPE}
    "${LOWLEVELAABB_SRC}"
    "${LOWLEVELAABB_PLATFORM_INCLUDES}"
    "${LOWLEVELAABB_COMPILE_DEFS}"
    "PhysXFoundation"
)

_create_physx_target(LowLevelDynamics ${LOWLEVELDYNAMICS_LIBTYPE}
    "${LOWLEVELDYNAMICS_SRC}"
    "${LOWLEVELDYNAMICS_PLATFORM_INCLUDES}"
    "${LOWLEVELDYNAMICS_COMPILE_DEFS}"
    "PhysXFoundation"
)

_create_physx_target(SceneQuery ${SCENEQUERY_LIBTYPE}
    "${SCENEQUERY_SRC}"
    "${SCENEQUERY_PLATFORM_INCLUDES}"
    "${SCENEQUERY_COMPILE_DEFS}"
    "PhysXFoundation;PhysXCommon"
)

_create_physx_target(SimulationController ${SIMULATIONCONTROLLER_LIBTYPE}
    "${SIMULATION_SRC}"
    "${SIMULATIONCONTROLLER_PLATFORM_INCLUDES}"
    "${SIMULATIONCONTROLLER_COMPILE_DEFS}"
    "PhysXFoundation;PhysXCommon"
)

_create_physx_target(PhysX ${PHYSX_LIBTYPE}
    "${PHYSX_SRC}"
    "${PHYSX_PLATFORM_INCLUDES}"
    "${PHYSX_COMPILE_DEFS}"
    "PhysXFoundation;PhysXCommon;PhysXTask;LowLevel;LowLevelAABB;LowLevelDynamics;SceneQuery;SimulationController"
)

_create_physx_target(PhysXCharacterKinematic ${PHYSXCHARACTERKINEMATIC_LIBTYPE}
    "${CKINEMATIC_SRC}"
    ""
    "${PHYSXCHARACTERKINEMATICS_COMPILE_DEFS}"
    "PhysX;PhysXFoundation;PhysXCommon"
)

_create_physx_target(PhysXCooking ${PHYSXCOOKING_LIBTYPE}
    "${COOKING_SRC}"
    ""
    "${PHYSXCOOKING_COMPILE_DEFS}"
    "PhysXCommon;PhysXFoundation"
)

_create_physx_target(PhysXExtensions ${PHYSXEXTENSIONS_LIBTYPE}
    "${EXTENSIONS_SRC}"
    "${PHYSXEXTENSIONS_PLATFORM_INCLUDES}"
    "${PHYSXEXTENSIONS_COMPILE_DEFS}"
    "PhysX;PhysXFoundation;PhysXCommon"
)

_create_physx_target(PhysXVehicle2 ${PHYSXVEHICLE_LIBTYPE}
    "${VEHICLE_SRC}"
    ""
    "${PHYSXVEHICLE_COMPILE_DEFS}"
    "PhysXExtensions;PhysX;PhysXFoundation;PhysXCommon"
)

_create_physx_target(PhysXPvdSDK ${PHYSXPVDSDK_LIBTYPE}
    "${PVD_SRC}"
    "${PHYSXPVDSDK_PLATFORM_INCLUDES}"
    "${PHYSXPVDSDK_COMPILE_DEFS}"
    "PhysXFoundation"
)

# Folder organization
foreach(t PhysX PhysXCharacterKinematic PhysXCommon PhysXCooking PhysXExtensions
          PhysXVehicle2 LowLevel LowLevelAABB LowLevelDynamics SceneQuery
          SimulationController PhysXPvdSDK PhysXTask PhysXFoundation)
    if(TARGET ${t})
        set_target_properties(${t} PROPERTIES FOLDER "PhysX SDK")
    endif()
endforeach()

SET(PHYSXDISTRO_LIBS PhysXFoundation PhysX PhysXCharacterKinematic PhysXPvdSDK
    PhysXCommon PhysXCooking PhysXExtensions PhysXVehicle2)

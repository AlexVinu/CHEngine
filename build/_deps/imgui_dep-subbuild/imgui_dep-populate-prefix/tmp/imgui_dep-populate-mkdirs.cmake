# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-src")
  file(MAKE_DIRECTORY "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-src")
endif()
file(MAKE_DIRECTORY
  "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-build"
  "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix"
  "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix/tmp"
  "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix/src/imgui_dep-populate-stamp"
  "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix/src"
  "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix/src/imgui_dep-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix/src/imgui_dep-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/marat/Downloads/CHEngine-main-2/build/_deps/imgui_dep-subbuild/imgui_dep-populate-prefix/src/imgui_dep-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

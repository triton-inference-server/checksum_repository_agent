# Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.17)

project(tritonchecksumrepoagent LANGUAGES C CXX)

#
# Options
#
# Must include options required for this project as well as any
# projects included in this one by FetchContent.
#
set(TRITON_COMMON_REPO_TAG "main" CACHE STRING "Tag for triton-inference-server/common repo")
set(TRITON_CORE_REPO_TAG "main" CACHE STRING "Tag for triton-inference-server/core repo")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#
# Dependencies
#
# FetchContent's composability isn't very good. We must include the
# transitive closure of all repos so that we can override the tag.
#
include(FetchContent)

FetchContent_Declare(
  repo-common
  GIT_REPOSITORY https://github.com/triton-inference-server/common.git
  GIT_TAG ${TRITON_COMMON_REPO_TAG}
  GIT_SHALLOW ON
)
FetchContent_Declare(
  repo-core
  GIT_REPOSITORY https://github.com/triton-inference-server/core.git
  GIT_TAG ${TRITON_CORE_REPO_TAG}
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(repo-common repo-core)

#
# Shared library implementing the Triton Backend API
#
configure_file(src/libtritonrepoagent_checksum.ldscript libtritonrepoagent_checksum.ldscript COPYONLY)

add_library(
  triton-checksum-repoagent SHARED
  src/checksum.cc
)

add_library(
  TritonChecksumRepoAgent::triton-checksum-repoagent ALIAS triton-checksum-repoagent
)

target_include_directories(
  triton-checksum-repoagent
  PUBLIC
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(triton-checksum-repoagent PRIVATE cxx_std_11)
target_compile_options(
  triton-checksum-repoagent PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wno-unused-parameter -Wno-type-limits -Werror>
)

target_link_libraries(
  triton-checksum-repoagent
  PRIVATE
    triton-core-serverapi   # from repo-core
    triton-core-repoagentapi   # from repo-core
    triton-core-serverstub  # from repo-core
)

set_target_properties(
  triton-checksum-repoagent PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME tritonrepoagent_checksum
  LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libtritonrepoagent_checksum.ldscript
  LINK_FLAGS "-Wl,--version-script libtritonrepoagent_checksum.ldscript"
)

#
# Install
#
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/TritonChecksumRepoAgent)

install(
  TARGETS
    triton-checksum-repoagent
  EXPORT
    triton-checksum-repoagent-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/repoagents/checksum
  ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/repoagents/checksum
)

install(
  EXPORT
    triton-checksum-repoagent-targets
  FILE
    TritonChecksumRepoAgentTargets.cmake
  NAMESPACE
    TritonChecksumRepoAgent::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/TritonChecksumRepoAgentConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TritonChecksumRepoAgentConfig.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/TritonChecksumRepoAgentConfig.cmake
  DESTINATION ${INSTALL_CONFIGDIR}
)

#
# Export from build tree
#
export(
  EXPORT triton-checksum-repoagent-targets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/TritonChecksumRepoAgentTargets.cmake
  NAMESPACE TritonChecksumRepoAgent::
)

export(PACKAGE TritonChecksumRepoAgent)

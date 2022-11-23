## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set(COMPONENT_NAME embree)

if (BUILD_EMBREE_FROM_SOURCE)
  string(REGEX REPLACE "(^[0-9]+\.[0-9]+\.[0-9]+$)" "v\\1" EMBREE_ARCHIVE ${EMBREE_VERSION})
  set(EMBREE_BRANCH "${EMBREE_ARCHIVE}" CACHE STRING "Which branch of Embree to build" )
  set(EMBREE_URL "https://github.com/embree/embree/archive/${EMBREE_BRANCH}.zip"
    CACHE STRING "Location to clone Embree source from")
endif()

set(COMPONENT_PATH ${INSTALL_DIR_ABSOLUTE})
if (INSTALL_IN_SEPARATE_DIRECTORIES)
  set(COMPONENT_PATH ${INSTALL_DIR_ABSOLUTE}/${COMPONENT_NAME})
endif()

if (EMBREE_HASH)
  set(EMBREE_URL_HASH URL_HASH SHA256=${EMBREE_HASH})
endif()

if (BUILD_EMBREE_FROM_SOURCE)
  string(REGEX MATCH ".*\.zip$" ZIP_FILENAME ${EMBREE_URL})
  if (ZIP_FILENAME)
    set(EMBREE_CLONE_URL URL ${EMBREE_URL})
  else()
    set(EMBREE_CLONE_URL GIT_REPOSITORY ${EMBREE_URL} GIT_TAG ${EMBREE_BRANCH})
  endif()

  ExternalProject_Add(${COMPONENT_NAME}
    PREFIX ${COMPONENT_NAME}
    DOWNLOAD_DIR ${COMPONENT_NAME}
    STAMP_DIR ${COMPONENT_NAME}/stamp
    SOURCE_DIR ${COMPONENT_NAME}/src
    BINARY_DIR ${COMPONENT_NAME}/build
    LIST_SEPARATOR | # Use the alternate list separator
    ${EMBREE_CLONE_URL}
    ${EMBREE_URL_HASH}
    CMAKE_ARGS
      -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
      -DCMAKE_INSTALL_PREFIX:PATH=${COMPONENT_PATH}
      -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
      -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
      -DCMAKE_INSTALL_DOCDIR=${CMAKE_INSTALL_DOCDIR}
      -DCMAKE_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
      $<$<BOOL:${DOWNLOAD_TBB}>:-DEMBREE_TBB_ROOT=${TBB_PATH}>
      $<$<BOOL:${DOWNLOAD_ISPC}>:-DEMBREE_ISPC_EXECUTABLE=${ISPC_PATH}>
      -DCMAKE_BUILD_TYPE=${DEPENDENCIES_BUILD_TYPE}
      -DEMBREE_TUTORIALS=OFF
      -DBUILD_TESTING=OFF
      -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DEMBREE_ISA_SSE42=${BUILD_ISA_SSE4}
      -DEMBREE_ISA_AVX=${BUILD_ISA_AVX}
      -DEMBREE_ISA_AVX2=${BUILD_ISA_AVX2}
      -DEMBREE_ISA_AVX512=${BUILD_ISA_AVX512}
      -DEMBREE_ISA_NEON=${BUILD_ISA_NEON}
      # No dpcpp compiler in CI right now
      -DEMBREE_SYCL_SUPPORT=${BUILD_GPU_SUPPORT}
      # WA for bug
      -DEMBREE_SYCL_IMPLICIT_DISPATCH_GLOBALS=OFF
      # Maybe none as the default?
      -DEMBREE_SYCL_AOT_DEVICES=dg2-b0
      -DEMBREE_FILTER_FUNCTION_IN_GEOMETRY=OFF
      -DEMBREE_GEOMETRY_USER_IN_GEOMETRY=OFF
    BUILD_COMMAND ${DEFAULT_BUILD_COMMAND}
    BUILD_ALWAYS ${ALWAYS_REBUILD}
  )

  if (DOWNLOAD_TBB)
    ExternalProject_Add_StepDependencies(${COMPONENT_NAME} configure tbb)
  endif()
  if (DOWNLOAD_ISPC)
    ExternalProject_Add_StepDependencies(${COMPONENT_NAME} configure ispc)
  endif()
else()

  if (APPLE)
    set(EMBREE_OSSUFFIX "x86_64.macosx.zip")
  elseif (WIN32)
    set(EMBREE_OSSUFFIX "x64.vc14.windows.zip")
  else()
    set(EMBREE_OSSUFFIX "x86_64.linux.tar.gz")
  endif()
  set(EMBREE_URL "https://github.com/embree/embree/releases/download/v${EMBREE_VERSION}/embree-${EMBREE_VERSION}.${EMBREE_OSSUFFIX}")

  ExternalProject_Add(${COMPONENT_NAME}
    PREFIX ${COMPONENT_NAME}
    DOWNLOAD_DIR ${COMPONENT_NAME}
    STAMP_DIR ${COMPONENT_NAME}/stamp
    SOURCE_DIR ${COMPONENT_NAME}/src
    BINARY_DIR ${COMPONENT_NAME}
    URL ${EMBREE_URL}
    ${EMBREE_URL_HASH}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_directory
      <SOURCE_DIR>/
      ${COMPONENT_PATH}
    BUILD_ALWAYS OFF
  )

endif()

list(APPEND CMAKE_PREFIX_PATH ${COMPONENT_PATH})
string(REPLACE ";" "|" CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")

# flexlib as CMake targets: flexlib::utils, flexlib::math, flexlib::dsp and
# flexlib::nn.
#
#     add_subdirectory(lib/flexlib)
#     target_link_libraries(app PRIVATE flexlib::dsp)  # brings math and utils
#
# Your toolchain and MCU flags apply. Set FLEXLIB_REQUIRE (e.g. 0.6.0) before
# add_subdirectory to stop the configure when this copy is older, and
# FLEXLIB_DEFINES for the build options (FU_CRC_SMALL, FM_LUT_IN_RAM, ...).

cmake_minimum_required(VERSION 3.16)

# the version from arch/fx_version.h
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/arch/fx_version.h" _flexlib_version
     REGEX "^#define FLEXLIB_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(_part MAJOR MINOR PATCH)
  string(REGEX MATCH "FLEXLIB_VERSION_${_part} ([0-9]+)" _ "${_flexlib_version}")
  set(_flexlib_${_part} ${CMAKE_MATCH_1})
endforeach()

project(flexlib
        VERSION ${_flexlib_MAJOR}.${_flexlib_MINOR}.${_flexlib_PATCH}
        LANGUAGES C)

if(DEFINED FLEXLIB_REQUIRE AND PROJECT_VERSION VERSION_LESS FLEXLIB_REQUIRE)
  message(FATAL_ERROR "flexlib ${PROJECT_VERSION} in ${PROJECT_SOURCE_DIR} "
                      "is older than FLEXLIB_REQUIRE ${FLEXLIB_REQUIRE}")
endif()

# a plain variable: a cache one would hide the parent's set() (CMP0126)
if(NOT DEFINED FLEXLIB_DEFINES)
  set(FLEXLIB_DEFINES "")
endif()

function(_flexlib_add name dir)
  file(GLOB _srcs CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${dir}/src/*.c")
  add_library(flexlib_${name} STATIC ${_srcs})
  add_library(flexlib::${name} ALIAS flexlib_${name})
  target_include_directories(flexlib_${name} PUBLIC
                             "${PROJECT_SOURCE_DIR}/arch"
                             "${PROJECT_SOURCE_DIR}/${dir}/include")
  target_compile_features(flexlib_${name} PUBLIC c_std_11)
  target_compile_definitions(flexlib_${name} PUBLIC ${FLEXLIB_DEFINES})
endfunction()

_flexlib_add(utils utils)
_flexlib_add(math math)
_flexlib_add(dsp dsp)
_flexlib_add(nn nn)
target_link_libraries(flexlib_math PUBLIC flexlib_utils)
target_link_libraries(flexlib_dsp PUBLIC flexlib_math)
target_link_libraries(flexlib_nn PUBLIC flexlib_math)

# A project using flexlib through CMake (make cmake-check): on the host it
# builds and runs the consumer test's program, cross-compiled it builds the
# libraries.

cmake_minimum_required(VERSION 3.16)
project(flexlib_cmake_consumer C)

if(DEFINED CONSUMER_DEFINES)
  set(FLEXLIB_DEFINES ${CONSUMER_DEFINES})
endif()
add_subdirectory("${FLEXLIB_DIR}" flexlib)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(app "${CMAKE_CURRENT_LIST_DIR}/../consumer/main.c")
  target_link_libraries(app PRIVATE flexlib::dsp)
endif()

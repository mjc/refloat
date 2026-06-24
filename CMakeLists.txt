cmake_minimum_required(VERSION 3.25)

project(
  RefloatHostTests
  VERSION 0.0.0
  LANGUAGES C CXX
)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)

option(REFLOAT_BUILD_TESTS "Build Refloat host tests" ON)
option(REFLOAT_BUILD_PACKAGE "Expose CMake targets for the package build path" OFF)
option(REFLOAT_MINIFY_QML "Minify the generated package QML" ON)
option(REFLOAT_STRICT_WARNINGS "Treat supported host-test warnings as errors" ON)
option(REFLOAT_ENABLE_COVERAGE "Enable host-test coverage instrumentation" OFF)

if(REFLOAT_BUILD_TESTS)
  enable_testing()
  include(cmake/RefloatHostTests.cmake)
endif()

cmake_minimum_required(VERSION 3.25)

project(
  Refloat
  LANGUAGES C CXX
)

option(REFLOAT_STRICT_WARNINGS "Treat supported host-test warnings as errors" ON)

include(CTest)

if(BUILD_TESTING)
  include("${CMAKE_CURRENT_LIST_DIR}/cmake/RefloatHostTests.cmake")
endif()

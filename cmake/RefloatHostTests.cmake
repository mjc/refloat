include_guard(GLOBAL)

find_package(Catch2 3 REQUIRED)
find_package(Git QUIET)
include(Catch)
find_program(REFLOAT_MAKE_EXECUTABLE NAMES gmake make REQUIRED)
find_program(
  REFLOAT_VESC_TOOL_EXECUTABLE
  NAMES vesc_tool
  REQUIRED
  DOC "Path to vesc_tool used to generate Refloat config sources"
)

get_filename_component(REFLOAT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(REFLOAT_SRC_DIR "${REFLOAT_ROOT}/src")
set(REFLOAT_TEST_DIR "${REFLOAT_ROOT}/tests")

function(refloat_append_git_path out_var git_path)
  if(NOT Git_FOUND)
    return()
  endif()

  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${REFLOAT_ROOT}" rev-parse --git-path "${git_path}"
    OUTPUT_VARIABLE _refloat_git_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _refloat_git_path_result
    ERROR_QUIET
  )

  if(NOT _refloat_git_path_result EQUAL 0 OR _refloat_git_path STREQUAL "")
    return()
  endif()

  if(IS_ABSOLUTE "${_refloat_git_path}")
    set(_refloat_git_abs_path "${_refloat_git_path}")
  else()
    cmake_path(
      ABSOLUTE_PATH _refloat_git_path
      BASE_DIRECTORY "${REFLOAT_ROOT}"
      NORMALIZE
      OUTPUT_VARIABLE _refloat_git_abs_path
    )
  endif()

  if(EXISTS "${_refloat_git_abs_path}")
    set(${out_var} ${${out_var}} "${_refloat_git_abs_path}" PARENT_SCOPE)
  endif()
endfunction()

add_library(refloat_host_c_options INTERFACE)
target_compile_definitions(refloat_host_c_options INTERFACE _GNU_SOURCE IS_VESC_LIB)
target_include_directories(
  refloat_host_c_options
  INTERFACE
    "${REFLOAT_TEST_DIR}"
    "${REFLOAT_TEST_DIR}/stubs"
    "${REFLOAT_SRC_DIR}"
    "${REFLOAT_SRC_DIR}/lib"
    "${REFLOAT_SRC_DIR}/filters"
    "${REFLOAT_ROOT}/vesc_pkg_lib"
)
target_compile_options(
  refloat_host_c_options
  INTERFACE
    "$<$<COMPILE_LANGUAGE:C>:-include>"
    "$<$<COMPILE_LANGUAGE:C>:${REFLOAT_TEST_DIR}/vesc_if_fake.h>"
    -Wall
    -Wextra
    "$<$<BOOL:${REFLOAT_STRICT_WARNINGS}>:-Werror>"
    "$<$<COMPILE_LANGUAGE:C>:-Wno-error=array-bounds>"
    "$<$<COMPILE_LANGUAGE:C>:-Wno-error=pointer-to-int-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-Wno-error=maybe-uninitialized>"
)
target_link_libraries(refloat_host_c_options INTERFACE m)

add_library(refloat_host_cpp_options INTERFACE)
target_compile_definitions(refloat_host_cpp_options INTERFACE _GNU_SOURCE IS_VESC_LIB)
target_include_directories(
  refloat_host_cpp_options
  INTERFACE
    "${REFLOAT_TEST_DIR}/cpp/support"
)
target_compile_options(
  refloat_host_cpp_options
  INTERFACE
    # Refloat has src/time.h for firmware builds.  Use -iquote for project
    # headers so quoted includes find Refloat code without shadowing <time.h>.
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-iquote ${REFLOAT_TEST_DIR}>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-iquote ${REFLOAT_TEST_DIR}/stubs>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-iquote ${REFLOAT_SRC_DIR}>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-iquote ${REFLOAT_SRC_DIR}/lib>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-iquote ${REFLOAT_SRC_DIR}/filters>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-iquote ${REFLOAT_ROOT}/vesc_pkg_lib>"
    -Wall
    -Wextra
    "$<$<BOOL:${REFLOAT_STRICT_WARNINGS}>:-Werror>"
)

set(REFLOAT_GENERATED_CONF_DEPS)
refloat_append_git_path(REFLOAT_GENERATED_CONF_DEPS HEAD)
refloat_append_git_path(REFLOAT_GENERATED_CONF_DEPS packed-refs)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${REFLOAT_ROOT}" symbolic-ref -q HEAD
    OUTPUT_VARIABLE _refloat_git_ref
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _refloat_git_ref_result
    ERROR_QUIET
  )
  if(_refloat_git_ref_result EQUAL 0 AND NOT _refloat_git_ref STREQUAL "")
    refloat_append_git_path(REFLOAT_GENERATED_CONF_DEPS "${_refloat_git_ref}")
  endif()
endif()

set(REFLOAT_GENERATED_CONF
  "${REFLOAT_SRC_DIR}/conf/conf_default.h"
  "${REFLOAT_SRC_DIR}/conf/conf_general.h"
  "${REFLOAT_SRC_DIR}/conf/confparser.c"
  "${REFLOAT_SRC_DIR}/conf/confparser.h"
  "${REFLOAT_SRC_DIR}/conf/confxml.c"
  "${REFLOAT_SRC_DIR}/conf/confxml.h"
)
set(REFLOAT_GENERATED_CONF_STAMP "${CMAKE_CURRENT_BINARY_DIR}/refloat_generated_conf.stamp")

add_custom_command(
  OUTPUT "${REFLOAT_GENERATED_CONF_STAMP}"
  BYPRODUCTS ${REFLOAT_GENERATED_CONF}
  COMMAND "${REFLOAT_MAKE_EXECUTABLE}" -C "${REFLOAT_SRC_DIR}"
          "VESC_TOOL=${REFLOAT_VESC_TOOL_EXECUTABLE}"
          conf/conf_default.h
          conf/conf_general.h
          conf/confparser.c
          conf/confparser.h
          conf/confxml.c
          conf/confxml.h
  COMMAND "${CMAKE_COMMAND}" -E touch "${REFLOAT_GENERATED_CONF_STAMP}"
  DEPENDS
    "${REFLOAT_SRC_DIR}/conf/settings.xml"
    "${REFLOAT_SRC_DIR}/conf/conf_general.h.in"
    "${REFLOAT_ROOT}/package_name"
    "${REFLOAT_ROOT}/version"
    ${REFLOAT_GENERATED_CONF_DEPS}
    "${REFLOAT_SRC_DIR}/Makefile"
    "${REFLOAT_ROOT}/vesc_pkg_lib/rules.mk"
  COMMENT "Generating config sources with the existing Make target"
  VERBATIM
)
add_custom_target(refloat_generated_conf DEPENDS "${REFLOAT_GENERATED_CONF_STAMP}")

function(refloat_apply_host_c_options target)
  set_target_properties(
    ${target}
    PROPERTIES
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS ON
  )
  target_link_libraries(${target} PRIVATE refloat_host_c_options)
endfunction()

add_library(refloat_config_parser STATIC)
target_sources(
  refloat_config_parser
  PRIVATE
    "${REFLOAT_SRC_DIR}/conf/buffer.c"
    "${REFLOAT_SRC_DIR}/conf/confparser.c"
    "${REFLOAT_SRC_DIR}/conf/confxml.c"
)
refloat_apply_host_c_options(refloat_config_parser)
add_dependencies(refloat_config_parser refloat_generated_conf)

add_library(refloat_vesc_fake STATIC "${REFLOAT_TEST_DIR}/vesc_if_fake.c")
refloat_apply_host_c_options(refloat_vesc_fake)

add_library(refloat_lcm_under_test STATIC)
target_sources(
  refloat_lcm_under_test
  PRIVATE
    "${REFLOAT_SRC_DIR}/conf/buffer.c"
    "${REFLOAT_SRC_DIR}/lib/utils.c"
    "${REFLOAT_SRC_DIR}/state.c"
    "${REFLOAT_SRC_DIR}/lcm.c"
    "${REFLOAT_TEST_DIR}/lcm_fakes.c"
)
refloat_apply_host_c_options(refloat_lcm_under_test)
add_dependencies(refloat_lcm_under_test refloat_generated_conf)

function(refloat_add_catch2_test target)
  set(options)
  set(oneValueArgs PREFIX)
  set(multiValueArgs SOURCES LIBRARIES LABELS)
  cmake_parse_arguments(REFLOAT_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT REFLOAT_TEST_PREFIX)
    message(FATAL_ERROR "refloat_add_catch2_test(${target}) requires PREFIX")
  endif()
  if(NOT REFLOAT_TEST_SOURCES)
    message(FATAL_ERROR "refloat_add_catch2_test(${target}) requires SOURCES")
  endif()

  add_executable(${target})
  set_target_properties(
    ${target}
    PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
  )
  target_sources(${target} PRIVATE ${REFLOAT_TEST_SOURCES})
  target_link_libraries(
    ${target}
    PRIVATE
      refloat_host_cpp_options
      Catch2::Catch2WithMain
      ${REFLOAT_TEST_LIBRARIES}
  )
  catch_discover_tests(
    ${target}
    TEST_PREFIX "${REFLOAT_TEST_PREFIX}."
    PROPERTIES LABELS "${REFLOAT_TEST_LABELS}"
  )
endfunction()

refloat_add_catch2_test(
  refloat-generated-config-tests
  PREFIX generated-config
  SOURCES "${REFLOAT_TEST_DIR}/cpp/config/generated_config_parser_test.cpp"
  LIBRARIES refloat_config_parser
  LABELS "host;cpp;generated-config"
)

refloat_add_catch2_test(
  refloat-lcm-tests
  PREFIX lcm
  SOURCES "${REFLOAT_TEST_DIR}/cpp/lcm/lcm_test.cpp"
  LIBRARIES
    refloat_lcm_under_test
    refloat_vesc_fake
  LABELS "host;cpp;lcm"
)

add_custom_target(
  refloat_host_tests
  DEPENDS
    refloat-generated-config-tests
    refloat-lcm-tests
)

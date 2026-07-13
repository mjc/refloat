include_guard(GLOBAL)

find_package(Catch2 3 REQUIRED)
find_package(Git REQUIRED)
find_program(REFLOAT_VESC_TOOL_EXECUTABLE NAMES vesc_tool REQUIRED)
include(Catch)

file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/.." REFLOAT_ROOT)
set(REFLOAT_SRC_DIR "${REFLOAT_ROOT}/src")
set(REFLOAT_TEST_DIR "${REFLOAT_ROOT}/tests")
set(REFLOAT_GENERATED_CONF_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(REFLOAT_GENERATED_CONF_SRC_DIR "${REFLOAT_GENERATED_CONF_ROOT}/src")
set(REFLOAT_GENERATED_CONF_DIR "${REFLOAT_GENERATED_CONF_SRC_DIR}/conf")

function(refloat_git_path out_var git_path)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${REFLOAT_ROOT}" rev-parse --git-path "${git_path}"
    OUTPUT_VARIABLE _refloat_git_rel_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )

  if(IS_ABSOLUTE "${_refloat_git_rel_path}")
    set(_refloat_git_abs_path "${_refloat_git_rel_path}")
  else()
    file(REAL_PATH "${REFLOAT_ROOT}/${_refloat_git_rel_path}" _refloat_git_abs_path)
  endif()

  set(${out_var} "${_refloat_git_abs_path}" PARENT_SCOPE)
endfunction()

refloat_git_path(REFLOAT_GIT_HEAD HEAD)
execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${REFLOAT_ROOT}" symbolic-ref -q HEAD
  OUTPUT_VARIABLE REFLOAT_GIT_REF
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

set(REFLOAT_GIT_DEPS "${REFLOAT_GIT_HEAD}")
if(REFLOAT_GIT_REF)
  refloat_git_path(REFLOAT_GIT_REF_FILE "${REFLOAT_GIT_REF}")
  list(APPEND REFLOAT_GIT_DEPS "${REFLOAT_GIT_REF_FILE}")
endif()
refloat_git_path(REFLOAT_GIT_PACKED_REFS packed-refs)
list(APPEND REFLOAT_GIT_DEPS "${REFLOAT_GIT_PACKED_REFS}")

set(REFLOAT_GENERATED_CONF_INPUTS
  "${REFLOAT_SRC_DIR}/conf/settings.xml"
  "${REFLOAT_SRC_DIR}/conf/conf_general.h.in"
  "${REFLOAT_ROOT}/package_name"
  "${REFLOAT_ROOT}/version"
  ${REFLOAT_GIT_DEPS}
)
set(REFLOAT_GENERATED_CONF_OUTPUTS
  "${REFLOAT_GENERATED_CONF_DIR}/conf_default.h"
  "${REFLOAT_GENERATED_CONF_DIR}/confparser.h"
  "${REFLOAT_GENERATED_CONF_DIR}/confxml.h"
  "${REFLOAT_GENERATED_CONF_DIR}/conf_general.h"
  "${REFLOAT_GENERATED_CONF_DIR}/confparser.c"
  "${REFLOAT_GENERATED_CONF_DIR}/confxml.c"
)

add_library(refloat_host_base INTERFACE)
target_compile_definitions(refloat_host_base INTERFACE _GNU_SOURCE IS_VESC_LIB)
target_include_directories(
  refloat_host_base
  INTERFACE
    "${REFLOAT_TEST_DIR}"
    "${REFLOAT_TEST_DIR}/stubs"
    "${REFLOAT_GENERATED_CONF_SRC_DIR}"
    "${REFLOAT_SRC_DIR}/conf"
    "${REFLOAT_SRC_DIR}/lib"
    "${REFLOAT_SRC_DIR}/filters"
    "${REFLOAT_ROOT}/vesc_pkg_lib"
)
target_link_libraries(refloat_host_base INTERFACE m)

if(REFLOAT_ENABLE_COVERAGE)
  target_compile_options(refloat_host_base INTERFACE --coverage)
  target_link_options(refloat_host_base INTERFACE --coverage)
endif()

add_library(refloat_host_c_options INTERFACE)
target_link_libraries(refloat_host_c_options INTERFACE refloat_host_base)
target_compile_features(refloat_host_c_options INTERFACE c_std_11)
target_include_directories(refloat_host_c_options INTERFACE "$<$<COMPILE_LANGUAGE:C>:${REFLOAT_SRC_DIR}>")
target_compile_options(
  refloat_host_c_options
  INTERFACE
    "$<$<COMPILE_LANGUAGE:C>:-std=gnu11>"
    "$<$<COMPILE_LANGUAGE:C>:-include>"
    "$<$<COMPILE_LANGUAGE:C>:${REFLOAT_TEST_DIR}/vesc_if_fake.h>"
    -Wall
    -Wextra
    "$<$<BOOL:${REFLOAT_STRICT_WARNINGS}>:-Werror>"
    "$<$<COMPILE_LANGUAGE:C>:-Wno-error=array-bounds>"
    "$<$<COMPILE_LANGUAGE:C>:-Wno-error=pointer-to-int-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-Wno-error=maybe-uninitialized>"
)

add_library(refloat_host_cpp_options INTERFACE)
target_link_libraries(refloat_host_cpp_options INTERFACE refloat_host_base)
target_compile_features(refloat_host_cpp_options INTERFACE cxx_std_20)
target_compile_options(
  refloat_host_cpp_options
  INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-iquote>"
    "$<$<COMPILE_LANGUAGE:CXX>:${REFLOAT_SRC_DIR}>"
)
target_include_directories(refloat_host_cpp_options INTERFACE "${REFLOAT_TEST_DIR}/cpp/support")
target_compile_options(
  refloat_host_cpp_options
  INTERFACE
    -Wall
    -Wextra
    "$<$<BOOL:${REFLOAT_STRICT_WARNINGS}>:-Werror>"
)

function(refloat_add_cpp_test target test_name)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs SOURCES LINK_LIBRARIES LABELS COMPILE_DEFINITIONS DEPENDS)
  cmake_parse_arguments(REFLOAT_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT REFLOAT_TEST_SOURCES)
    message(FATAL_ERROR "refloat_add_cpp_test(${target}) requires SOURCES")
  endif()

  add_executable(${target})
  target_sources(${target} PRIVATE ${REFLOAT_TEST_SOURCES})
  target_link_libraries(
    ${target}
    PRIVATE
      refloat_host_cpp_options
      Catch2::Catch2WithMain
      ${REFLOAT_TEST_LINK_LIBRARIES}
  )
  if(REFLOAT_TEST_COMPILE_DEFINITIONS)
    target_compile_definitions(${target} PRIVATE ${REFLOAT_TEST_COMPILE_DEFINITIONS})
  endif()
  add_dependencies(${target} refloat_generated_conf ${REFLOAT_TEST_DEPENDS})
  catch_discover_tests(
    ${target}
    TEST_PREFIX "${test_name}."
    PROPERTIES LABELS "${REFLOAT_TEST_LABELS}"
  )
endfunction()

add_custom_command(
  OUTPUT ${REFLOAT_GENERATED_CONF_OUTPUTS}
  COMMAND
    "${CMAKE_COMMAND}"
    -DREFLOAT_ROOT=${REFLOAT_ROOT}
    -DREFLOAT_SRC_DIR=${REFLOAT_SRC_DIR}
    -DREFLOAT_CONF_OUTPUT_DIR=${REFLOAT_GENERATED_CONF_DIR}
    -DVESC_TOOL_EXECUTABLE=${REFLOAT_VESC_TOOL_EXECUTABLE}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -P "${CMAKE_CURRENT_LIST_DIR}/RefloatGeneratePackageConf.cmake"
  DEPENDS ${REFLOAT_GENERATED_CONF_INPUTS}
  COMMENT "Generating Refloat config parser sources"
  VERBATIM
)
add_custom_target(refloat_generated_conf DEPENDS ${REFLOAT_GENERATED_CONF_OUTPUTS})

add_library(refloat_conf_support STATIC)
target_sources(
  refloat_conf_support
  PRIVATE
    "${REFLOAT_SRC_DIR}/conf/buffer.c"
    "${REFLOAT_GENERATED_CONF_DIR}/confparser.c"
    "${REFLOAT_GENERATED_CONF_DIR}/confxml.c"
)
target_link_libraries(refloat_conf_support PRIVATE refloat_host_c_options)
add_dependencies(refloat_conf_support refloat_generated_conf)

add_library(refloat_host_common STATIC)
target_sources(
  refloat_host_common
  PRIVATE
    "${REFLOAT_SRC_DIR}/lib/circular_buffer.c"
    "${REFLOAT_SRC_DIR}/lib/utils.c"
    "${REFLOAT_SRC_DIR}/state.c"
    "${REFLOAT_SRC_DIR}/time.c"
    "${REFLOAT_SRC_DIR}/filters/biquad.c"
    "${REFLOAT_SRC_DIR}/filters/ema.c"
    "${REFLOAT_SRC_DIR}/filters/sma.c"
    "${REFLOAT_SRC_DIR}/filters/smooth_setpoint.c"
    "${REFLOAT_SRC_DIR}/pid.c"
    "${REFLOAT_SRC_DIR}/frequency_tracker.c"
    "${REFLOAT_SRC_DIR}/lib/transitions.c"
    "${REFLOAT_SRC_DIR}/lcm.c"
    "${REFLOAT_SRC_DIR}/data_recorder.c"
    "${REFLOAT_SRC_DIR}/motor_data.c"
    "${REFLOAT_SRC_DIR}/motor_control.c"
    "${REFLOAT_SRC_DIR}/footpad_sensor.c"
    "${REFLOAT_SRC_DIR}/remote.c"
    "${REFLOAT_SRC_DIR}/imu.c"
    "${REFLOAT_SRC_DIR}/balance_filter.c"
    "${REFLOAT_SRC_DIR}/charging.c"
    "${REFLOAT_SRC_DIR}/bms.c"
    "${REFLOAT_SRC_DIR}/led_strip.c"
    "${REFLOAT_SRC_DIR}/led_driver.c"
    "${REFLOAT_SRC_DIR}/atr.c"
    "${REFLOAT_SRC_DIR}/booster.c"
    "${REFLOAT_SRC_DIR}/brake_tilt.c"
    "${REFLOAT_SRC_DIR}/alert_tracker.c"
    "${REFLOAT_SRC_DIR}/haptic_feedback.c"
    "${REFLOAT_SRC_DIR}/reverse_stop.c"
    "${REFLOAT_SRC_DIR}/torque_tilt.c"
    "${REFLOAT_SRC_DIR}/turn_tilt.c"
    "${REFLOAT_SRC_DIR}/konami.c"
)
target_link_libraries(refloat_host_common PRIVATE refloat_host_c_options refloat_conf_support)
add_dependencies(refloat_host_common refloat_generated_conf)

add_library(refloat_led_common STATIC)
target_sources(
  refloat_led_common
  PRIVATE
    "${REFLOAT_SRC_DIR}/lib/utils.c"
    "${REFLOAT_SRC_DIR}/state.c"
    "${REFLOAT_SRC_DIR}/led_strip.c"
    "${REFLOAT_SRC_DIR}/leds.c"
)
target_compile_definitions(refloat_led_common PRIVATE __time_t_defined)
target_link_libraries(refloat_led_common PRIVATE refloat_host_c_options refloat_conf_support)
add_dependencies(refloat_led_common refloat_generated_conf)

add_library(refloat_vesc_fake STATIC "${REFLOAT_TEST_DIR}/vesc_if_fake.c")
target_link_libraries(refloat_vesc_fake PRIVATE refloat_host_c_options)

add_library(refloat_host_test_fakes STATIC)
target_sources(
  refloat_host_test_fakes
  PRIVATE
    "${REFLOAT_TEST_DIR}/feedback_fakes.c"
    "${REFLOAT_TEST_DIR}/lcm_fakes.c"
)
target_link_libraries(refloat_host_test_fakes PRIVATE refloat_host_c_options)

add_library(refloat_led_driver_fake STATIC "${REFLOAT_TEST_DIR}/led_driver_fake.c")
target_link_libraries(refloat_led_driver_fake PRIVATE refloat_host_c_options)

add_library(refloat_leds_main_fakes STATIC "${REFLOAT_TEST_DIR}/leds_main_fakes.c")
target_link_libraries(refloat_leds_main_fakes PRIVATE refloat_host_c_options)

add_executable(
  refloat-firmware-tests
  "${REFLOAT_TEST_DIR}/cpp/firmware/bms_leds_data_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/filters_lcm_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/input_remote_imu_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/konami_haptic_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/motor_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/tilt_balance_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/time_and_smoothing_test.cpp"
  "${REFLOAT_TEST_DIR}/cpp/firmware/turn_reverse_alert_test.cpp"
)
target_link_libraries(
  refloat-firmware-tests
  PRIVATE
    refloat_host_c_options
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_host_common
    refloat_vesc_fake
    refloat_host_test_fakes
)
target_compile_options(
  refloat-firmware-tests
  PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>"
)
add_dependencies(refloat-firmware-tests refloat_generated_conf)
catch_discover_tests(
  refloat-firmware-tests
  TEST_SPEC "~[red]"
  TEST_PREFIX "firmware."
  PROPERTIES LABELS "host;cpp;firmware"
)
catch_discover_tests(
  refloat-firmware-tests
  TEST_SPEC "[red]"
  TEST_PREFIX "red.firmware."
  PROPERTIES LABELS "red;cpp;firmware"
)

add_library(refloat_main_bridge STATIC "${REFLOAT_TEST_DIR}/main_protocol_wrapper.c")
target_link_libraries(
  refloat_main_bridge
  PRIVATE
    refloat_host_c_options
    refloat_host_common
    refloat_vesc_fake
    refloat_host_test_fakes
    refloat_leds_main_fakes
)
add_dependencies(refloat_main_bridge refloat_generated_conf)

refloat_add_cpp_test(
  refloat-cpp-main-lifecycle-tests
  cpp.main-lifecycle
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_lifecycle_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "host;cpp;main;lifecycle"
)

refloat_add_cpp_test(
  refloat-cpp-main-command-length-tests
  cpp.main-command-length
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_command_length_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "red;cpp;main;protocol"
)

refloat_add_cpp_test(
  refloat-cpp-main-protocol-tests
  cpp.main-protocol
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_alerts_lights_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "host;cpp;main;protocol"
)

refloat_add_cpp_test(
  refloat-cpp-main-all-data-tests
  cpp.main-all-data
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_all_data_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "red;cpp;main;protocol"
)

refloat_add_cpp_test(
  refloat-cpp-main-gnss-tests
  cpp.main-gnss
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_gnss_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "host;cpp;main;gnss"
)

refloat_add_cpp_test(
  refloat-cpp-main-gnss-missing-tests
  cpp.main-gnss-missing
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_gnss_unavailable_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "red;cpp;main;gnss"
  COMPILE_DEFINITIONS
    REFLOAT_GNSS_UNAVAILABLE_SCENARIO=1
)

refloat_add_cpp_test(
  refloat-cpp-main-gnss-null-tests
  cpp.main-gnss-null
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/main/main_gnss_unavailable_test.cpp"
  LINK_LIBRARIES
    refloat_main_bridge
  LABELS
    "red;cpp;main;gnss"
  COMPILE_DEFINITIONS
    REFLOAT_GNSS_UNAVAILABLE_SCENARIO=2
)

add_library(refloat_cpp_circular_buffer STATIC "${REFLOAT_SRC_DIR}/lib/circular_buffer.c")
target_link_libraries(refloat_cpp_circular_buffer PRIVATE refloat_host_c_options)

refloat_add_cpp_test(
  refloat-cpp-circular-buffer-tests
  cpp.circular-buffer
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/core/circular_buffer_test.cpp"
  LINK_LIBRARIES
    refloat_cpp_circular_buffer
    refloat_led_driver_fake
  LABELS
    "host;cpp;circular-buffer"
)

refloat_add_cpp_test(
  refloat-cpp-leds-tests
  cpp.leds
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/leds/leds_test.cpp"
  LINK_LIBRARIES
    refloat_host_common
    refloat_led_common
    refloat_vesc_fake
    refloat_led_driver_fake
  LABELS
    "host;cpp;leds"
)

refloat_add_cpp_test(
  refloat-generated-config-tests
  cpp.generated-config
  SOURCES
    "${REFLOAT_TEST_DIR}/cpp/config/generated_config_parser_test.cpp"
  LINK_LIBRARIES
    refloat_conf_support
  LABELS
    "host;cpp;generated-config"
)

add_custom_target(
  refloat_host_tests
  DEPENDS
    refloat_generated_conf
    refloat-firmware-tests
    refloat-cpp-main-lifecycle-tests
    refloat-cpp-main-command-length-tests
    refloat-cpp-main-protocol-tests
    refloat-cpp-main-all-data-tests
    refloat-cpp-main-gnss-tests
    refloat-cpp-main-gnss-missing-tests
    refloat-cpp-main-gnss-null-tests
    refloat-generated-config-tests
    refloat-cpp-circular-buffer-tests
    refloat-cpp-leds-tests
  )

if(REFLOAT_BUILD_PACKAGE)
  include("${CMAKE_CURRENT_LIST_DIR}/RefloatPackage.cmake")
endif()

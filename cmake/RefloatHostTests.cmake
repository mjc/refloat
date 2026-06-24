include_guard(GLOBAL)

find_package(Catch2 3 REQUIRED)
find_package(Git REQUIRED)
find_program(REFLOAT_MAKE_EXECUTABLE NAMES gmake make REQUIRED)

set(REFLOAT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
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
target_include_directories(refloat_host_c_options INTERFACE "${REFLOAT_SRC_DIR}")
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
target_compile_options(
  refloat_host_cpp_options
  INTERFACE
    -Wall
    -Wextra
    "$<$<BOOL:${REFLOAT_STRICT_WARNINGS}>:-Werror>"
)

add_custom_command(
  OUTPUT ${REFLOAT_GENERATED_CONF_OUTPUTS}
  COMMAND "${REFLOAT_MAKE_EXECUTABLE}" -C tests generated_conf
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${REFLOAT_GENERATED_CONF_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${REFLOAT_SRC_DIR}/conf/conf_default.h"
    "${REFLOAT_GENERATED_CONF_DIR}/conf_default.h"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${REFLOAT_SRC_DIR}/conf/confparser.h"
    "${REFLOAT_GENERATED_CONF_DIR}/confparser.h"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${REFLOAT_SRC_DIR}/conf/confxml.h"
    "${REFLOAT_GENERATED_CONF_DIR}/confxml.h"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${REFLOAT_SRC_DIR}/conf/conf_general.h"
    "${REFLOAT_GENERATED_CONF_DIR}/conf_general.h"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${REFLOAT_SRC_DIR}/conf/confparser.c"
    "${REFLOAT_GENERATED_CONF_DIR}/confparser.c"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${REFLOAT_SRC_DIR}/conf/confxml.c"
    "${REFLOAT_GENERATED_CONF_DIR}/confxml.c"
  WORKING_DIRECTORY "${REFLOAT_ROOT}"
  DEPENDS ${REFLOAT_GENERATED_CONF_INPUTS}
  COMMENT "Generating Refloat config parser sources through the canonical Make target"
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

add_executable(refloat-c-tests "${REFLOAT_TEST_DIR}/c_tests.c")
target_link_libraries(
  refloat-c-tests
  PRIVATE
    refloat_host_c_options
    refloat_host_common
    refloat_vesc_fake
    refloat_host_test_fakes
)
add_dependencies(refloat-c-tests refloat_generated_conf)
add_test(NAME c.host COMMAND refloat-c-tests)
set_tests_properties(c.host PROPERTIES LABELS "host;c")

add_executable(refloat-led-tests "${REFLOAT_TEST_DIR}/led_tests.c")
target_link_libraries(
  refloat-led-tests
  PRIVATE
    refloat_host_c_options
    refloat_led_common
    refloat_vesc_fake
    refloat_led_driver_fake
)
target_compile_definitions(refloat-led-tests PRIVATE __time_t_defined)
add_dependencies(refloat-led-tests refloat_generated_conf)
add_test(NAME leds.host COMMAND refloat-led-tests)
set_tests_properties(leds.host PROPERTIES LABELS "host;leds")

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

add_executable(
  refloat-main-tests
  "${REFLOAT_TEST_DIR}/main_protocol_tests.c"
)
target_link_libraries(
  refloat-main-tests
  PRIVATE
    refloat_host_c_options
    refloat_host_common
    refloat_main_bridge
    refloat_vesc_fake
    refloat_host_test_fakes
    refloat_leds_main_fakes
)
add_dependencies(refloat-main-tests refloat_generated_conf)
add_test(NAME main.host COMMAND refloat-main-tests)
set_tests_properties(main.host PROPERTIES LABELS "host;main")

add_executable(
  refloat-cpp-main-lifecycle-tests
  "${REFLOAT_TEST_DIR}/cpp/main_lifecycle_test.cpp"
)
target_link_libraries(
  refloat-cpp-main-lifecycle-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-lifecycle-tests refloat_generated_conf)
add_test(NAME cpp.main-lifecycle COMMAND refloat-cpp-main-lifecycle-tests)
set_tests_properties(cpp.main-lifecycle PROPERTIES LABELS "host;cpp;main;lifecycle")

add_executable(
  refloat-cpp-main-command-length-tests
  "${REFLOAT_TEST_DIR}/cpp/main_command_length_test.cpp"
)
target_link_libraries(
  refloat-cpp-main-command-length-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-command-length-tests refloat_generated_conf)
add_test(NAME cpp.main-command-length COMMAND refloat-cpp-main-command-length-tests)
set_tests_properties(
  cpp.main-command-length
  PROPERTIES
    LABELS "host;cpp;main;protocol"
    WILL_FAIL TRUE
)

add_executable(
  refloat-cpp-main-protocol-tests
  "${REFLOAT_TEST_DIR}/cpp/main_alerts_lights_test.cpp"
)
target_link_libraries(
  refloat-cpp-main-protocol-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-protocol-tests refloat_generated_conf)
add_test(NAME cpp.main-protocol COMMAND refloat-cpp-main-protocol-tests)
set_tests_properties(cpp.main-protocol PROPERTIES LABELS "host;cpp;main;protocol")

add_executable(
  refloat-cpp-main-all-data-tests
  "${REFLOAT_TEST_DIR}/cpp/main_all_data_test.cpp"
)
target_link_libraries(
  refloat-cpp-main-all-data-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-all-data-tests refloat_generated_conf)
add_test(NAME cpp.main-all-data COMMAND refloat-cpp-main-all-data-tests)
set_tests_properties(
  cpp.main-all-data
  PROPERTIES
    LABELS "host;cpp;main;protocol"
    WILL_FAIL TRUE
)

add_executable(
  refloat-cpp-main-gnss-tests
  "${REFLOAT_TEST_DIR}/cpp/main_gnss_test.cpp"
)
target_link_libraries(
  refloat-cpp-main-gnss-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-gnss-tests refloat_generated_conf)
add_test(NAME cpp.main-gnss COMMAND refloat-cpp-main-gnss-tests)
set_tests_properties(cpp.main-gnss PROPERTIES LABELS "host;cpp;main;gnss")

add_executable(
  refloat-cpp-main-gnss-missing-tests
  "${REFLOAT_TEST_DIR}/cpp/main_gnss_unavailable_test.cpp"
)
target_compile_definitions(refloat-cpp-main-gnss-missing-tests PRIVATE REFLOAT_GNSS_UNAVAILABLE_SCENARIO=1)
target_link_libraries(
  refloat-cpp-main-gnss-missing-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-gnss-missing-tests refloat_generated_conf)
add_test(NAME cpp.main-gnss-missing COMMAND refloat-cpp-main-gnss-missing-tests)
set_tests_properties(
  cpp.main-gnss-missing
  PROPERTIES
    LABELS "host;cpp;main;gnss"
    WILL_FAIL TRUE
)

add_executable(
  refloat-cpp-main-gnss-null-tests
  "${REFLOAT_TEST_DIR}/cpp/main_gnss_unavailable_test.cpp"
)
target_compile_definitions(refloat-cpp-main-gnss-null-tests PRIVATE REFLOAT_GNSS_UNAVAILABLE_SCENARIO=2)
target_link_libraries(
  refloat-cpp-main-gnss-null-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_main_bridge
)
add_dependencies(refloat-cpp-main-gnss-null-tests refloat_generated_conf)
add_test(NAME cpp.main-gnss-null COMMAND refloat-cpp-main-gnss-null-tests)
set_tests_properties(
  cpp.main-gnss-null
  PROPERTIES
    LABELS "host;cpp;main;gnss"
    WILL_FAIL TRUE
)

add_library(refloat_cpp_circular_buffer STATIC "${REFLOAT_SRC_DIR}/lib/circular_buffer.c")
target_link_libraries(refloat_cpp_circular_buffer PRIVATE refloat_host_c_options)

add_executable(refloat-cpp-smoke-tests "${REFLOAT_TEST_DIR}/cpp/circular_buffer_smoke_test.cpp")
target_link_libraries(
  refloat-cpp-smoke-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_cpp_circular_buffer
    refloat_led_driver_fake
)
add_test(NAME cpp.circular-buffer-smoke COMMAND refloat-cpp-smoke-tests)
set_tests_properties(cpp.circular-buffer-smoke PROPERTIES LABELS "host;cpp;circular-buffer")

add_executable(
  refloat-cpp-circular-buffer-tests
  "${REFLOAT_TEST_DIR}/cpp/circular_buffer_test.cpp"
)
target_link_libraries(
  refloat-cpp-circular-buffer-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_cpp_circular_buffer
)
add_test(NAME cpp.circular-buffer COMMAND refloat-cpp-circular-buffer-tests)
set_tests_properties(cpp.circular-buffer PROPERTIES LABELS "host;cpp;circular-buffer")

add_executable(refloat-cpp-leds-tests "${REFLOAT_TEST_DIR}/cpp/leds_test.cpp")
target_link_libraries(
  refloat-cpp-leds-tests
  PRIVATE
    refloat_host_cpp_options
    Catch2::Catch2WithMain
    refloat_host_common
    refloat_led_common
    refloat_vesc_fake
    refloat_led_driver_fake
)
add_test(NAME cpp.leds COMMAND refloat-cpp-leds-tests)
set_tests_properties(cpp.leds PROPERTIES LABELS "host;cpp;leds")

add_executable(
  refloat-generated-config-tests
  "${REFLOAT_TEST_DIR}/cpp/generated_config_parser_test.cpp"
)
target_link_libraries(
  refloat-generated-config-tests
  PRIVATE
    refloat_host_cpp_options
    refloat_conf_support
    Catch2::Catch2WithMain
)
add_dependencies(refloat-generated-config-tests refloat_generated_conf)
add_test(NAME cpp.generated-config COMMAND refloat-generated-config-tests)
set_tests_properties(cpp.generated-config PROPERTIES LABELS "host;cpp;generated-config")

add_custom_target(
  refloat_host_tests
  DEPENDS
    refloat_generated_conf
    refloat-c-tests
    refloat-led-tests
    refloat-main-tests
    refloat-cpp-main-lifecycle-tests
    refloat-cpp-main-command-length-tests
    refloat-cpp-main-protocol-tests
    refloat-cpp-main-all-data-tests
    refloat-cpp-main-gnss-tests
    refloat-cpp-main-gnss-missing-tests
    refloat-cpp-main-gnss-null-tests
    refloat-cpp-smoke-tests
    refloat-generated-config-tests
    refloat-cpp-circular-buffer-tests
    refloat-cpp-leds-tests
  )

if(REFLOAT_BUILD_PACKAGE)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(REFLOAT_VESC_TOOL_EXECUTABLE NAMES vesc_tool REQUIRED)
  find_program(REFLOAT_ARM_GCC_EXECUTABLE NAMES arm-none-eabi-gcc REQUIRED)
  find_program(REFLOAT_ARM_OBJDUMP_EXECUTABLE NAMES arm-none-eabi-objdump REQUIRED)
  find_program(REFLOAT_ARM_OBJCOPY_EXECUTABLE NAMES arm-none-eabi-objcopy REQUIRED)

  set(REFLOAT_PACKAGE_README_INPUT "${REFLOAT_ROOT}/package_README.md")
  set(REFLOAT_PACKAGE_README_OUTPUT "${REFLOAT_ROOT}/package_README-gen.md")
  set(REFLOAT_PACKAGE_QML_INPUT "${REFLOAT_ROOT}/ui.qml.in")
  set(REFLOAT_PACKAGE_QML_OUTPUT "${REFLOAT_ROOT}/ui.qml")
  set(REFLOAT_PACKAGE_ARTIFACT "${REFLOAT_ROOT}/refloat.vescpkg")
  set(REFLOAT_PACKAGE_CONF_OUTPUTS
    "${REFLOAT_SRC_DIR}/conf/conf_default.h"
    "${REFLOAT_SRC_DIR}/conf/confparser.h"
    "${REFLOAT_SRC_DIR}/conf/confxml.h"
    "${REFLOAT_SRC_DIR}/conf/conf_general.h"
    "${REFLOAT_SRC_DIR}/conf/confparser.c"
    "${REFLOAT_SRC_DIR}/conf/confxml.c"
  )
  set(REFLOAT_PACKAGE_LIB_BINARY "${REFLOAT_SRC_DIR}/package_lib.bin")
  set(REFLOAT_PACKAGE_LIB_ELF "${REFLOAT_SRC_DIR}/package_lib.elf")
  set(REFLOAT_PACKAGE_LIB_LIST "${REFLOAT_SRC_DIR}/package_lib.list")
  set(REFLOAT_PACKAGE_LIB_LISP "${REFLOAT_SRC_DIR}/package_lib.lisp")
  set(REFLOAT_PACKAGE_LIB_OBJ_DIR "${CMAKE_CURRENT_BINARY_DIR}/package-lib-objects")
  file(GLOB REFLOAT_PACKAGE_LIB_SOURCES CONFIGURE_DEPENDS
    "${REFLOAT_SRC_DIR}/*.c"
    "${REFLOAT_SRC_DIR}/filters/*.c"
    "${REFLOAT_SRC_DIR}/lib/*.c"
  )
  list(APPEND REFLOAT_PACKAGE_LIB_SOURCES
    "${REFLOAT_SRC_DIR}/conf/buffer.c"
    "${REFLOAT_SRC_DIR}/conf/confparser.c"
    "${REFLOAT_SRC_DIR}/conf/confxml.c"
    "${REFLOAT_ROOT}/vesc_pkg_lib/utils/rb.c"
    "${REFLOAT_ROOT}/vesc_pkg_lib/utils/utils.c"
  )

  add_custom_command(
    OUTPUT "${REFLOAT_PACKAGE_README_OUTPUT}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DREFLOAT_PACKAGE_README_INPUT=${REFLOAT_PACKAGE_README_INPUT}
      -DREFLOAT_PACKAGE_README_OUTPUT=${REFLOAT_PACKAGE_README_OUTPUT}
      -DREFLOAT_VERSION_FILE=${REFLOAT_ROOT}/version
      -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
      -DREFLOAT_ROOT=${REFLOAT_ROOT}
      -P "${CMAKE_CURRENT_LIST_DIR}/RefloatGeneratePackageReadme.cmake"
    DEPENDS
      "${REFLOAT_PACKAGE_README_INPUT}"
      "${REFLOAT_ROOT}/version"
      ${REFLOAT_GIT_DEPS}
    COMMENT "Generating package_README-gen.md"
    VERBATIM
  )
  add_custom_target(refloat-package-readme DEPENDS "${REFLOAT_PACKAGE_README_OUTPUT}")

  add_custom_command(
    OUTPUT "${REFLOAT_PACKAGE_QML_OUTPUT}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DREFLOAT_PACKAGE_QML_INPUT=${REFLOAT_PACKAGE_QML_INPUT}
      -DREFLOAT_PACKAGE_QML_OUTPUT=${REFLOAT_PACKAGE_QML_OUTPUT}
      -DREFLOAT_PACKAGE_NAME_FILE=${REFLOAT_ROOT}/package_name
      -DREFLOAT_VERSION_FILE=${REFLOAT_ROOT}/version
      -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
      -DREFLOAT_RJSMIN=${REFLOAT_ROOT}/rjsmin.py
      -DREFLOAT_MINIFY_QML=${REFLOAT_MINIFY_QML}
      -P "${CMAKE_CURRENT_LIST_DIR}/RefloatGeneratePackageQml.cmake"
    DEPENDS
      "${REFLOAT_PACKAGE_QML_INPUT}"
      "${REFLOAT_ROOT}/package_name"
      "${REFLOAT_ROOT}/version"
      "${REFLOAT_ROOT}/rjsmin.py"
    COMMENT "Generating ui.qml"
    VERBATIM
  )
  add_custom_target(refloat-package-qml DEPENDS "${REFLOAT_PACKAGE_QML_OUTPUT}")

  add_custom_command(
    OUTPUT ${REFLOAT_PACKAGE_CONF_OUTPUTS}
    COMMAND
      "${CMAKE_COMMAND}"
      -DREFLOAT_ROOT=${REFLOAT_ROOT}
      -DREFLOAT_SRC_DIR=${REFLOAT_SRC_DIR}
      -DVESC_TOOL_EXECUTABLE=${REFLOAT_VESC_TOOL_EXECUTABLE}
      -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
      -P "${CMAKE_CURRENT_LIST_DIR}/RefloatGeneratePackageConf.cmake"
    DEPENDS
      "${REFLOAT_SRC_DIR}/conf/settings.xml"
      "${REFLOAT_SRC_DIR}/conf/conf_general.h.in"
      "${REFLOAT_ROOT}/package_name"
      "${REFLOAT_ROOT}/version"
      ${REFLOAT_GIT_DEPS}
    COMMENT "Generating package configuration sources"
    VERBATIM
  )
  add_custom_target(refloat-package-conf DEPENDS ${REFLOAT_PACKAGE_CONF_OUTPUTS})

  set(REFLOAT_PACKAGE_LIB_INCLUDE_DIRS
    "${REFLOAT_SRC_DIR}"
    "${REFLOAT_ROOT}/vesc_pkg_lib"
    "${REFLOAT_ROOT}/vesc_pkg_lib/utils"
  )
  set(REFLOAT_PACKAGE_LIB_COMPILE_OPTIONS
    -fpic
    -Os
    -Wall
    -Wextra
    -Wundef
    -std=gnu99
    -fomit-frame-pointer
    -falign-functions=16
    -mthumb
    -fsingle-precision-constant
    -Wdouble-promotion
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -mcpu=cortex-m4
    -fdata-sections
    -ffunction-sections
    -DIS_VESC_LIB
  )
  set(REFLOAT_PACKAGE_LIB_LINK_OPTIONS
    -nostartfiles
    -static
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -mcpu=cortex-m4
    -lm
    -Wl,--gc-sections,--undefined=init
    -T "${REFLOAT_ROOT}/vesc_pkg_lib/link.ld"
  )
  set(REFLOAT_PACKAGE_LIB_OBJECTS "")
  foreach(REFLOAT_PACKAGE_LIB_SOURCE IN LISTS REFLOAT_PACKAGE_LIB_SOURCES)
    file(RELATIVE_PATH REFLOAT_PACKAGE_LIB_REL_SOURCE "${REFLOAT_ROOT}" "${REFLOAT_PACKAGE_LIB_SOURCE}")
    string(REGEX REPLACE "\\.c$" ".o" REFLOAT_PACKAGE_LIB_REL_OBJECT "${REFLOAT_PACKAGE_LIB_REL_SOURCE}")
    set(REFLOAT_PACKAGE_LIB_OBJECT "${REFLOAT_PACKAGE_LIB_OBJ_DIR}/${REFLOAT_PACKAGE_LIB_REL_OBJECT}")
    list(APPEND REFLOAT_PACKAGE_LIB_OBJECTS "${REFLOAT_PACKAGE_LIB_OBJECT}")
    get_filename_component(REFLOAT_PACKAGE_LIB_OBJECT_DIR "${REFLOAT_PACKAGE_LIB_OBJECT}" DIRECTORY)
    add_custom_command(
      OUTPUT "${REFLOAT_PACKAGE_LIB_OBJECT}"
      DEPFILE "${REFLOAT_PACKAGE_LIB_OBJECT}.d"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${REFLOAT_PACKAGE_LIB_OBJECT_DIR}"
      COMMAND
        "${REFLOAT_ARM_GCC_EXECUTABLE}"
        ${REFLOAT_PACKAGE_LIB_COMPILE_OPTIONS}
        -MMD
        -MF "${REFLOAT_PACKAGE_LIB_OBJECT}.d"
        -MT "${REFLOAT_PACKAGE_LIB_OBJECT}"
        -I${REFLOAT_ROOT}/vesc_pkg_lib
        -I${REFLOAT_ROOT}/vesc_pkg_lib/utils
        -I${REFLOAT_ROOT}/vesc_pkg_lib/stdperiph_stm32f4/CMSIS/include
        -I${REFLOAT_ROOT}/vesc_pkg_lib/stdperiph_stm32f4/CMSIS/ST
        -I${REFLOAT_ROOT}/vesc_pkg_lib/stdperiph_stm32f4/inc
        -I${REFLOAT_SRC_DIR}
        -c "${REFLOAT_PACKAGE_LIB_SOURCE}"
        -o "${REFLOAT_PACKAGE_LIB_OBJECT}"
      DEPENDS
        "${REFLOAT_PACKAGE_LIB_SOURCE}"
        ${REFLOAT_PACKAGE_CONF_OUTPUTS}
      COMMENT "Compiling ${REFLOAT_PACKAGE_LIB_REL_SOURCE}"
      VERBATIM
  )
  endforeach()
  string(JOIN "|" REFLOAT_PACKAGE_LIB_OBJECTS_JOINED ${REFLOAT_PACKAGE_LIB_OBJECTS})
  string(JOIN "|" REFLOAT_PACKAGE_LIB_LINK_OPTIONS_JOINED ${REFLOAT_PACKAGE_LIB_LINK_OPTIONS})

  add_custom_command(
    OUTPUT "${REFLOAT_PACKAGE_LIB_BINARY}"
    BYPRODUCTS
      "${REFLOAT_PACKAGE_LIB_ELF}"
      "${REFLOAT_PACKAGE_LIB_LIST}"
      "${REFLOAT_PACKAGE_LIB_LISP}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DREFLOAT_ARM_GCC_EXECUTABLE=${REFLOAT_ARM_GCC_EXECUTABLE}
      -DREFLOAT_ARM_OBJDUMP_EXECUTABLE=${REFLOAT_ARM_OBJDUMP_EXECUTABLE}
      -DREFLOAT_ARM_OBJCOPY_EXECUTABLE=${REFLOAT_ARM_OBJCOPY_EXECUTABLE}
      -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
      -DREFLOAT_ROOT=${REFLOAT_ROOT}
      -DREFLOAT_PACKAGE_LIB_ELF=${REFLOAT_PACKAGE_LIB_ELF}
      -DREFLOAT_PACKAGE_LIB_BINARY=${REFLOAT_PACKAGE_LIB_BINARY}
      -DREFLOAT_PACKAGE_LIB_LIST=${REFLOAT_PACKAGE_LIB_LIST}
      -DREFLOAT_PACKAGE_LIB_LISP=${REFLOAT_PACKAGE_LIB_LISP}
      -DREFLOAT_PACKAGE_LIB_OBJECTS=${REFLOAT_PACKAGE_LIB_OBJECTS_JOINED}
      -DREFLOAT_PACKAGE_LIB_LINK_OPTIONS=${REFLOAT_PACKAGE_LIB_LINK_OPTIONS_JOINED}
      -P "${CMAKE_CURRENT_LIST_DIR}/RefloatBuildPackageLib.cmake"
    DEPENDS
      ${REFLOAT_PACKAGE_LIB_OBJECTS}
      ${REFLOAT_PACKAGE_CONF_OUTPUTS}
      "${REFLOAT_ROOT}/vesc_pkg_lib/link.ld"
      "${REFLOAT_ROOT}/vesc_pkg_lib/conv.py"
    COMMENT "Building package_lib with native CMake commands"
    VERBATIM
  )
  add_custom_target(refloat-package-lib DEPENDS "${REFLOAT_PACKAGE_LIB_BINARY}")
  add_dependencies(refloat-package-lib refloat-package-conf)

  add_custom_command(
    OUTPUT "${REFLOAT_PACKAGE_ARTIFACT}"
    COMMAND "${REFLOAT_VESC_TOOL_EXECUTABLE}" --buildPkgFromDesc pkgdesc.qml
    WORKING_DIRECTORY "${REFLOAT_ROOT}"
    DEPENDS
      "${REFLOAT_ROOT}/pkgdesc.qml"
      "${REFLOAT_PACKAGE_README_OUTPUT}"
      "${REFLOAT_PACKAGE_QML_OUTPUT}"
      "${REFLOAT_ROOT}/lisp/package.lisp"
      "${REFLOAT_ROOT}/lisp/bms.lisp"
      "${REFLOAT_PACKAGE_LIB_BINARY}"
    COMMENT "Building refloat.vescpkg with vesc_tool"
    VERBATIM
  )

  add_custom_target(
    refloat-package-only
    DEPENDS "${REFLOAT_PACKAGE_ARTIFACT}"
  )
  add_dependencies(
    refloat-package-only
    refloat-package-lib
    refloat-package-readme
    refloat-package-qml
  )

  add_custom_target(
    refloat-package
    DEPENDS refloat-package-only
  )
endif()

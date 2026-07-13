if(NOT DEFINED REFLOAT_ROOT)
  message(FATAL_ERROR "REFLOAT_ROOT is required")
endif()

if(NOT DEFINED REFLOAT_VESC_TOOL_EXECUTABLE)
  message(FATAL_ERROR "REFLOAT_VESC_TOOL_EXECUTABLE is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_BUILD_LOG)
  message(FATAL_ERROR "REFLOAT_PACKAGE_BUILD_LOG is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_DESC)
  message(FATAL_ERROR "REFLOAT_PACKAGE_DESC is required")
endif()

execute_process(
  COMMAND "${REFLOAT_VESC_TOOL_EXECUTABLE}" --buildPkgFromDesc "${REFLOAT_PACKAGE_DESC}"
  RESULT_VARIABLE _refloat_package_result
  OUTPUT_VARIABLE _refloat_package_stdout
  ERROR_VARIABLE _refloat_package_stderr
)

set(_refloat_package_output "${_refloat_package_stdout}")
string(APPEND _refloat_package_output "${_refloat_package_stderr}")
file(WRITE "${REFLOAT_PACKAGE_BUILD_LOG}" "${_refloat_package_output}")

if(_refloat_package_stdout)
  message(STATUS "${_refloat_package_stdout}")
endif()

if(_refloat_package_stderr)
  message(STATUS "${_refloat_package_stderr}")
endif()

if(NOT _refloat_package_result EQUAL 0)
  message(FATAL_ERROR "Package build failed")
endif()

if(NOT DEFINED REFLOAT_ROOT)
  message(FATAL_ERROR "REFLOAT_ROOT is required")
endif()

if(NOT DEFINED REFLOAT_SRC_DIR)
  message(FATAL_ERROR "REFLOAT_SRC_DIR is required")
endif()

if(NOT DEFINED VESC_TOOL_EXECUTABLE)
  message(FATAL_ERROR "VESC_TOOL_EXECUTABLE is required")
endif()

if(NOT DEFINED GIT_EXECUTABLE)
  message(FATAL_ERROR "GIT_EXECUTABLE is required")
endif()

file(READ "${REFLOAT_ROOT}/package_name" _refloat_package_name)
file(READ "${REFLOAT_ROOT}/version" _refloat_version)
string(STRIP "${_refloat_package_name}" _refloat_package_name)
string(STRIP "${_refloat_version}" _refloat_version)
string(SUBSTRING "${_refloat_package_name}" 0 20 _refloat_package_name)

execute_process(
  COMMAND "${VESC_TOOL_EXECUTABLE}" --xmlConfToCode "${REFLOAT_SRC_DIR}/conf/settings.xml"
  WORKING_DIRECTORY "${REFLOAT_SRC_DIR}/conf"
  COMMAND_ERROR_IS_FATAL ANY
)

file(READ "${REFLOAT_SRC_DIR}/conf/confxml.c" _refloat_confxml)
string(REPLACE "uint8_t data_" "__attribute__((used)) uint8_t data_" _refloat_confxml "${_refloat_confxml}")
file(WRITE "${REFLOAT_SRC_DIR}/conf/confxml.c" "${_refloat_confxml}")

execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${REFLOAT_ROOT}" rev-parse --short=8 HEAD
  OUTPUT_VARIABLE _refloat_git_hash
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
file(READ "${REFLOAT_SRC_DIR}/conf/conf_general.h.in" _refloat_conf_general_template)
if(_refloat_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([0-9A-Za-z.-]+))?$")
  set(_refloat_major "${CMAKE_MATCH_1}")
  set(_refloat_minor "${CMAKE_MATCH_2}")
  set(_refloat_patch "${CMAKE_MATCH_3}")
  set(_refloat_suffix "${CMAKE_MATCH_5}")
else()
  message(FATAL_ERROR "Unexpected version format: ${_refloat_version}")
endif()

string(REPLACE "{{PACKAGE_NAME}}" "${_refloat_package_name}" _refloat_conf_general "${_refloat_conf_general_template}")
string(REPLACE "{{VERSION}}" "${_refloat_version}" _refloat_conf_general "${_refloat_conf_general}")
string(REPLACE "{{MAJOR_VERSION}}" "${_refloat_major}" _refloat_conf_general "${_refloat_conf_general}")
string(REPLACE "{{MINOR_VERSION}}" "${_refloat_minor}" _refloat_conf_general "${_refloat_conf_general}")
string(REPLACE "{{PATCH_VERSION}}" "${_refloat_patch}" _refloat_conf_general "${_refloat_conf_general}")
string(REPLACE "{{VERSION_SUFFIX}}" "${_refloat_suffix}" _refloat_conf_general "${_refloat_conf_general}")
string(REPLACE "{{GIT_HASH}}" "${_refloat_git_hash}" _refloat_conf_general "${_refloat_conf_general}")

set(_refloat_conf_general_output "${REFLOAT_SRC_DIR}/conf/conf_general.h")
if(EXISTS "${_refloat_conf_general_output}")
  file(READ "${_refloat_conf_general_output}" _refloat_existing_conf_general)
  if(_refloat_existing_conf_general STREQUAL _refloat_conf_general)
    return()
  endif()
endif()

file(WRITE "${_refloat_conf_general_output}.tmp" "${_refloat_conf_general}")
file(REMOVE "${_refloat_conf_general_output}")
file(RENAME "${_refloat_conf_general_output}.tmp" "${_refloat_conf_general_output}")

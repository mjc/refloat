if(NOT DEFINED REFLOAT_PACKAGE_QML_INPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_QML_INPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_QML_OUTPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_QML_OUTPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_NAME_FILE)
  message(FATAL_ERROR "REFLOAT_PACKAGE_NAME_FILE is required")
endif()

if(NOT DEFINED REFLOAT_VERSION_FILE)
  message(FATAL_ERROR "REFLOAT_VERSION_FILE is required")
endif()

if(NOT DEFINED PYTHON_EXECUTABLE)
  message(FATAL_ERROR "PYTHON_EXECUTABLE is required")
endif()

if(NOT DEFINED REFLOAT_RJSMIN)
  message(FATAL_ERROR "REFLOAT_RJSMIN is required")
endif()

file(READ "${REFLOAT_PACKAGE_QML_INPUT}" _refloat_qml)
file(READ "${REFLOAT_PACKAGE_NAME_FILE}" _refloat_package_name)
file(READ "${REFLOAT_VERSION_FILE}" _refloat_version)

string(STRIP "${_refloat_package_name}" _refloat_package_name)
string(STRIP "${_refloat_version}" _refloat_version)
string(SUBSTRING "${_refloat_package_name}" 0 20 _refloat_package_name)

string(REPLACE "{{PACKAGE_NAME}}" "${_refloat_package_name}" _refloat_qml "${_refloat_qml}")
string(REPLACE "{{VERSION}}" "${_refloat_version}" _refloat_qml "${_refloat_qml}")

set(_refloat_qml_tmp "${REFLOAT_PACKAGE_QML_OUTPUT}.tmp")
file(WRITE "${_refloat_qml_tmp}.input" "${_refloat_qml}")

if(REFLOAT_MINIFY_QML)
  execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${REFLOAT_RJSMIN}"
    INPUT_FILE "${_refloat_qml_tmp}.input"
    OUTPUT_FILE "${_refloat_qml_tmp}"
    COMMAND_ERROR_IS_FATAL ANY
  )
else()
  file(RENAME "${_refloat_qml_tmp}.input" "${_refloat_qml_tmp}")
endif()

file(REMOVE "${_refloat_qml_tmp}.input")

if(EXISTS "${REFLOAT_PACKAGE_QML_OUTPUT}")
  file(READ "${REFLOAT_PACKAGE_QML_OUTPUT}" _refloat_existing_qml)
  file(READ "${_refloat_qml_tmp}" _refloat_generated_qml)
  if(_refloat_existing_qml STREQUAL _refloat_generated_qml)
    file(REMOVE "${_refloat_qml_tmp}")
    return()
  endif()
endif()

file(REMOVE "${REFLOAT_PACKAGE_QML_OUTPUT}")
file(RENAME "${_refloat_qml_tmp}" "${REFLOAT_PACKAGE_QML_OUTPUT}")

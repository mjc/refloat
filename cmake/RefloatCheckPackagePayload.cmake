if(NOT DEFINED REFLOAT_ROOT)
  message(FATAL_ERROR "REFLOAT_ROOT is required")
endif()

if(NOT DEFINED CMAKE_COMMAND)
  message(FATAL_ERROR "CMAKE_COMMAND is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_ARTIFACT)
  set(REFLOAT_PACKAGE_ARTIFACT "${REFLOAT_ROOT}/refloat.vescpkg")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_BUILD_LOG)
  set(REFLOAT_PACKAGE_BUILD_LOG "${REFLOAT_ROOT}/build/package/refloat-package.log")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES)
  message(FATAL_ERROR "REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES is required")
endif()

if(NOT EXISTS "${REFLOAT_PACKAGE_ARTIFACT}" OR NOT EXISTS "${REFLOAT_PACKAGE_BUILD_LOG}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build --preset package
    WORKING_DIRECTORY "${REFLOAT_ROOT}"
    RESULT_VARIABLE _refloat_package_result
  )
  if(NOT _refloat_package_result EQUAL 0)
    message(FATAL_ERROR "Package build failed")
  endif()
endif()

if(NOT EXISTS "${REFLOAT_PACKAGE_ARTIFACT}")
  message(FATAL_ERROR "Package build did not create ${REFLOAT_PACKAGE_ARTIFACT}")
endif()

if(NOT EXISTS "${REFLOAT_PACKAGE_BUILD_LOG}")
  message(FATAL_ERROR "Package build did not create ${REFLOAT_PACKAGE_BUILD_LOG}")
endif()

file(READ "${REFLOAT_PACKAGE_BUILD_LOG}" _refloat_package_output)

string(REGEX MATCH "Compressed QML size[ ]*:[ ]*([0-9]+)[ ]*/[ ]*([0-9]+)[ ]*bytes" _refloat_qml_match "${_refloat_package_output}")
if(NOT _refloat_qml_match)
  message(FATAL_ERROR "Could not find compressed QML size in package output:\n${_refloat_package_output}")
endif()
set(_refloat_qml_size "${CMAKE_MATCH_1}")
set(_refloat_qml_limit "${CMAKE_MATCH_2}")

string(REGEX MATCH "Lisp data size[ ]*:[ ]*([0-9]+)[ ]*/[ ]*([0-9]+)[ ]*bytes" _refloat_lisp_match "${_refloat_package_output}")
if(NOT _refloat_lisp_match)
  message(FATAL_ERROR "Could not find Lisp data size in package output:\n${_refloat_package_output}")
endif()
set(_refloat_lisp_size "${CMAKE_MATCH_1}")
set(_refloat_lisp_limit "${CMAKE_MATCH_2}")

if(NOT _refloat_qml_limit STREQUAL REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES OR
   NOT _refloat_lisp_limit STREQUAL REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES)
  message(
    FATAL_ERROR
      "Package limit drifted: QML reported ${_refloat_qml_limit}, Lisp reported ${_refloat_lisp_limit}, expected ${REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES}\n${_refloat_package_output}"
  )
endif()

if(_refloat_qml_size GREATER REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES OR _refloat_lisp_size GREATER REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES)
  message(
    FATAL_ERROR
      "Package payload exceeded the VESC limit of ${REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES} bytes: QML=${_refloat_qml_size}, Lisp=${_refloat_lisp_size}\n${_refloat_package_output}"
  )
endif()

message(
  STATUS
    "Package payload check passed: QML ${_refloat_qml_size}/${REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES}, Lisp ${_refloat_lisp_size}/${REFLOAT_PACKAGE_PAYLOAD_LIMIT_BYTES}"
)

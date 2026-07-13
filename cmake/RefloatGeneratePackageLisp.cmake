if(NOT DEFINED REFLOAT_PACKAGE_LISP_INPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_LISP_INPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_LISP_OUTPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_LISP_OUTPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_LIB_BINARY)
  message(FATAL_ERROR "REFLOAT_PACKAGE_LIB_BINARY is required")
endif()

if(NOT DEFINED REFLOAT_BMS_LISP_INPUT)
  message(FATAL_ERROR "REFLOAT_BMS_LISP_INPUT is required")
endif()

file(READ "${REFLOAT_PACKAGE_LISP_INPUT}" _refloat_package_lisp)
string(REPLACE "\"src/package_lib.bin\"" "\"${REFLOAT_PACKAGE_LIB_BINARY}\"" _refloat_package_lisp "${_refloat_package_lisp}")
string(REPLACE "\"bms.lisp\"" "\"${REFLOAT_BMS_LISP_INPUT}\"" _refloat_package_lisp "${_refloat_package_lisp}")

if(EXISTS "${REFLOAT_PACKAGE_LISP_OUTPUT}")
  file(READ "${REFLOAT_PACKAGE_LISP_OUTPUT}" _refloat_existing_lisp)
  if(_refloat_existing_lisp STREQUAL _refloat_package_lisp)
    return()
  endif()
endif()

file(WRITE "${REFLOAT_PACKAGE_LISP_OUTPUT}.tmp" "${_refloat_package_lisp}")
file(RENAME "${REFLOAT_PACKAGE_LISP_OUTPUT}.tmp" "${REFLOAT_PACKAGE_LISP_OUTPUT}")

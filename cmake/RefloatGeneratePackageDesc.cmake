if(NOT DEFINED REFLOAT_PACKAGE_DESC_INPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_DESC_INPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_DESC_OUTPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_DESC_OUTPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_README_OUTPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_README_OUTPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_QML_OUTPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_QML_OUTPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_LISP_INPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_LISP_INPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_ARTIFACT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_ARTIFACT is required")
endif()

file(READ "${REFLOAT_PACKAGE_DESC_INPUT}" _refloat_pkgdesc)

string(REPLACE "property string pkgDescriptionMd: \"package_README-gen.md\""
  "property string pkgDescriptionMd: \"${REFLOAT_PACKAGE_README_OUTPUT}\""
  _refloat_pkgdesc "${_refloat_pkgdesc}")
string(REPLACE "property string pkgLisp: \"lisp/package.lisp\""
  "property string pkgLisp: \"${REFLOAT_PACKAGE_LISP_INPUT}\""
  _refloat_pkgdesc "${_refloat_pkgdesc}")
string(REPLACE "property string pkgQml: \"ui.qml\""
  "property string pkgQml: \"${REFLOAT_PACKAGE_QML_OUTPUT}\""
  _refloat_pkgdesc "${_refloat_pkgdesc}")
string(REPLACE "property string pkgOutput: \"refloat.vescpkg\""
  "property string pkgOutput: \"${REFLOAT_PACKAGE_ARTIFACT}\""
  _refloat_pkgdesc "${_refloat_pkgdesc}")

if(EXISTS "${REFLOAT_PACKAGE_DESC_OUTPUT}")
  file(READ "${REFLOAT_PACKAGE_DESC_OUTPUT}" _refloat_existing_pkgdesc)
  if(_refloat_existing_pkgdesc STREQUAL _refloat_pkgdesc)
    return()
  endif()
endif()

file(WRITE "${REFLOAT_PACKAGE_DESC_OUTPUT}.tmp" "${_refloat_pkgdesc}")
file(RENAME "${REFLOAT_PACKAGE_DESC_OUTPUT}.tmp" "${REFLOAT_PACKAGE_DESC_OUTPUT}")

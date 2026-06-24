if(NOT DEFINED REFLOAT_PACKAGE_README_INPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_README_INPUT is required")
endif()

if(NOT DEFINED REFLOAT_PACKAGE_README_OUTPUT)
  message(FATAL_ERROR "REFLOAT_PACKAGE_README_OUTPUT is required")
endif()

if(NOT DEFINED REFLOAT_VERSION_FILE)
  message(FATAL_ERROR "REFLOAT_VERSION_FILE is required")
endif()

if(NOT DEFINED GIT_EXECUTABLE)
  message(FATAL_ERROR "GIT_EXECUTABLE is required")
endif()

if(NOT DEFINED REFLOAT_ROOT)
  message(FATAL_ERROR "REFLOAT_ROOT is required")
endif()

file(READ "${REFLOAT_PACKAGE_README_INPUT}" _refloat_package_readme)
file(READ "${REFLOAT_VERSION_FILE}" _refloat_version)
string(STRIP "${_refloat_version}" _refloat_version)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${REFLOAT_ROOT}" rev-parse --short HEAD
  OUTPUT_VARIABLE _refloat_git_commit
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)

string(TIMESTAMP _refloat_build_date "%Y-%m-%d %H:%M:%S%z")

set(_refloat_generated_readme "${_refloat_package_readme}\n\n### Build Info\n")
string(APPEND _refloat_generated_readme "- Version: ${_refloat_version}\n")
string(APPEND _refloat_generated_readme "- Build Date: ${_refloat_build_date}\n")
string(APPEND _refloat_generated_readme "- Git Commit: #${_refloat_git_commit}\n")

if(EXISTS "${REFLOAT_PACKAGE_README_OUTPUT}")
  file(READ "${REFLOAT_PACKAGE_README_OUTPUT}" _refloat_existing_readme)
  if(_refloat_existing_readme STREQUAL _refloat_generated_readme)
    return()
  endif()
endif()

file(WRITE "${REFLOAT_PACKAGE_README_OUTPUT}.tmp" "${_refloat_generated_readme}")
file(REMOVE "${REFLOAT_PACKAGE_README_OUTPUT}")
file(RENAME "${REFLOAT_PACKAGE_README_OUTPUT}.tmp" "${REFLOAT_PACKAGE_README_OUTPUT}")

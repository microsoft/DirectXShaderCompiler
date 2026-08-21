# Settings used to build official DXC Release binaries.

include("${CMAKE_CURRENT_LIST_DIR}/PredefinedParams.cmake")

set(LLVM_USE_CRT_RELEASE MT CACHE STRING
  "Use the static multithreaded MSVC runtime for Release builds.")


if(WIN32)
find_package(TAEF REQUIRED)
include_directories(${TAEF_INCLUDE_DIRS})

add_clang_library(HLSLTestLib
  D3DReflectionDumper.cpp
  DxcTestUtils.cpp
  FileCheckerTest.cpp
  FileCheckForTest.cpp
)
else(WIN32)
set(HLSL_IGNORE_SOURCES
  D3DReflectionDumper.cpp
  FileCheckerTest.cpp
)
add_clang_library(HLSLTestLib
  DxcTestUtils.cpp
  FileCheckForTest.cpp
)
include_directories(${DXC_GTEST_DIR}/googletest/include)
include_directories(${DXC_GTEST_DIR}/googlemock/include)
endif(WIN32)

add_dependencies(HLSLTestLib TablegenHLSLOptions)

set_target_properties(HLSLTestLib PROPERTIES FOLDER "Clang tests")

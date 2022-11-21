
if(WIN32)
find_package(TAEF REQUIRED)
include_directories(${TAEF_INCLUDE_DIRS})
else(WIN32)
include_directories(${LLVM_MAIN_SRC_DIR}/utils/unittest/googletest/include)
include_directories(${LLVM_MAIN_SRC_DIR}/utils/unittest/googlemock/include)
endif(WIN32)

add_clang_library(HLSLTestLib
  D3DReflectionDumper.cpp
  D3DReflectionStrings.cpp
  DxcTestUtils.cpp
  FileCheckerTest.cpp
  FileCheckForTest.cpp
  RDATDumper.cpp
)

add_dependencies(HLSLTestLib TablegenHLSLOptions)

set_target_properties(HLSLTestLib PROPERTIES FOLDER "Clang tests")


if(WIN32)
set(HLSL_IGNORE_SOURCES
  HLSLTestOptions.cpp
)
find_package(TAEF REQUIRED)
include_directories(${TAEF_INCLUDE_DIRS})

add_clang_library(HLSLTestLib
  D3DReflectionDumper.cpp
  DxcTestUtils.cpp
  FileCheckerTest.cpp
  FileCheckForTest.cpp
  RDATDumper.cpp
)
else(WIN32)
set(HLSL_IGNORE_SOURCES
  D3DReflectionDumper.cpp
  RDATDumper.cpp
)
add_clang_library(HLSLTestLib
  DxcTestUtils.cpp
  FileCheckerTest.cpp
  FileCheckForTest.cpp
  HLSLTestOptions.cpp
)
include_directories(${LLVM_MAIN_SRC_DIR}/utils/unittest/googletest/include)
include_directories(${LLVM_MAIN_SRC_DIR}/utils/unittest/googlemock/include)

if(ENABLE_DXIL2SPV)
include_directories(
  ${LLVM_SOURCE_DIR}/tools/clang/tools
  ${SPIRV_TOOLS_INCLUDE_DIR}
  ${SPIRV_HEADER_INCLUDE_DIR}
  )
target_link_libraries(HLSLTestLib
  clangSPIRV
  dxcompiler
  dxil2spvlib
  )
endif(ENABLE_DXIL2SPV)

endif(WIN32)

add_dependencies(HLSLTestLib TablegenHLSLOptions)

set_target_properties(HLSLTestLib PROPERTIES FOLDER "Clang tests")

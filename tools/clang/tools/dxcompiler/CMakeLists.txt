# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  analysis
  asmparser
#  asmprinter # no support for LLVM codegen
  bitreader
  bitwriter
#  codegen # no support for LLVM codegen
  core
#  debuginfodwarf # no support for DWARF files (IR debug info is OK)
#  debuginfopdb # no support for PDB files
  dxcsupport
  dxil
  dxilcontainer
  dxilpixpasses # for DxcOptimizerPass
  dxilrootsignature
  hlsl
  instcombine
  ipa
  ipo
  irreader
#  libdriver
#  lineeditor
  linker
  lto
#  mirparser # no support for LLVM codegen
  mssupport
#  object # no support for object files (coff, elf)
  option
#  passes
  profiledata
  scalaropts
#  selectiondag # no support for LLVM codegen
  support
  target
  transformutils
  vectorize
  )

if (WIN32)
set(SOURCES
  dxcapi.cpp
  dxcassembler.cpp
  dxclibrary.cpp
  dxcompilerobj.cpp
  dxcvalidator.cpp
  DXCompiler.cpp
  DXCompiler.rc
  DXCompiler.def
  dxcfilesystem.cpp
  dxillib.cpp
  dxcutil.cpp
  dxcdisassembler.cpp
  dxclinker.cpp
)
else ()
set(SOURCES
  dxcapi.cpp
  dxcassembler.cpp
  dxclibrary.cpp
  dxcompilerobj.cpp
  DXCompiler.cpp
  dxcfilesystem.cpp
  dxcutil.cpp
  dxcdisassembler.cpp
  dxillib.cpp
  dxcvalidator.cpp
)
set (HLSL_IGNORE_SOURCES
  dxcdia.cpp
  dxclinker.cpp
)
endif(WIN32)

set(LIBRARIES
  clangIndex
#  clangARCMigrate
  clangRewrite
  clangCodeGen
  clangRewriteFrontend
  clangFrontend
  clangDriver
#  clangSerialization
  clangSema
  clangEdit
  clangAST
  clangCodeGen
  clangLex
  clangTooling
  clangBasic
  libclang
  )

if(WIN32)
  set(LIBRARIES
    ${LIBRARIES} 
    LLVMDxilDia)
endif(WIN32)

set(GENERATED_HEADERS
  ClangAttrClasses
  ClangAttrList
  ClangAttrParsedAttrList
  ClangCommentNodes
  ClangDiagnosticCommon
  ClangDiagnosticFrontend
  ClangDeclNodes
  ClangStmtNodes
  )

add_clang_library(dxcompiler SHARED ${SOURCES})
add_dependencies(dxcompiler TablegenHLSLOptions) 
if (WIN32)
  # No DxcEtw on non-Windows platforms.
  add_dependencies(dxcompiler DxcEtw)
endif()
target_link_libraries(dxcompiler PRIVATE ${LIBRARIES})
if (ENABLE_SPIRV_CODEGEN)
  target_link_libraries(dxcompiler PRIVATE clangSPIRV)
endif (ENABLE_SPIRV_CODEGEN)
include_directories(AFTER ${LLVM_INCLUDE_DIR}/dxc/Tracing)

set_target_properties(dxcompiler
  PROPERTIES
  OUTPUT_NAME "dxcompiler"
  VERSION ${LIBCLANG_LIBRARY_VERSION}
  DEFINE_SYMBOL _CINDEX_LIB_)

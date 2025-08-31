# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

set(LIBRARIES
  clangFrontend
  clangAST
  clangBasic
  clangLex
  )
  
add_clang_library(dxcreflection STATIC DxcReflection.cpp)
target_link_libraries(dxcreflection PRIVATE ${LIBRARIES})

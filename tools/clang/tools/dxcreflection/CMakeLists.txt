# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

set(LIBRARIES
  clangFrontend
  clangAST
  clangBasic
  clangLex
  )
  
add_clang_library(dxcreflection STATIC 
  dxcreflector.cpp
  dxcreflection_from_ast.cpp)

target_link_libraries(dxcreflection PRIVATE ${LIBRARIES} dxcreflectioncontainer)

if(MSVC)
  target_compile_options(dxcreflection PRIVATE /W4 /WX)
else()
  target_compile_options(dxcreflection PRIVATE -Wall -Wextra -Werror -Wno-pedantic)
endif()

# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

add_clang_library(dxcreflectioncontainer STATIC DxcReflectionContainer.cpp DxcReflectionJson.cpp)

if(MSVC)
  target_compile_options(dxcreflectioncontainer PRIVATE /W4 /WX)
else()
  target_compile_options(dxcreflectioncontainer PRIVATE -Wall -Wextra -Werror -Wno-pedantic)
endif()

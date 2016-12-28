# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
if(NOT CLANG_BUILD_EXAMPLES)
  set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)
  set(EXCLUDE_FROM_ALL ON)
endif()

if(CLANG_ENABLE_STATIC_ANALYZER)
add_subdirectory(analyzer-plugin)
endif()
# add_subdirectory(clang-interpreter) // HLSL Change
# add_subdirectory(PrintFunctionNames) // HLSL Change

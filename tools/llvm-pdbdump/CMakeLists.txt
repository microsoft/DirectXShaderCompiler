# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_LINK_COMPONENTS
  Support
  DebugInfoPDB
  )

add_llvm_tool(llvm-pdbdump
  llvm-pdbdump.cpp
  BuiltinDumper.cpp
  ClassDefinitionDumper.cpp
  CompilandDumper.cpp
  EnumDumper.cpp
  ExternalSymbolDumper.cpp
  FunctionDumper.cpp
  LinePrinter.cpp
  TypeDumper.cpp
  TypedefDumper.cpp
  VariableDumper.cpp
  )

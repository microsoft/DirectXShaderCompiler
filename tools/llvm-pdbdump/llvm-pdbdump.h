//===- llvm-pdbdump.h ----------------------------------------- *- C++ --*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// llvm-pdbdump.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_LLVMPDBDUMP_H
#define LLVM_TOOLS_LLVMPDBDUMP_LLVMPDBDUMP_H

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

namespace opts {
extern llvm::cl::opt<bool> Compilands;
extern llvm::cl::opt<bool> Symbols;
extern llvm::cl::opt<bool> Globals;
extern llvm::cl::opt<bool> Types;
extern llvm::cl::opt<bool> All;

extern llvm::cl::opt<bool> ExcludeCompilerGenerated;

extern llvm::cl::opt<bool> NoClassDefs;
extern llvm::cl::opt<bool> NoEnumDefs;
extern llvm::cl::list<std::string> ExcludeTypes;
extern llvm::cl::list<std::string> ExcludeSymbols;
extern llvm::cl::list<std::string> ExcludeCompilands;
}

#endif
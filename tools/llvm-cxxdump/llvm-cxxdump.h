//===-- llvm-cxxdump.h ------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// llvm-cxxdump.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVM_CXXDUMP_LLVM_CXXDUMP_H
#define LLVM_TOOLS_LLVM_CXXDUMP_LLVM_CXXDUMP_H

#include "llvm/Support/CommandLine.h"
#include <string>

namespace opts {
extern llvm::cl::list<std::string> InputFilenames;
} // namespace opts

#define LLVM_CXXDUMP_ENUM_ENT(ns, enum)                                        \
  { #enum, ns::enum }

#endif

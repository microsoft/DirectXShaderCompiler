//===-- CommandFlags.h - Command Line Flags Interface -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the options accepted by dxil2spv. The general approach is
// to mirror the relevant options from HLSLOptions.td.
//
//===----------------------------------------------------------------------===//

#ifndef DXIL2SPV_LIB_COMMANDFLAGS_H
#define DXIL2SPV_LIB_COMMANDFLAGS_H

#include "llvm/Support/CommandLine.h"
#include <string>

static llvm::cl::opt<bool> Help("help",
                                llvm::cl::desc("Display available options"));
static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                                llvm::cl::desc("<input file>"));
static llvm::cl::opt<std::string>
    OutputFilename("Fo", llvm::cl::desc("Output object file"),
                   llvm::cl::value_desc("file"));

#endif

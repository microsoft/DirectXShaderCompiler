//===--- yaml2obj.h - -------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// yaml2obj.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
/// \file                                                                    //
/// \brief Common declarations for yaml2obj                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef LLVM_TOOLS_YAML2OBJ_YAML2OBJ_H
#define LLVM_TOOLS_YAML2OBJ_YAML2OBJ_H

namespace llvm {
class raw_ostream;
namespace yaml {
class Input;
}
}
int yaml2coff(llvm::yaml::Input &YIn, llvm::raw_ostream &Out);
int yaml2elf(llvm::yaml::Input &YIn, llvm::raw_ostream &Out);

#endif

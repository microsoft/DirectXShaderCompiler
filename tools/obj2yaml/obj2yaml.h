//===------ utils/obj2yaml.hpp - obj2yaml conversion tool -------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// obj2yaml.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_OBJ2YAML_OBJ2YAML_H
#define LLVM_TOOLS_OBJ2YAML_OBJ2YAML_H

#include "llvm/Object/COFF.h"
#include "llvm/Support/raw_ostream.h"
#include <system_error>

std::error_code coff2yaml(llvm::raw_ostream &Out,
                          const llvm::object::COFFObjectFile &Obj);
std::error_code elf2yaml(llvm::raw_ostream &Out,
                         const llvm::object::ObjectFile &Obj);

#endif

//===-- ObjDumper.cpp - Base dumper class -----------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ObjDumper.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief This file implements ObjDumper.                                   //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "ObjDumper.h"
#include "Error.h"
#include "StreamWriter.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

ObjDumper::ObjDumper(StreamWriter& Writer)
  : W(Writer) {
}

ObjDumper::~ObjDumper() {
}

} // namespace llvm

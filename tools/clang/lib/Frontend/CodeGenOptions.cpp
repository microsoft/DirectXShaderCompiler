//===--- CodeGenOptions.cpp -----------------------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CodeGenOptions.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Frontend/CodeGenOptions.h"
#include <string.h>

namespace clang {

CodeGenOptions::CodeGenOptions() {
#define CODEGENOPT(Name, Bits, Default) Name = Default;
#define ENUM_CODEGENOPT(Name, Type, Bits, Default) set##Name(Default);
#include "clang/Frontend/CodeGenOptions.def"

  RelocationModel = "pic";
  memcpy(CoverageVersion, "402*", 4);
}

}  // end namespace clang

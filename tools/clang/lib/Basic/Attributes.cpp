//===- Attributes.cpp -------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Attributes.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Basic/Attributes.h"
#include "clang/Basic/IdentifierTable.h"
#include "llvm/ADT/StringSwitch.h"
using namespace clang;

int clang::hasAttribute(AttrSyntax Syntax, const IdentifierInfo *Scope,
                         const IdentifierInfo *Attr, const llvm::Triple &T,
                         const LangOptions &LangOpts) {
  StringRef Name = Attr->getName();
  // Normalize the attribute name, __foo__ becomes foo.
  if (Name.size() >= 4 && Name.startswith("__") && Name.endswith("__"))
    Name = Name.substr(2, Name.size() - 4);

#include "clang/Basic/AttrHasAttributeImpl.inc"

  return 0;
}

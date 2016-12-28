//===--- LangStandards.cpp - Language Standard Definitions ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LangStandards.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Frontend/LangStandard.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
using namespace clang;
using namespace clang::frontend;

#define LANGSTANDARD(id, name, desc, features) \
  static const LangStandard Lang_##id = { name, desc, features };
#include "clang/Frontend/LangStandards.def"

const LangStandard &LangStandard::getLangStandardForKind(Kind K) {
  switch (K) {
  case lang_unspecified:
    llvm::report_fatal_error("getLangStandardForKind() on unspecified kind");
#define LANGSTANDARD(id, name, desc, features) \
    case lang_##id: return Lang_##id;
#include "clang/Frontend/LangStandards.def"
  }
  llvm_unreachable("Invalid language kind!");
}

const LangStandard *LangStandard::getLangStandardForName(StringRef Name) {
  Kind K = llvm::StringSwitch<Kind>(Name)
#define LANGSTANDARD(id, name, desc, features) \
    .Case(name, lang_##id)
#include "clang/Frontend/LangStandards.def"
    .Default(lang_unspecified);
  if (K == lang_unspecified)
    return nullptr;

  return &getLangStandardForKind(K);
}



//===--- LangOptions.cpp - C Language Family Language Options ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LangOptions.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the LangOptions class.                                 //
//                                                                           //

#include "clang/Basic/LangOptions.h"
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
using namespace clang;

LangOptions::LangOptions() {
#ifdef MS_SUPPORT_VARIABLE_LANGOPTS
#define LANGOPT(Name, Bits, Default, Description) Name = Default;
#define ENUM_LANGOPT(Name, Type, Bits, Default, Description) set##Name(Default);
#include "clang/Basic/LangOptions.def"
#endif
}

void LangOptions::resetNonModularOptions() {
#ifdef MS_SUPPORT_VARIABLE_LANGOPTS
#define LANGOPT(Name, Bits, Default, Description)
#define BENIGN_LANGOPT(Name, Bits, Default, Description) Name = Default;
#define BENIGN_ENUM_LANGOPT(Name, Type, Bits, Default, Description) \
  Name = Default;
#include "clang/Basic/LangOptions.def"
#endif

  // FIXME: This should not be reset; modules can be different with different
  // sanitizer options (this affects __has_feature(address_sanitizer) etc).
  Sanitize.clear();
  SanitizerBlacklistFiles.clear();

  CurrentModule.clear();
  ImplementationOfModule.clear();
}


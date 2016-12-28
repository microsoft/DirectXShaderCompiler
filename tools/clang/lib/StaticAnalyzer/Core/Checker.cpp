//== Checker.cpp - Registration mechanism for checkers -----------*- C++ -*--=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Checker.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines Checker, used to create and register checkers.         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramState.h"
#include "clang/StaticAnalyzer/Core/Checker.h"

using namespace clang;
using namespace ento;

StringRef CheckerBase::getTagDescription() const {
  return getCheckName().getName();
}

CheckName CheckerBase::getCheckName() const { return Name; }

CheckerProgramPointTag::CheckerProgramPointTag(StringRef CheckerName, 
                                               StringRef Msg)
  : SimpleProgramPointTag(CheckerName, Msg) {}

CheckerProgramPointTag::CheckerProgramPointTag(const CheckerBase *Checker,
                                               StringRef Msg)
  : SimpleProgramPointTag(Checker->getCheckName().getName(), Msg) {}

raw_ostream& clang::ento::operator<<(raw_ostream &Out,
                                     const CheckerBase &Checker) {
  Out << Checker.getCheckName().getName();
  return Out;
}

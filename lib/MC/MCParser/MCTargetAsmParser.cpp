//===-- MCTargetAsmParser.cpp - Target Assembly Parser ---------------------==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCTargetAsmParser.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCTargetAsmParser.h"
using namespace llvm;

MCTargetAsmParser::MCTargetAsmParser()
  : AvailableFeatures(0), ParsingInlineAsm(false)
{
}

MCTargetAsmParser::~MCTargetAsmParser() {
}

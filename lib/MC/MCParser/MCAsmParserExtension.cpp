//===-- MCAsmParserExtension.cpp - Asm Parser Hooks -----------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmParserExtension.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCParser/MCAsmParserExtension.h"
using namespace llvm;

MCAsmParserExtension::MCAsmParserExtension() :
  BracketExpressionsSupported(false) {
}

MCAsmParserExtension::~MCAsmParserExtension() {
}

void MCAsmParserExtension::Initialize(MCAsmParser &Parser) {
  this->Parser = &Parser;
}

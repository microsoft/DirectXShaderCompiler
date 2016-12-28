//===-- MCAsmLexer.cpp - Abstract Asm Lexer Interface ---------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmLexer.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;

MCAsmLexer::MCAsmLexer() : CurTok(AsmToken::Error, StringRef()),
                           TokStart(nullptr), SkipSpace(true) {
}

MCAsmLexer::~MCAsmLexer() {
}

SMLoc MCAsmLexer::getLoc() const {
  return SMLoc::getFromPointer(TokStart);
}

SMLoc AsmToken::getLoc() const {
  return SMLoc::getFromPointer(Str.data());
}

SMLoc AsmToken::getEndLoc() const {
  return SMLoc::getFromPointer(Str.data() + Str.size());
}

SMRange AsmToken::getLocRange() const {
  return SMRange(getLoc(), getEndLoc());
}

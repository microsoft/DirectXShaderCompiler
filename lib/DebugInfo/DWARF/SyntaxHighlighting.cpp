//===-- SyntaxHighlighting.cpp ----------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SyntaxHighlighting.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "SyntaxHighlighting.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;
using namespace dwarf;
using namespace syntax;

static cl::opt<cl::boolOrDefault>
    UseColor("color",
             cl::desc("use colored syntax highlighting (default=autodetect)"),
             cl::init(cl::BOU_UNSET));

WithColor::WithColor(llvm::raw_ostream &OS, enum HighlightColor Type) : OS(OS) {
  // Detect color from terminal type unless the user passed the --color option.
  if (UseColor == cl::BOU_UNSET ? OS.has_colors() : UseColor == cl::BOU_TRUE) {
    switch (Type) {
    case Address:    OS.changeColor(llvm::raw_ostream::YELLOW);  break;
    case String:     OS.changeColor(llvm::raw_ostream::GREEN);   break;
    case Tag:        OS.changeColor(llvm::raw_ostream::BLUE);    break;
    case Attribute:  OS.changeColor(llvm::raw_ostream::CYAN);    break;
    case Enumerator: OS.changeColor(llvm::raw_ostream::MAGENTA); break;
    }
  }
}

WithColor::~WithColor() {
  if (UseColor == cl::BOU_UNSET ? OS.has_colors() : UseColor == cl::BOU_TRUE)
    OS.resetColor();
}

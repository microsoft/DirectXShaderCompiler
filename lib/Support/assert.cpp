///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// assert.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "assert.h"

#ifdef _WIN32

#include "dxc/Support/Global.h"
#include "windows.h"

void llvm_assert(const char *Message, const char *File, unsigned Line,
                 const char *Function) {
  OutputDebugFormatA("Error: assert(%s)\nFile:\n%s(%d)\nFunc:\t%s\n", Message,
                     File, Line, Function);
  RaiseException(STATUS_LLVM_ASSERT, 0, 0, 0);
}

#else

#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

void llvm_assert(const char *Message, const char *File, unsigned Line,
                 const char *Function) {
  llvm::errs() << "Error: assert(" << Message << ")\nFile:\n"
               << File << "(" << Line << ")\nFunc:\t" << Function << "\n";
  LLVM_BUILTIN_TRAP;
}

#endif

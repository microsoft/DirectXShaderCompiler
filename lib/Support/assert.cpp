///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// assert.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "assert.h"

#ifdef LLVM_ASSERT_TRAPS
#include "llvm/Support/Compiler.h"
#include <cstdio>
namespace {
void llvm_assert_trap(const char *_Message, const char *_File, unsigned _Line,
                      const char *_Function) {
  fprintf(stderr, "Error: assert(%s)\nFile:\n%s(%d)\nFunc:\t%s\n", _Message,
          _File, _Line, _Function);
  fflush(stderr);
  LLVM_BUILTIN_TRAP;
}
} // namespace
#endif

#ifdef _WIN32
#include "dxc/Support/Global.h"
#include "windows.h"

void llvm_assert(const char *_Message, const char *_File, unsigned _Line,
                 const char *_Function) {
#ifdef LLVM_ASSERT_TRAPS
  llvm_assert_trap(_Message, _File, _Line, _Function);
#else
  OutputDebugFormatA("Error: assert(%s)\nFile:\n%s(%d)\nFunc:\t%s\n", _Message,
                     _File, _Line, _Function);
  RaiseException(STATUS_LLVM_ASSERT, 0, 0, 0);
#endif
}

#else /* _WIN32 */

#include <assert.h>

void llvm_assert(const char *_Message, const char *_File, unsigned _Line,
                 const char *_Function) {
#ifdef LLVM_ASSERT_TRAPS
  llvm_assert_trap(_Message, _File, _Line, _Function);
#else
  (void)_File;
  (void)_Line;
  (void)_Function;
  assert(false && _Message);
#endif
}

#endif /* _WIN32 */

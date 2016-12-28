//===--- ScratchBuffer.h - Scratch space for forming tokens -----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ScratchBuffer.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the ScratchBuffer interface.                           //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_LEX_SCRATCHBUFFER_H
#define LLVM_CLANG_LEX_SCRATCHBUFFER_H

#include "clang/Basic/SourceLocation.h"

namespace clang {
  class SourceManager;

/// ScratchBuffer - This class exposes a simple interface for the dynamic
/// construction of tokens.  This is used for builtin macros (e.g. __LINE__) as
/// well as token pasting, etc.
class ScratchBuffer {
  SourceManager &SourceMgr;
  char *CurBuffer;
  SourceLocation BufferStartLoc;
  unsigned BytesUsed;
public:
  ScratchBuffer(SourceManager &SM);

  /// getToken - Splat the specified text into a temporary MemoryBuffer and
  /// return a SourceLocation that refers to the token.  This is just like the
  /// previous method, but returns a location that indicates the physloc of the
  /// token.
  SourceLocation getToken(const char *Buf, unsigned Len, const char *&DestPtr);

private:
  void AllocScratchBuffer(unsigned RequestLen);
};

} // end namespace clang

#endif

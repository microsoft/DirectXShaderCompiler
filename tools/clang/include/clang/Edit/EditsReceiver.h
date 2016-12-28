//===----- EditedSource.h - Collection of source edits ----------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// EditsReceiver.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_EDIT_EDITSRECEIVER_H
#define LLVM_CLANG_EDIT_EDITSRECEIVER_H

#include "clang/Basic/LLVM.h"

namespace clang {
  class SourceLocation;
  class CharSourceRange;

namespace edit {

class EditsReceiver {
public:
  virtual ~EditsReceiver() { }

  virtual void insert(SourceLocation loc, StringRef text) = 0;
  virtual void replace(CharSourceRange range, StringRef text) = 0;
  /// \brief By default it calls replace with an empty string.
  virtual void remove(CharSourceRange range);
};

}

} // end namespace clang

#endif

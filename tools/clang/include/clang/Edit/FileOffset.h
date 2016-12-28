//===----- FileOffset.h - Offset in a file ----------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FileOffset.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_EDIT_FILEOFFSET_H
#define LLVM_CLANG_EDIT_FILEOFFSET_H

#include "clang/Basic/SourceLocation.h"

namespace clang {

namespace edit {

class FileOffset {
  FileID FID;
  unsigned Offs;
public:
  FileOffset() : Offs(0) { }
  FileOffset(FileID fid, unsigned offs) : FID(fid), Offs(offs) { }

  bool isInvalid() const { return FID.isInvalid(); }

  FileID getFID() const { return FID; }
  unsigned getOffset() const { return Offs; }

  FileOffset getWithOffset(unsigned offset) const {
    FileOffset NewOffs = *this;
    NewOffs.Offs += offset;
    return NewOffs;
  }

  friend bool operator==(FileOffset LHS, FileOffset RHS) {
    return LHS.FID == RHS.FID && LHS.Offs == RHS.Offs;
  }
  friend bool operator!=(FileOffset LHS, FileOffset RHS) {
    return !(LHS == RHS);
  }
  friend bool operator<(FileOffset LHS, FileOffset RHS) {
    return std::tie(LHS.FID, LHS.Offs) < std::tie(RHS.FID, RHS.Offs);
  }
  friend bool operator>(FileOffset LHS, FileOffset RHS) {
    return RHS < LHS;
  }
  friend bool operator>=(FileOffset LHS, FileOffset RHS) {
    return !(LHS < RHS);
  }
  friend bool operator<=(FileOffset LHS, FileOffset RHS) {
    return !(RHS < LHS);
  }
};

}

} // end namespace clang

#endif

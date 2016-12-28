//===----- EditedSource.h - Collection of source edits ----------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// EditedSource.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_EDIT_EDITEDSOURCE_H
#define LLVM_CLANG_EDIT_EDITEDSOURCE_H

#include "clang/Edit/FileOffset.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"
#include <map>

namespace clang {
  class LangOptions;
  class PPConditionalDirectiveRecord;

namespace edit {
  class Commit;
  class EditsReceiver;

class EditedSource {
  const SourceManager &SourceMgr;
  const LangOptions &LangOpts;
  const PPConditionalDirectiveRecord *PPRec;

  struct FileEdit {
    StringRef Text;
    unsigned RemoveLen;

    FileEdit() : RemoveLen(0) {}
  };

  typedef std::map<FileOffset, FileEdit> FileEditsTy;
  FileEditsTy FileEdits;

  llvm::DenseMap<unsigned, SourceLocation> ExpansionToArgMap;

  llvm::BumpPtrAllocator StrAlloc;

public:
  EditedSource(const SourceManager &SM, const LangOptions &LangOpts,
               const PPConditionalDirectiveRecord *PPRec = nullptr)
    : SourceMgr(SM), LangOpts(LangOpts), PPRec(PPRec),
      StrAlloc() { }

  const SourceManager &getSourceManager() const { return SourceMgr; }
  const LangOptions &getLangOpts() const { return LangOpts; }
  const PPConditionalDirectiveRecord *getPPCondDirectiveRecord() const {
    return PPRec;
  }

  bool canInsertInOffset(SourceLocation OrigLoc, FileOffset Offs);

  bool commit(const Commit &commit);
  
  void applyRewrites(EditsReceiver &receiver);
  void clearRewrites();

  StringRef copyString(StringRef str) {
    char *buf = StrAlloc.Allocate<char>(str.size());
    std::memcpy(buf, str.data(), str.size());
    return StringRef(buf, str.size());
  }
  StringRef copyString(const Twine &twine);

private:
  bool commitInsert(SourceLocation OrigLoc, FileOffset Offs, StringRef text,
                    bool beforePreviousInsertions);
  bool commitInsertFromRange(SourceLocation OrigLoc, FileOffset Offs,
                             FileOffset InsertFromRangeOffs, unsigned Len,
                             bool beforePreviousInsertions);
  void commitRemove(SourceLocation OrigLoc, FileOffset BeginOffs, unsigned Len);

  StringRef getSourceText(FileOffset BeginOffs, FileOffset EndOffs,
                          bool &Invalid);
  FileEditsTy::iterator getActionForOffset(FileOffset Offs);
};

}

} // end namespace clang

#endif

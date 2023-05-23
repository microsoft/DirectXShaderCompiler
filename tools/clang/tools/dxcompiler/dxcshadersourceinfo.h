///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcshadersourceinfo.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Utility helpers for dealing with DXIL part related to shader sources      //
// and options.                                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <stdint.h>
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include "dxc/DxilContainer/DxilContainer.h"

namespace clang {
  class CodeGenOptions;
  class SourceManager;
}

namespace hlsl {

  namespace options {
    struct ArgPair;
  }

// TODO: Move this type to its own library.
struct SourceInfoReader {
  using Buffer = std::vector<uint8_t>;
  Buffer m_UncompressedSources;

  struct Source {
    llvm::StringRef Name;
    llvm::StringRef Content;
  };

  std::vector<Source> m_Sources;
  std::vector<options::ArgPair> m_ArgPairs;

  Source GetSource(unsigned i) const { return m_Sources[i]; }
  unsigned GetSourcesCount() const { return m_Sources.size(); }

  const options::ArgPair &GetArgPair(unsigned i) const { return m_ArgPairs[i]; }
  unsigned GetArgPairCount() const { return m_ArgPairs.size(); }

  // Note: The memory for SourceInfo must outlive this structure.
  bool Init(const hlsl::DxilSourceInfo *SourceInfo, unsigned sourceInfoSize);
};

// Herper for writing the shader source part.
struct SourceInfoWriter {
  using Buffer = std::vector<uint8_t>;
  Buffer m_Buffer;

  const hlsl::DxilSourceInfo *GetPart() const;
  void Write(clang::CodeGenOptions &cgOpts, llvm::ArrayRef<options::ArgPair> argPairs, clang::SourceManager &srcMgr);
};

} // namespace hlsl;

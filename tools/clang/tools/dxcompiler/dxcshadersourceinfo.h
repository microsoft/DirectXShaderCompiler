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
#include "dxc/DxilContainer/DxilContainer.h"

namespace clang {
  class CodeGenOptions;
  class SourceManager;
}

namespace hlsl {

struct SourceInfo {
  struct Source {
    llvm::StringRef Path;
    llvm::StringRef Content;
  };

  llvm::StringRef MainFile;
  std::vector<llvm::StringRef> Args;
  std::vector<llvm::StringRef> Defines;
  std::vector<Source> Sources;
};

struct SourceInfoReader {
  using Buffer = std::vector<uint8_t>;
  Buffer m_UncompressedSources;

  struct Source {
    llvm::StringRef Name;
    llvm::StringRef Content;
  };

  std::vector<Source> m_Sources;
  llvm::StringRef m_Defines;
  llvm::StringRef m_Args;

  llvm::StringRef GetArgs() const;
  llvm::StringRef GetDefines() const;
  Source GetSource(unsigned i) const;
  unsigned GetSourcesCount() const;
  void Read(const hlsl::DxilSourceInfo *SourceInfo);
};

// Herper for writing the shader source part.
struct SourceInfoWriter {
  using Buffer = std::vector<uint8_t>;
  Buffer m_Buffer;

  const hlsl::DxilSourceInfo *GetPart() const;
  void Write(clang::CodeGenOptions &cgOpts, clang::SourceManager &srcMgr);
};

} // namespace hlsl;

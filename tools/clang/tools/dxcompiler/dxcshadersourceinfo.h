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

struct SourceInfoReader {
  using Buffer = std::vector<uint8_t>;
  Buffer m_UncompressedSources;

  struct Source {
    llvm::StringRef Name;
    llvm::StringRef Content;
  };

  struct ArgPair {
    std::string Name;
    std::string Value;
    inline std::string Render() const {
      if (Name.size())
        return std::string("-") + Name + Value;
      return Value;
    }
  };

  std::vector<Source> m_Sources;
  std::vector<ArgPair> m_ArgPairs;

  Source GetSource(unsigned i) const;
  unsigned GetSourcesCount() const;
  unsigned GetArgPairCount() const { return m_ArgPairs.size(); }
  const ArgPair &GetArgPair(unsigned i) const { return m_ArgPairs[i]; }
  void Read(const hlsl::DxilSourceInfo *SourceInfo);
};

// Herper for writing the shader source part.
struct SourceInfoWriter {
  using Buffer = std::vector<uint8_t>;
  Buffer m_Buffer;

  const hlsl::DxilSourceInfo *GetPart() const;
  void Write(llvm::StringRef targetProfile, llvm::StringRef entryPoint, clang::CodeGenOptions &cgOpts, clang::SourceManager &srcMgr);
};

} // namespace hlsl;

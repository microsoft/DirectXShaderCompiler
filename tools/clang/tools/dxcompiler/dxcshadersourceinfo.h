///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcshadersourceinfo.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Has utility helpers for dealing with DXIL part related to shader sources  //
// and options.                                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <stdint.h>
#include "llvm/ADT/StringRef.h"

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

// Herper for writing the shader source part.
struct SourceInfoWriter {
  typedef std::vector<uint8_t> Buffer;
  Buffer m_Buffer;
  llvm::StringRef GetBuffer();
  void Write(clang::CodeGenOptions &CodeGenOpts, clang::SourceManager &srcMgr);
};

} // namespace hlsl;

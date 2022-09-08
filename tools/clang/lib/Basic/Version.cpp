//===- Version.cpp - Clang Version Number -----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines several version-related utility functions for Clang.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/Version.h"
#include "clang/Basic/LLVM.h"
#include "clang/Config/config.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <cstring>

#ifdef HAVE_SVN_VERSION_INC
#  include "SVNVersion.inc"
#endif

#include "dxcversion.inc" // HLSL Change

namespace clang {

std::string getClangRepositoryPath() {
#ifdef HLSL_FIXED_VER // HLSL Change Starts
  return std::string();
#else
#if defined(CLANG_REPOSITORY_STRING)
  return CLANG_REPOSITORY_STRING;
#else
#ifdef SVN_REPOSITORY
  StringRef URL(SVN_REPOSITORY);
#else
  StringRef URL("");
#endif

  // If the SVN_REPOSITORY is empty, try to use the SVN keyword. This helps us
  // pick up a tag in an SVN export, for example.
  StringRef SVNRepository("$URL: https://llvm.org/svn/llvm-project/cfe/tags/RELEASE_370/final/lib/Basic/Version.cpp $");
  if (URL.empty()) {
    URL = SVNRepository.slice(SVNRepository.find(':'),
                              SVNRepository.find("/lib/Basic"));
  }

  // Strip off version from a build from an integration branch.
  URL = URL.slice(0, URL.find("/src/tools/clang"));

  // Trim path prefix off, assuming path came from standard cfe path.
  size_t Start = URL.find("cfe/");
  if (Start != StringRef::npos)
    URL = URL.substr(Start + 4);

  return URL;
#endif
#endif // HLSL Change Ends
}

std::string getLLVMRepositoryPath() {
#ifdef HLSL_FIXED_VER // HLSL Change Starts
  return std::string();
#else
#ifdef LLVM_REPOSITORY
  StringRef URL(LLVM_REPOSITORY);
#else
  StringRef URL("");
#endif

  // Trim path prefix off, assuming path came from standard llvm path.
  // Leave "llvm/" prefix to distinguish the following llvm revision from the
  // clang revision.
  size_t Start = URL.find("llvm/");
  if (Start != StringRef::npos)
    URL = URL.substr(Start);

  return URL;
#endif // HLSL Change Ends
}

std::string getClangRevision() {
#ifdef HLSL_FIXED_VER // HLSL Change Starts
  return std::string();
#else
#ifdef SVN_REVISION
  return SVN_REVISION;
#else
  return "";
#endif
#endif // HLSL Change Ends
}

std::string getLLVMRevision() {
#ifdef HLSL_FIXED_VER // HLSL Change Starts
  return std::string();
#else
#ifdef LLVM_REVISION
  return LLVM_REVISION;
#else
  return "";
#endif
#endif // HLSL Change Ends
}

std::string getClangFullRepositoryVersion() {
#ifdef HLSL_FIXED_VER // HLSL Change Starts
  return std::string();
#else
  std::string buf;
  llvm::raw_string_ostream OS(buf);
  std::string Path = getClangRepositoryPath();
  std::string Revision = getClangRevision();
  if (!Path.empty() || !Revision.empty()) {
    OS << '(';
    if (!Path.empty())
      OS << Path;
    if (!Revision.empty()) {
      if (!Path.empty())
        OS << ' ';
      OS << Revision;
    }
    OS << ')';
  }
  // Support LLVM in a separate repository.
  std::string LLVMRev = getLLVMRevision();
  if (!LLVMRev.empty() && LLVMRev != Revision) {
    OS << " (";
    std::string LLVMRepo = getLLVMRepositoryPath();
    if (!LLVMRepo.empty())
      OS << LLVMRepo << ' ';
    OS << LLVMRev << ')';
  }
  return OS.str();
#endif
}

std::string getClangFullVersion() {
#ifdef HLSL_TOOL_NAME
  return getClangToolFullVersion(HLSL_TOOL_NAME);
#else
  return getClangToolFullVersion("clang");
#endif
}

std::string getClangToolFullVersion(StringRef ToolName) {
  std::string buf;
  llvm::raw_string_ostream OS(buf);
#ifdef CLANG_VENDOR
  OS << CLANG_VENDOR;
#endif

#ifdef RC_PRODUCT_VERSION // HLSL Change Starts
  OS << ToolName << " " << RC_PRODUCT_VERSION;
#else
  OS << ToolName << " version " CLANG_VERSION_STRING " "
     << getClangFullRepositoryVersion();
#endif  // HLSL Change Ends

  // If vendor supplied, include the base LLVM version as well.
#ifdef CLANG_VENDOR
  OS << " (based on " << BACKEND_PACKAGE_STRING << ")";
#endif

  return OS.str();
}

std::string getClangFullCPPVersion() {
  // The version string we report in __VERSION__ is just a compacted version of
  // the one we report on the command line.
  std::string buf;
  llvm::raw_string_ostream OS(buf);
#ifdef CLANG_VENDOR
  OS << CLANG_VENDOR;
#endif

// HLSL Change Starts
#ifdef HLSL_TOOL_NAME
  OS << HLSL_TOOL_NAME;
#endif
#ifdef RC_FILE_VERSION
  OS << " " << RC_VERSION_FIELD_1 << "." << RC_VERSION_FIELD_2 << "."
     << RC_VERSION_FIELD_3 << "." << RC_VERSION_FIELD_4;
#else
  OS << "unofficial";
#endif
// HLSL Change Ends

  return OS.str();
}

// HLSL Change Starts
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "GitCommitInfo.inc"
uint32_t getGitCommitCount() { return kGitCommitCount; }
const char *getGitCommitHash() { return kGitCommitHash; }
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
// HLSL Change Ends

} // end namespace clang

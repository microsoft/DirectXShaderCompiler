//===--- InitHeaderSearch.cpp - Initialize header search paths ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the InitHeaderSearch class.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/Utils.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Config/config.h" // C_INCLUDE_DIRS
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::frontend;

namespace {

/// InitHeaderSearch - This class makes it easier to set the search paths of
///  a HeaderSearch object. InitHeaderSearch stores several search path lists
///  internally, which can be sent to a HeaderSearch object in one swoop.
class InitHeaderSearch {
  std::vector<std::pair<IncludeDirGroup, DirectoryLookup> > IncludePath;
  typedef std::vector<std::pair<IncludeDirGroup,
                      DirectoryLookup> >::const_iterator path_iterator;
  std::vector<std::pair<std::string, bool> > SystemHeaderPrefixes;
  HeaderSearch &Headers;
  bool Verbose;
  std::string IncludeSysroot;
  bool HasSysroot;

public:

  InitHeaderSearch(HeaderSearch &HS, bool verbose, StringRef sysroot)
    : Headers(HS), Verbose(verbose), IncludeSysroot(sysroot),
      HasSysroot(!(sysroot.empty() || sysroot == "/")) {
  }

  /// AddPath - Add the specified path to the specified group list, prefixing
  /// the sysroot if used.
  void AddPath(const Twine &Path, IncludeDirGroup Group, bool isFramework);

  /// AddUnmappedPath - Add the specified path to the specified group list,
  /// without performing any sysroot remapping.
  void AddUnmappedPath(const Twine &Path, IncludeDirGroup Group,
                       bool isFramework);

  /// AddSystemHeaderPrefix - Add the specified prefix to the system header
  /// prefix list.
  void AddSystemHeaderPrefix(StringRef Prefix, bool IsSystemHeader) {
    SystemHeaderPrefixes.emplace_back(Prefix, IsSystemHeader);
  }

  /// AddGnuCPlusPlusIncludePaths - Add the necessary paths to support a gnu
  ///  libstdc++.
  void AddGnuCPlusPlusIncludePaths(StringRef Base,
                                   StringRef ArchDir,
                                   StringRef Dir32,
                                   StringRef Dir64,
                                   const llvm::Triple &triple);

  /// AddMinGWCPlusPlusIncludePaths - Add the necessary paths to support a MinGW
  ///  libstdc++.
  void AddMinGWCPlusPlusIncludePaths(StringRef Base,
                                     StringRef Arch,
                                     StringRef Version);

  // AddDefaultCIncludePaths - Add paths that should always be searched.
  void AddDefaultCIncludePaths(const llvm::Triple &triple,
                               const HeaderSearchOptions &HSOpts);

  // AddDefaultCPlusPlusIncludePaths -  Add paths that should be searched when
  //  compiling c++.
  void AddDefaultCPlusPlusIncludePaths(const llvm::Triple &triple,
                                       const HeaderSearchOptions &HSOpts);

  /// AddDefaultSystemIncludePaths - Adds the default system include paths so
  ///  that e.g. stdio.h is found.
  void AddDefaultIncludePaths(const LangOptions &Lang,
                              const llvm::Triple &triple,
                              const HeaderSearchOptions &HSOpts);

  /// Realize - Merges all search path lists into one list and send it to
  /// HeaderSearch.
  void Realize(const LangOptions &Lang);
};

}  // end anonymous namespace.

static bool CanPrefixSysroot(StringRef Path) {
#if defined(LLVM_ON_WIN32)
  return !Path.empty() && llvm::sys::path::is_separator(Path[0]);
#else
  return llvm::sys::path::is_absolute(Path);
#endif
}

void InitHeaderSearch::AddPath(const Twine &Path, IncludeDirGroup Group,
                               bool isFramework) {
  // Add the path with sysroot prepended, if desired and this is a system header
  // group.
  if (HasSysroot) {
    SmallString<256> MappedPathStorage;
    StringRef MappedPathStr = Path.toStringRef(MappedPathStorage);
    if (CanPrefixSysroot(MappedPathStr)) {
      AddUnmappedPath(IncludeSysroot + Path, Group, isFramework);
      return;
    }
  }

  AddUnmappedPath(Path, Group, isFramework);
}

void InitHeaderSearch::AddUnmappedPath(const Twine &Path, IncludeDirGroup Group,
                                       bool isFramework) {
  assert(!Path.isTriviallyEmpty() && "can't handle empty path here");

  FileManager &FM = Headers.getFileMgr();
  SmallString<256> MappedPathStorage;
  StringRef MappedPathStr = Path.toStringRef(MappedPathStorage);

#if 1 // HLSL Change Starts - simplify mapping based on actual usage
  assert(Group == Angled);
  assert(!isFramework);

  // If the directory exists, add it.
  if (const DirectoryEntry *DE = FM.getDirectory(MappedPathStr)) {
    IncludePath.push_back(
      std::make_pair(Angled, DirectoryLookup(DE, SrcMgr::C_User, false)));
    return;
  }
#else
  // Compute the DirectoryLookup type.
  SrcMgr::CharacteristicKind Type;
  if (Group == Quoted || Group == Angled || Group == IndexHeaderMap) {
    Type = SrcMgr::C_User;
  } else if (Group == ExternCSystem) {
    Type = SrcMgr::C_ExternCSystem;
  } else {
    Type = SrcMgr::C_System;
  }

  // If the directory exists, add it.
  if (const DirectoryEntry *DE = FM.getDirectory(MappedPathStr)) {
    IncludePath.push_back(
      std::make_pair(Group, DirectoryLookup(DE, Type, isFramework)));
    return;
  }

  // Check to see if this is an apple-style headermap (which are not allowed to
  // be frameworks).
  if (!isFramework) {
    if (const FileEntry *FE = FM.getFile(MappedPathStr)) {
      if (const HeaderMap *HM = Headers.CreateHeaderMap(FE)) {
        // It is a headermap, add it to the search path.
        IncludePath.push_back(
          std::make_pair(Group,
                         DirectoryLookup(HM, Type, Group == IndexHeaderMap)));
        return;
      }
    }
  }

  if (Verbose)
    llvm::errs() << "ignoring nonexistent directory \""
                 << MappedPathStr << "\"\n";
#endif // HLSL Change Ends - simplify mapping based on actual usage
}

// HLSL Change: Remove unused function InitHeaderSearch::AddGnuCPlusPlusIncludePaths

// HLSL Change: Remove unused function InitHeaderSearch::AddMinGWCPlusPlusIncludePath

// HLSL Change: Remove unused function InitHeaderSearch::AddDefaultCIncludePaths

// HLSL Change: Remove unused function InitHeaderSearch::AddDefaultCPlusPlusIncludePaths

void InitHeaderSearch::AddDefaultIncludePaths(const LangOptions &Lang,
                                              const llvm::Triple &triple,
                                            const HeaderSearchOptions &HSOpts) {
#if 1 // HLSL Change Starts
  return;
#else
  // NB: This code path is going away. All of the logic is moving into the
  // driver which has the information necessary to do target-specific
  // selections of default include paths. Each target which moves there will be
  // exempted from this logic here until we can delete the entire pile of code.
  switch (triple.getOS()) {
  default:
    break; // Everything else continues to use this routine's logic.

  case llvm::Triple::Linux:
    return;

  case llvm::Triple::Win32:
    if (triple.getEnvironment() != llvm::Triple::Cygnus ||
        triple.isOSBinFormatMachO())
      return;
    break;
  }

  if (Lang.CPlusPlus && HSOpts.UseStandardCXXIncludes &&
      HSOpts.UseStandardSystemIncludes) {
    if (HSOpts.UseLibcxx) {
      if (triple.isOSDarwin()) {
        // On Darwin, libc++ may be installed alongside the compiler in
        // include/c++/v1.
        if (!HSOpts.ResourceDir.empty()) {
          // Remove version from foo/lib/clang/version
          StringRef NoVer = llvm::sys::path::parent_path(HSOpts.ResourceDir);
          // Remove clang from foo/lib/clang
          StringRef Lib = llvm::sys::path::parent_path(NoVer);
          // Remove lib from foo/lib
          SmallString<128> P = llvm::sys::path::parent_path(Lib);

          // Get foo/include/c++/v1
          llvm::sys::path::append(P, "include", "c++", "v1");
          AddUnmappedPath(P, CXXSystem, false);
        }
      }
      // On Solaris, include the support directory for things like xlocale and
      // fudged system headers.
      if (triple.getOS() == llvm::Triple::Solaris) 
        AddPath("/usr/include/c++/v1/support/solaris", CXXSystem, false);
      
      AddPath("/usr/include/c++/v1", CXXSystem, false);
    } else {
      AddDefaultCPlusPlusIncludePaths(triple, HSOpts);
    }
  }

  AddDefaultCIncludePaths(triple, HSOpts);

  // Add the default framework include paths on Darwin.
  if (HSOpts.UseStandardSystemIncludes) {
    if (triple.isOSDarwin()) {
      AddPath("/System/Library/Frameworks", System, true);
      AddPath("/Library/Frameworks", System, true);
    }
  }
#endif // HLSL Change Ends
}

/// RemoveDuplicates - If there are duplicate directory entries in the specified
/// search list, remove the later (dead) ones.  Returns the number of non-system
/// headers removed, which is used to update NumAngled.
static unsigned RemoveDuplicates(std::vector<DirectoryLookup> &SearchList,
                                 unsigned First, bool Verbose) {
  llvm::SmallPtrSet<const DirectoryEntry *, 8> SeenDirs;
  llvm::SmallPtrSet<const DirectoryEntry *, 8> SeenFrameworkDirs;
  llvm::SmallPtrSet<const HeaderMap *, 8> SeenHeaderMaps;
  unsigned NonSystemRemoved = 0;
  for (unsigned i = First; i != SearchList.size(); ++i) {
    unsigned DirToRemove = i;

    const DirectoryLookup &CurEntry = SearchList[i];

    if (CurEntry.isNormalDir()) {
      // If this isn't the first time we've seen this dir, remove it.
      if (SeenDirs.insert(CurEntry.getDir()).second)
        continue;
    } else if (CurEntry.isFramework()) {
      // If this isn't the first time we've seen this framework dir, remove it.
      if (SeenFrameworkDirs.insert(CurEntry.getFrameworkDir()).second)
        continue;
    } else {
      assert(CurEntry.isHeaderMap() && "Not a headermap or normal dir?");
      // If this isn't the first time we've seen this headermap, remove it.
      if (SeenHeaderMaps.insert(CurEntry.getHeaderMap()).second)
        continue;
    }

    // If we have a normal #include dir/framework/headermap that is shadowed
    // later in the chain by a system include location, we actually want to
    // ignore the user's request and drop the user dir... keeping the system
    // dir.  This is weird, but required to emulate GCC's search path correctly.
    //
    // Since dupes of system dirs are rare, just rescan to find the original
    // that we're nuking instead of using a DenseMap.
    if (CurEntry.getDirCharacteristic() != SrcMgr::C_User) {
      // Find the dir that this is the same of.
      unsigned FirstDir;
      for (FirstDir = 0; ; ++FirstDir) {
        assert(FirstDir != i && "Didn't find dupe?");

        const DirectoryLookup &SearchEntry = SearchList[FirstDir];

        // If these are different lookup types, then they can't be the dupe.
        if (SearchEntry.getLookupType() != CurEntry.getLookupType())
          continue;

        bool isSame;
        if (CurEntry.isNormalDir())
          isSame = SearchEntry.getDir() == CurEntry.getDir();
        else if (CurEntry.isFramework())
          isSame = SearchEntry.getFrameworkDir() == CurEntry.getFrameworkDir();
        else {
          assert(CurEntry.isHeaderMap() && "Not a headermap or normal dir?");
          isSame = SearchEntry.getHeaderMap() == CurEntry.getHeaderMap();
        }

        if (isSame)
          break;
      }

      // If the first dir in the search path is a non-system dir, zap it
      // instead of the system one.
      if (SearchList[FirstDir].getDirCharacteristic() == SrcMgr::C_User)
        DirToRemove = FirstDir;
    }

    if (Verbose) {
      llvm::errs() << "ignoring duplicate directory \""
                   << CurEntry.getName() << "\"\n";
      if (DirToRemove != i)
        llvm::errs() << "  as it is a non-system directory that duplicates "
                     << "a system directory\n";
    }
    if (DirToRemove != i)
      ++NonSystemRemoved;

    // This is reached if the current entry is a duplicate.  Remove the
    // DirToRemove (usually the current dir).
    SearchList.erase(SearchList.begin()+DirToRemove);
    --i;
  }
  return NonSystemRemoved;
}


void InitHeaderSearch::Realize(const LangOptions &Lang) {
  // Concatenate ANGLE+SYSTEM+AFTER chains together into SearchList.
  std::vector<DirectoryLookup> SearchList;
  SearchList.reserve(IncludePath.size());

  // Quoted arguments go first.
  for (path_iterator it = IncludePath.begin(), ie = IncludePath.end();
       it != ie; ++it) {
    if (it->first == Quoted)
      SearchList.push_back(it->second);
  }
  // Deduplicate and remember index.
  RemoveDuplicates(SearchList, 0, Verbose);
  unsigned NumQuoted = SearchList.size();

  for (path_iterator it = IncludePath.begin(), ie = IncludePath.end();
       it != ie; ++it) {
    if (it->first == Angled || it->first == IndexHeaderMap)
      SearchList.push_back(it->second);
  }

  RemoveDuplicates(SearchList, NumQuoted, Verbose);
  unsigned NumAngled = SearchList.size();

  for (path_iterator it = IncludePath.begin(), ie = IncludePath.end();
       it != ie; ++it) {
    if (it->first == System || it->first == ExternCSystem ||
        (!Lang.ObjC1 && !Lang.CPlusPlus && it->first == CSystem)    ||
        (/*FIXME !Lang.ObjC1 && */Lang.CPlusPlus  && it->first == CXXSystem)  ||
        (Lang.ObjC1  && !Lang.CPlusPlus && it->first == ObjCSystem) ||
        (Lang.ObjC1  && Lang.CPlusPlus  && it->first == ObjCXXSystem))
      SearchList.push_back(it->second);
  }

  for (path_iterator it = IncludePath.begin(), ie = IncludePath.end();
       it != ie; ++it) {
    if (it->first == After)
      SearchList.push_back(it->second);
  }

  // Remove duplicates across both the Angled and System directories.  GCC does
  // this and failing to remove duplicates across these two groups breaks
  // #include_next.
  unsigned NonSystemRemoved = RemoveDuplicates(SearchList, NumQuoted, Verbose);
  NumAngled -= NonSystemRemoved;

  bool DontSearchCurDir = false;  // TODO: set to true if -I- is set?
  Headers.SetSearchPaths(SearchList, NumQuoted, NumAngled, DontSearchCurDir);

  Headers.SetSystemHeaderPrefixes(SystemHeaderPrefixes);

  // If verbose, print the list of directories that will be searched.
  if (Verbose) {
    llvm::errs() << "#include \"...\" search starts here:\n";
    for (unsigned i = 0, e = SearchList.size(); i != e; ++i) {
      if (i == NumQuoted)
        llvm::errs() << "#include <...> search starts here:\n";
      const char *Name = SearchList[i].getName();
      const char *Suffix;
      if (SearchList[i].isNormalDir())
        Suffix = "";
      else if (SearchList[i].isFramework())
        Suffix = " (framework directory)";
      else {
        assert(SearchList[i].isHeaderMap() && "Unknown DirectoryLookup");
        Suffix = " (headermap)";
      }
      llvm::errs() << " " << Name << Suffix << "\n";
    }
    llvm::errs() << "End of search list.\n";
  }
}

void clang::ApplyHeaderSearchOptions(HeaderSearch &HS,
                                     const HeaderSearchOptions &HSOpts,
                                     const LangOptions &Lang,
                                     const llvm::Triple &Triple) {
  InitHeaderSearch Init(HS, HSOpts.Verbose, HSOpts.Sysroot);

  // Add the user defined entries.
  for (unsigned i = 0, e = HSOpts.UserEntries.size(); i != e; ++i) {
    const HeaderSearchOptions::Entry &E = HSOpts.UserEntries[i];
    if (E.IgnoreSysRoot) {
      Init.AddUnmappedPath(E.Path, E.Group, E.IsFramework);
    } else {
      Init.AddPath(E.Path, E.Group, E.IsFramework);
    }
  }

  Init.AddDefaultIncludePaths(Lang, Triple, HSOpts);

  for (unsigned i = 0, e = HSOpts.SystemHeaderPrefixes.size(); i != e; ++i)
    Init.AddSystemHeaderPrefix(HSOpts.SystemHeaderPrefixes[i].Prefix,
                               HSOpts.SystemHeaderPrefixes[i].IsSystemHeader);

  if (HSOpts.UseBuiltinIncludes) {
    // Set up the builtin include directory in the module map.
    SmallString<128> P = StringRef(HSOpts.ResourceDir);
    llvm::sys::path::append(P, "include");
    if (const DirectoryEntry *Dir = HS.getFileMgr().getDirectory(P))
      HS.getModuleMap().setBuiltinIncludeDir(Dir);
  }

  Init.Realize(Lang);
}

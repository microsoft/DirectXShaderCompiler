///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLSLOptions.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Support for command-line-style option parsing.                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef LLVM_HLSL_OPTIONS_H
#define LLVM_HLSL_OPTIONS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/ArgList.h"
#include "dxc/dxcapi.h"

namespace llvm {
namespace opt {
class OptTable;
class raw_ostream;
}
}

namespace dxc {
class DxcDllSupport;
}

namespace hlsl {

namespace options {
/// Flags specifically for clang options.  Must not overlap with
/// llvm::opt::DriverFlag or (for clarity) with clang::driver::options.
enum HlslFlags {
  DriverOption = (1 << 13),
  NoArgumentUnused = (1 << 14),
  CoreOption = (1 << 15),
  ISenseOption = (1 << 16),
};

enum ID {
    OPT_INVALID = 0, // This is not an option ID.
#define OPTION(PREFIX, NAME, ID, KIND, GROUP, ALIAS, ALIASARGS, FLAGS, PARAM, \
               HELPTEXT, METAVAR) OPT_##ID,
#include "dxc/Support/HLSLOptions.inc"
    LastOption
#undef OPTION
  };

const llvm::opt::OptTable *getHlslOptTable();

///////////////////////////////////////////////////////////////////////////////
// Helper classes to deal with options.

/// Flags for IDxcCompiler APIs.
static const unsigned CompilerFlags = HlslFlags::CoreOption;
/// Flags for dxc.exe command-line tool.
static const unsigned DxcFlags = HlslFlags::CoreOption | HlslFlags::DriverOption;
/// Flags for dxr.exe command-line tool.
static const unsigned DxrFlags = HlslFlags::CoreOption | HlslFlags::DriverOption;
/// Flags for IDxcIntelliSense APIs.
static const unsigned ISenseFlags = HlslFlags::CoreOption | HlslFlags::ISenseOption;

/// Use this class to capture preprocessor definitions and manage their lifetime.
class DxcDefines {
public:
  void push_back(llvm::StringRef value);
  LPWSTR DefineValues = nullptr;
  llvm::SmallVector<llvm::StringRef, 8> DefineStrings;
  llvm::SmallVector<DxcDefine, 8> DefineVector;

  ~DxcDefines() { delete[] DefineValues; }
  DxcDefines(const DxcDefines &) = delete;
  DxcDefines() {}
  void BuildDefines(); // Must be called after all defines are pushed back
  UINT32 ComputeNumberOfWCharsNeededForDefines();
  const DxcDefine *data() const { return DefineVector.data(); }
  unsigned size() const { return DefineVector.size(); }
};

/// Use this class to capture all options.
class DxcOpts {
public:
  DxcDefines Defines;
  llvm::opt::InputArgList Args = llvm::opt::InputArgList(nullptr, nullptr); // Original arguments.

  llvm::StringRef AssemblyCode; // OPT_Fc
  llvm::StringRef DebugFile;    // OPT_Fd
  llvm::StringRef EntryPoint;   // OPT_entrypoint
  llvm::StringRef ExternalFn;   // OPT_external_fn
  llvm::StringRef ExternalLib;  // OPT_external_lib
  llvm::StringRef ExtractPrivateFile; // OPT_getprivate
  llvm::StringRef ForceRootSigVer; // OPT_force_rootsig_ver
  llvm::StringRef InputFile; // OPT_INPUT
  llvm::StringRef OutputHeader; // OPT_Fh
  llvm::StringRef OutputObject; // OPT_Fo
  llvm::StringRef OutputWarningsFile; // OPT_Fe
  llvm::StringRef Preprocess; // OPT_P
  llvm::StringRef TargetProfile; // OPT_target_profile
  llvm::StringRef VariableName; // OPT_Vn
  llvm::StringRef PrivateSource; // OPT_setprivate
  llvm::StringRef RootSignatureSource; // OPT_setrootsignature
  llvm::StringRef VerifyRootSignatureSource; //OPT_verifyrootsignature
  llvm::StringRef RootSignatureDefine; // OPT_rootsig_define

  bool AllResourcesBound; // OPT_all_resources_bound
  bool AstDump; // OPT_ast_dump
  bool GenSPIRV; // OPT_spirv // SPIRV change
  bool ColorCodeAssembly; // OPT_Cc
  bool CodeGenHighLevel; // OPT_fcgl
  bool DebugInfo; // OPT__SLASH_Zi
  bool DumpBin;        // OPT_dumpbin
  bool WarningAsError; // OPT__SLASH_WX
  bool IEEEStrict;     // OPT_Gis
  bool DefaultColMajor;  // OPT_Zpc
  bool DefaultRowMajor;  // OPT_Zpr
  bool DisableValidation; // OPT_VD
  unsigned OptLevel;      // OPT_O0/O1/O2/O3
  bool DisableOptimizations; // OPT_Od
  bool AvoidFlowControl;     // OPT_Gfa
  bool PreferFlowControl;    // OPT_Gfp
  bool EnableStrictMode;     // OPT_Ges
  bool HLSL2015;  // OPT_hlsl_version (=2015)
  bool HLSL2016;  // OPT_hlsl_version (=2016)
  bool OptDump; // OPT_ODump - dump optimizer commands
  bool OutputWarnings = true; // OPT_no_warnings
  bool ShowHelp = false;  // OPT_help
  bool UseColor; // OPT_Cc
  bool UseHexLiterals; // OPT_Lx
  bool UseInstructionByteOffsets; // OPT_No
  bool UseInstructionNumbers; // OPT_Ni
  bool NotUseLegacyCBufLoad;  // OPT_not_use_legacy_cbuf_load
  bool PackPrefixStable;  // OPT_pack_prefix_stable
  bool PackOptimized;  // OPT_pack_optimized
  bool DisplayIncludeProcess; // OPT__vi
  bool RecompileFromBinary; // OPT _Recompile (Recompiling the DXBC binary file not .hlsl file)
  bool StripDebug; // OPT Qstrip_debug
  bool StripRootSignature; // OPT_Qstrip_rootsignature
  bool StripPrivate; // OPT_Qstrip_priv
  bool StripReflection; // OPT_Qstrip_reflect
  bool ExtractRootSignature; // OPT_extractrootsignature
  bool DisassembleColorCoded; // OPT_Cc
  bool DisassembleInstNumbers; //OPT_Ni
  bool DisassembleByteOffset; //OPT_No
  bool DisaseembleHex; //OPT_Lx
  bool IsRootSignatureProfile();
};

/// Use this class to capture, convert and handle the lifetime for the
/// command-line arguments to a program.
class MainArgs {
public:
  llvm::SmallVector<std::string, 8> Utf8StringVector;
  llvm::SmallVector<const char *, 8> Utf8CharPtrVector;

  MainArgs() = default;
  MainArgs(int argc, const wchar_t **argv, int skipArgCount = 1);
  MainArgs(llvm::ArrayRef<llvm::StringRef> args);
  MainArgs& operator=(const MainArgs &other);
  llvm::ArrayRef<const char *> getArrayRef() const {
    return llvm::ArrayRef<const char *>(Utf8CharPtrVector.data(),
      Utf8CharPtrVector.size());
  }
};

/// Use this class to convert a StringRef into a wstring, handling empty values as nulls.
class StringRefUtf16 {
private:
  std::wstring m_value;

public:
  StringRefUtf16(llvm::StringRef value);
  operator LPCWSTR() const { return m_value.size() ? m_value.data() : nullptr; }
};

/// Reads all options from the given argument strings, populates opts, and
/// validates reporting errors and warnings.
int ReadDxcOpts(const llvm::opt::OptTable *optionTable, unsigned flagsToInclude,
                const MainArgs &argStrings, DxcOpts &opts,
                llvm::raw_ostream &errors);

/// Sets up the specified DxcDllSupport instance as per the given options.
int SetupDxcDllSupport(const DxcOpts &opts, dxc::DxcDllSupport &dxcSupport,
                       llvm::raw_ostream &errors);

void CopyArgsToWStrings(const llvm::opt::InputArgList &inArgs,
                        unsigned flagsToInclude,
                        std::vector<std::wstring> &outArgs);
}
}

#endif

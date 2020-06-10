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
#include "dxc/Support/HLSLOptimizationOptions.h"
#include "dxc/Support/SPIRVOptions.h"

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
  RewriteOption = (1 << 17),
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
std::error_code initHlslOptTable();
void cleanupHlslOptTable();

///////////////////////////////////////////////////////////////////////////////
// Helper classes to deal with options.

/// Flags for IDxcCompiler APIs.
static const unsigned CompilerFlags = HlslFlags::CoreOption;
/// Flags for dxc.exe command-line tool.
static const unsigned DxcFlags = HlslFlags::CoreOption | HlslFlags::DriverOption;
/// Flags for dxr.exe command-line tool.
static const unsigned DxrFlags = HlslFlags::RewriteOption | HlslFlags::DriverOption;
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

struct RewriterOpts {
  bool Unchanged = false;                   // OPT_rw_unchanged
  bool SkipFunctionBody = false;            // OPT_rw_skip_function_body
  bool SkipStatic = false;                  // OPT_rw_skip_static
  bool GlobalExternByDefault = false;       // OPT_rw_global_extern_by_default
  bool KeepUserMacro = false;               // OPT_rw_keep_user_macro
  bool ExtractEntryUniforms = false;        // OPT_rw_extract_entry_uniforms
  bool RemoveUnusedGlobals = false;         // OPT_rw_remove_unused_globals
  bool RemoveUnusedFunctions = false;         // OPT_rw_remove_unused_functions
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
  llvm::StringRef OutputReflectionFile; // OPT_Fre
  llvm::StringRef OutputRootSigFile; // OPT_Frs
  llvm::StringRef OutputShaderHashFile; // OPT_Fsh
  llvm::StringRef Preprocess; // OPT_P
  llvm::StringRef TargetProfile; // OPT_target_profile
  llvm::StringRef VariableName; // OPT_Vn
  llvm::StringRef PrivateSource; // OPT_setprivate
  llvm::StringRef RootSignatureSource; // OPT_setrootsignature
  llvm::StringRef VerifyRootSignatureSource; //OPT_verifyrootsignature
  llvm::StringRef RootSignatureDefine; // OPT_rootsig_define
  llvm::StringRef FloatDenormalMode; // OPT_denorm
  std::vector<std::string> Exports; // OPT_exports
  std::vector<std::string> PreciseOutputs; // OPT_precise_output
  llvm::StringRef DefaultLinkage; // OPT_default_linkage
  unsigned DefaultTextCodePage = DXC_CP_UTF8; // OPT_encoding

  bool AllResourcesBound = false; // OPT_all_resources_bound
  bool AstDump = false; // OPT_ast_dump
  bool ColorCodeAssembly = false; // OPT_Cc
  bool CodeGenHighLevel = false; // OPT_fcgl
  bool AllowPreserveValues = false; // OPT_preserve_intermediate_values
  bool DebugInfo = false; // OPT__SLASH_Zi
  bool DebugNameForBinary = false; // OPT_Zsb
  bool DebugNameForSource = false; // OPT_Zss
  bool DumpBin = false;        // OPT_dumpbin
  bool WarningAsError = false; // OPT__SLASH_WX
  bool IEEEStrict = false;     // OPT_Gis
  bool IgnoreLineDirectives = false; // OPT_ignore_line_directives
  bool DefaultColMajor = false;  // OPT_Zpc
  bool DefaultRowMajor = false;  // OPT_Zpr
  bool DisableValidation = false; // OPT_VD
  unsigned OptLevel = 0;      // OPT_O0/O1/O2/O3
  bool DisableOptimizations = false; // OPT_Od
  bool AvoidFlowControl = false;     // OPT_Gfa
  bool PreferFlowControl = false;    // OPT_Gfp
  bool EnableStrictMode = false;     // OPT_Ges
  bool EnableDX9CompatMode = false;     // OPT_Gec
  bool EnableFXCCompatMode = false;     // internal flag
  unsigned long HLSLVersion = 0; // OPT_hlsl_version (2015-2018)
  bool Enable16BitTypes = false; // OPT_enable_16bit_types
  bool OptDump = false; // OPT_ODump - dump optimizer commands
  bool OutputWarnings = true; // OPT_no_warnings
  bool ShowHelp = false;  // OPT_help
  bool ShowHelpHidden = false; // OPT__help_hidden
  bool ShowOptionNames = false; // OPT_fdiagnostics_show_option
  bool UseColor = false; // OPT_Cc
  bool UseHexLiterals = false; // OPT_Lx
  bool UseInstructionByteOffsets = false; // OPT_No
  bool UseInstructionNumbers = false; // OPT_Ni
  bool NotUseLegacyCBufLoad = false;  // OPT_no_legacy_cbuf_layout
  bool PackPrefixStable = false;  // OPT_pack_prefix_stable
  bool PackOptimized = false;  // OPT_pack_optimized
  bool DisplayIncludeProcess = false; // OPT__vi
  bool RecompileFromBinary = false; // OPT _Recompile (Recompiling the DXBC binary file not .hlsl file)
  bool StripDebug = false; // OPT Qstrip_debug
  bool EmbedDebug = false; // OPT Qembed_debug
  bool StripRootSignature = false; // OPT_Qstrip_rootsignature
  bool StripPrivate = false; // OPT_Qstrip_priv
  bool StripReflection = false; // OPT_Qstrip_reflect
  bool KeepReflectionInDxil = false; // OPT_Qkeep_reflect_in_dxil
  bool StripReflectionFromDxil = false; // OPT_Qstrip_reflect_from_dxil
  bool ExtractRootSignature = false; // OPT_extractrootsignature
  bool DisassembleColorCoded = false; // OPT_Cc
  bool DisassembleInstNumbers = false; //OPT_Ni
  bool DisassembleByteOffset = false; //OPT_No
  bool DisaseembleHex = false; //OPT_Lx
  bool LegacyMacroExpansion = false; // OPT_flegacy_macro_expansion
  bool LegacyResourceReservation = false; // OPT_flegacy_resource_reservation
  unsigned long AutoBindingSpace = UINT_MAX; // OPT_auto_binding_space
  bool ExportShadersOnly = false; // OPT_export_shaders_only
  bool ResMayAlias = false; // OPT_res_may_alias
  unsigned long ValVerMajor = UINT_MAX, ValVerMinor = UINT_MAX; // OPT_validator_version
  unsigned ScanLimit = 0; // OPT_memdep_block_scan_limit
  hlsl::OptimizationOptions DxcOptimizationOptions; // OPT_opt_disable

  // Rewriter Options
  RewriterOpts RWOpt;

  std::vector<std::string> Warnings;

  bool IsRootSignatureProfile();
  bool IsLibraryProfile();

  // Helpers to clarify interpretation of flags for behavior in implementation
  bool IsDebugInfoEnabled();    // Zi
  bool EmbedDebugInfo();        // Qembed_debug
  bool EmbedPDBName();          // Zi or Fd
  bool DebugFileIsDirectory();  // Fd ends in '\\'
  llvm::StringRef GetPDBName(); // Fd name

  // SPIRV Change Starts
#ifdef ENABLE_SPIRV_CODEGEN
  bool GenSPIRV;                    // OPT_spirv
  clang::spirv::SpirvCodeGenOptions SpirvOptions; // All SPIR-V CodeGen-related options
#endif
  // SPIRV Change Ends
};

/// Use this class to capture, convert and handle the lifetime for the
/// command-line arguments to a program.
class MainArgs {
public:
  llvm::SmallVector<std::string, 8> Utf8StringVector;
  llvm::SmallVector<const char *, 8> Utf8CharPtrVector;

  MainArgs() = default;
  MainArgs(int argc, const wchar_t **argv, int skipArgCount = 1);
  MainArgs(int argc, const char **argv, int skipArgCount = 1);
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

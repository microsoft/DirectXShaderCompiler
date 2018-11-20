//===--- HLSLOptions.cpp - Driver Options Table ---------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLSLOptions.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/STLExtras.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/raw_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/DXIL/DxilShaderModel.h"

using namespace llvm::opt;
using namespace dxc;
using namespace hlsl;
using namespace hlsl::options;

#define PREFIX(NAME, VALUE) static const char *const NAME[] = VALUE;
#include "dxc/Support/HLSLOptions.inc"
#undef PREFIX

static const OptTable::Info HlslInfoTable[] = {
#define OPTION(PREFIX, NAME, ID, KIND, GROUP, ALIAS, ALIASARGS, FLAGS, PARAM, \
               HELPTEXT, METAVAR)   \
  { PREFIX, NAME, HELPTEXT, METAVAR, OPT_##ID, Option::KIND##Class, PARAM, \
    FLAGS, OPT_##GROUP, OPT_##ALIAS, ALIASARGS },
#include "dxc/Support/HLSLOptions.inc"
#undef OPTION
};

namespace {

  class HlslOptTable : public OptTable {
  public:
    HlslOptTable()
      : OptTable(HlslInfoTable, llvm::array_lengthof(HlslInfoTable)) {}
  };

}

static HlslOptTable *g_HlslOptTable;

#ifndef _WIN32
#pragma GCC visibility push(hidden)
#endif

std::error_code hlsl::options::initHlslOptTable() {
  DXASSERT(g_HlslOptTable == nullptr, "else double-init");
  g_HlslOptTable = new (std::nothrow) HlslOptTable();
  if (g_HlslOptTable == nullptr)
    return std::error_code(E_OUTOFMEMORY, std::system_category());
  return std::error_code();
}

void hlsl::options::cleanupHlslOptTable() {
  delete g_HlslOptTable;
  g_HlslOptTable = nullptr;
}

const OptTable * hlsl::options::getHlslOptTable() {
  return g_HlslOptTable;
}

#ifndef _WIN32
#pragma GCC visibility pop
#endif

void DxcDefines::push_back(llvm::StringRef value) {
  // Skip empty defines.
  if (value.size() > 0) {
    DefineStrings.push_back(value);
  }
}

UINT32 DxcDefines::ComputeNumberOfWCharsNeededForDefines() {
  UINT32 wcharSize = 0;
  for (llvm::StringRef &S : DefineStrings) {
    DXASSERT(S.size() > 0,
             "else DxcDefines::push_back should not have added this");
    const int utf16Length = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, S.data(), S.size(), nullptr, 0);
    IFTARG(utf16Length != 0);
    wcharSize += utf16Length + 1; // adding null terminated character
  }
  return wcharSize;
}

void DxcDefines::BuildDefines() {
  // Calculate and prepare the size of the backing buffer.
  DXASSERT(DefineValues == nullptr, "else DxcDefines is already built");
  UINT32 wcharSize = ComputeNumberOfWCharsNeededForDefines();

  DefineValues = new wchar_t[wcharSize];
  DefineVector.resize(DefineStrings.size());

  // Build up the define structures while filling in the backing buffer.
  UINT32 remaining = wcharSize;
  LPWSTR pWriteCursor = DefineValues;
  for (size_t i = 0; i < DefineStrings.size(); ++i) {
    llvm::StringRef &S = DefineStrings[i];
    DxcDefine &D = DefineVector[i];
    const int utf16Length =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, S.data(), S.size(),
                              pWriteCursor, remaining);
    DXASSERT(utf16Length > 0,
             "else it should have failed during size calculation");
    LPWSTR pDefineEnd = pWriteCursor + utf16Length;
    D.Name = pWriteCursor;

    LPWSTR pEquals = std::find(pWriteCursor, pDefineEnd, L'=');
    if (pEquals == pDefineEnd) {
      D.Value = nullptr;
    } else {
      *pEquals = L'\0';
      D.Value = pEquals + 1;
    }

    // Advance past converted characters and include the null terminator.
    pWriteCursor += utf16Length;
    *pWriteCursor = L'\0';
    ++pWriteCursor;

    DXASSERT(pWriteCursor <= DefineValues + wcharSize,
             "else this function is calculating this incorrectly");
    remaining -= (utf16Length + 1);
  }
}

bool DxcOpts::IsRootSignatureProfile() {
  return TargetProfile == "rootsig_1_0" ||
      TargetProfile == "rootsig_1_1";
}

bool DxcOpts::IsLibraryProfile() {
  return TargetProfile.startswith("lib_");
}

MainArgs::MainArgs(int argc, const wchar_t **argv, int skipArgCount) {
  if (argc > skipArgCount) {
    Utf8StringVector.reserve(argc - skipArgCount);
    Utf8CharPtrVector.reserve(argc - skipArgCount);
    for (int i = skipArgCount; i < argc; ++i) {
      Utf8StringVector.emplace_back(Unicode::UTF16ToUTF8StringOrThrow(argv[i]));
      Utf8CharPtrVector.push_back(Utf8StringVector.back().data());
    }
  }
}

MainArgs::MainArgs(int argc, const char **argv, int skipArgCount) {
  if (argc > skipArgCount) {
    Utf8StringVector.reserve(argc - skipArgCount);
    Utf8CharPtrVector.reserve(argc - skipArgCount);
    for (int i = skipArgCount; i < argc; ++i) {
      Utf8StringVector.emplace_back(argv[i]);
      Utf8CharPtrVector.push_back(Utf8StringVector.back().data());
    }
  }
}

MainArgs::MainArgs(llvm::ArrayRef<llvm::StringRef> args) {
  Utf8StringVector.reserve(args.size());
  Utf8CharPtrVector.reserve(args.size());
  for (llvm::StringRef str : args) {
    Utf8StringVector.emplace_back(str.str());
    Utf8CharPtrVector.push_back(Utf8StringVector.back().data());
  }
}

MainArgs& MainArgs::operator=(const MainArgs &other) {
  Utf8StringVector.clear();
  Utf8CharPtrVector.clear();
  Utf8StringVector.reserve(other.Utf8StringVector.size());
  Utf8CharPtrVector.reserve(other.Utf8StringVector.size());
  for (const std::string &str : other.Utf8StringVector) {
    Utf8StringVector.emplace_back(str);
    Utf8CharPtrVector.push_back(Utf8StringVector.back().data());
  }
  return *this;
}

StringRefUtf16::StringRefUtf16(llvm::StringRef value) {
  if (!value.empty())
    m_value = Unicode::UTF8ToUTF16StringOrThrow(value.data());
}

static bool GetTargetVersionFromString(llvm::StringRef ref, unsigned *major, unsigned *minor) {
  *major = *minor = -1;
  unsigned len = ref.size();
  if (len < 6 || len > 11) // length: ps_6_0 to rootsig_1_0
    return false;
  if (ref[len - 4] != '_' || ref[len - 2] != '_')
    return false;

  char cMajor = ref[len - 3];
  char cMinor = ref[len - 1];

  if (cMajor >= '0' && cMajor <= '9')
    *major = cMajor - '0';
  else
    return false;

  if (cMinor == 'x')
    *minor = 0xF;
  else if (cMinor >= '0' && cMinor <= '9')
    *minor = cMinor - '0';
  else
    return false;

  return true;
}

// SPIRV Change Starts
#ifdef ENABLE_SPIRV_CODEGEN
/// Checks and collects the arguments for -fvk-{b|s|t|u}-shift into *shifts.
static bool handleVkShiftArgs(const InputArgList &args, OptSpecifier id,
                              const char *name,
                              llvm::SmallVectorImpl<int32_t> *shifts,
                              llvm::raw_ostream &errors) {
  const auto values = args.getAllArgValues(id);

  if (values.empty())
    return true;

  if (!args.hasArg(OPT_spirv)) {
    errors << "-fvk-" << name << "-shift requires -spirv";
    return false;
  }

  if (!args.getLastArgValue(OPT_fvk_bind_register).empty()) {
    errors << "-fvk-" << name
           << "-shift cannot be used together with -fvk-bind-register";
    return false;
  }

  shifts->clear();
  bool setForAll = false;

  for (const auto &val : values) {
    int32_t number = 0;
    if (val == "all") {
      number = -1;
      setForAll = true;
    } else {
      if (llvm::StringRef(val).getAsInteger(10, number)) {
        errors << "invalid -fvk-" << name << "-shift argument: " << val;
        return false;
      }
      if (number < 0) {
        errors << "negative -fvk-" << name << "-shift argument: " << val;
        return false;
      }
    }
    shifts->push_back(number);
  }
  if (setForAll && shifts->size() > 2) {
    errors << "setting all sets via -fvk-" << name
           << "-shift argument should be used alone";
    return false;
  }
  return true;
};
#endif
// SPIRV Change Ends

namespace hlsl {
namespace options {

/// Reads all options from the given argument strings, populates opts, and
/// validates reporting errors and warnings.
int ReadDxcOpts(const OptTable *optionTable, unsigned flagsToInclude,
  const MainArgs &argStrings, DxcOpts &opts,
  llvm::raw_ostream &errors) {
  DXASSERT_NOMSG(optionTable != nullptr);

  unsigned missingArgIndex = 0, missingArgCount = 0;
  InputArgList Args = optionTable->ParseArgs(
    argStrings.getArrayRef(), missingArgIndex, missingArgCount, flagsToInclude);
  // Verify consistency for external library support.
  opts.ExternalLib = Args.getLastArgValue(OPT_external_lib);
  opts.ExternalFn = Args.getLastArgValue(OPT_external_fn);
  if (opts.ExternalLib.empty()) {
    if (!opts.ExternalFn.empty()) {
      errors << "External function cannot be specified without an external "
        "library name.";
      return 1;
    }
  }
  else {
    if (opts.ExternalFn.empty()) {
      errors << "External library name requires specifying an external "
        "function name.";
      return 1;
    }
  }

  opts.ShowHelp = Args.hasFlag(OPT_help, OPT_INVALID, false);
  opts.ShowHelp |= (opts.ShowHelpHidden = Args.hasFlag(OPT__help_hidden, OPT_INVALID, false));
  if (opts.ShowHelp) {
    return 0;
  }

  if (missingArgCount) {
    errors << "Argument to '" << Args.getArgString(missingArgIndex)
      << "' is missing.";
    return 1;
  }

  if (!Args.hasArg(hlsl::options::OPT_Qunused_arguments)) {
    for (const Arg *A : Args.filtered(OPT_UNKNOWN)) {
      errors << "Unknown argument: '" << A->getAsString(Args).c_str() << "'";
      return 1;
    }
  }

  // Add macros from the command line.

  for (const Arg *A : Args.filtered(OPT_D)) {
    opts.Defines.push_back(A->getValue());
    // If supporting OPT_U and included in filter, handle undefs.
  }
  opts.Defines.BuildDefines(); // Must be called after all defines are pushed back

  DXASSERT(opts.ExternalLib.empty() == opts.ExternalFn.empty(),
           "else flow above is incorrect");

  // when no-warnings option is present, do not output warnings.
  opts.OutputWarnings = Args.hasFlag(OPT_INVALID, OPT_no_warnings, true);
  opts.EntryPoint = Args.getLastArgValue(OPT_entrypoint);
  // Entry point is required in arguments only for drivers; APIs take this through an argument.
  // The value should default to 'main', but we let the caller apply this policy.

  if (opts.TargetProfile.empty()) {
    opts.TargetProfile = Args.getLastArgValue(OPT_target_profile);
  }

  if (opts.IsLibraryProfile()) {
    if (Args.getLastArg(OPT_entrypoint)) {
      errors << "cannot specify entry point for a library";
      return 1;
    } else {
      // Set entry point to impossible name.
      opts.EntryPoint = "lib.no::entry";
    }
  } else {
    if (Args.getLastArg(OPT_exports)) {
      errors << "library profile required when using -exports option";
      return 1;
    } else if (Args.hasFlag(OPT_export_shaders_only, OPT_INVALID, false)) {
      errors << "library profile required when using -export-shaders-only option";
      return 1;
    } else if (Args.getLastArg(OPT_default_linkage)) {
      errors << "library profile required when using -default-linkage option";
      return 1;
    }
  }

  opts.EnableDX9CompatMode = Args.hasFlag(OPT_Gec, OPT_INVALID, false);
  llvm::StringRef ver = Args.getLastArgValue(OPT_hlsl_version);
  if (ver.empty()) {
    if (opts.EnableDX9CompatMode)
      opts.HLSLVersion = 2016; // Default to max supported version with /Gec flag
    else
      opts.HLSLVersion = 2018; // Default to latest version
  } else {
    try {
      opts.HLSLVersion = std::stoul(std::string(ver));
      if (opts.HLSLVersion < 2015 || opts.HLSLVersion > 2018) {
        errors << "Unknown HLSL version: " << opts.HLSLVersion;
        return 1;
      }
    }
    catch (const std::invalid_argument &) {
      errors << "Invalid HLSL Version";
      return 1;
    }
    catch (const std::out_of_range &) {
      errors << "Invalid HLSL Version";
      return 1;
    }
  }

  if (opts.HLSLVersion == 2015 && !(flagsToInclude & HlslFlags::ISenseOption)) {
    errors << "HLSL Version 2015 is only supported for language services";
    return 1;
  }

  if (opts.EnableDX9CompatMode && opts.HLSLVersion > 2016) {
    errors << "/Gec is not supported with HLSLVersion " << opts.HLSLVersion;
    return 1;
  }

  if (opts.HLSLVersion <= 2016) {
    opts.EnableFXCCompatMode = true;
  }

  // AssemblyCodeHex not supported (Fx)
  // OutputLibrary not supported (Fl)
  opts.AssemblyCode = Args.getLastArgValue(OPT_Fc);
  opts.DebugFile = Args.getLastArgValue(OPT_Fd);
  opts.ExtractPrivateFile = Args.getLastArgValue(OPT_getprivate);
  opts.Enable16BitTypes = Args.hasFlag(OPT_enable_16bit_types, OPT_INVALID, false);
  opts.OutputObject = Args.getLastArgValue(OPT_Fo);
  opts.OutputHeader = Args.getLastArgValue(OPT_Fh);
  opts.OutputWarningsFile = Args.getLastArgValue(OPT_Fe);
  opts.UseColor = Args.hasFlag(OPT_Cc, OPT_INVALID);
  opts.UseInstructionNumbers = Args.hasFlag(OPT_Ni, OPT_INVALID);
  opts.UseInstructionByteOffsets = Args.hasFlag(OPT_No, OPT_INVALID);
  opts.UseHexLiterals = Args.hasFlag(OPT_Lx, OPT_INVALID);
  opts.Preprocess = Args.getLastArgValue(OPT_P);
  opts.AstDump = Args.hasFlag(OPT_ast_dump, OPT_INVALID, false);
  opts.CodeGenHighLevel = Args.hasFlag(OPT_fcgl, OPT_INVALID, false);
  opts.DebugInfo = Args.hasFlag(OPT__SLASH_Zi, OPT_INVALID, false);
  opts.DebugNameForBinary = Args.hasFlag(OPT_Zsb, OPT_INVALID, false);
  opts.DebugNameForSource = Args.hasFlag(OPT_Zss, OPT_INVALID, false);
  opts.VariableName = Args.getLastArgValue(OPT_Vn);
  opts.InputFile = Args.getLastArgValue(OPT_INPUT);
  opts.ForceRootSigVer = Args.getLastArgValue(OPT_force_rootsig_ver);
  opts.PrivateSource = Args.getLastArgValue(OPT_setprivate);
  opts.RootSignatureSource = Args.getLastArgValue(OPT_setrootsignature);
  opts.VerifyRootSignatureSource = Args.getLastArgValue(OPT_verifyrootsignature);
  opts.RootSignatureDefine = Args.getLastArgValue(OPT_rootsig_define);

  if (!opts.ForceRootSigVer.empty() && opts.ForceRootSigVer != "rootsig_1_0" &&
      opts.ForceRootSigVer != "rootsig_1_1") {
    errors << "Unsupported value '" << opts.ForceRootSigVer
           << "' for root signature profile.";
    return 1;
  }

  opts.IEEEStrict = Args.hasFlag(OPT_Gis, OPT_INVALID, false);

  opts.IgnoreLineDirectives = Args.hasFlag(OPT_ignore_line_directives, OPT_INVALID, false);

  opts.FloatDenormalMode = Args.getLastArgValue(OPT_denorm);
  // Check if a given denormalized value is valid
  if (!opts.FloatDenormalMode.empty()) {
    if (!(opts.FloatDenormalMode.equals_lower("preserve") ||
          opts.FloatDenormalMode.equals_lower("ftz") ||
          opts.FloatDenormalMode.equals_lower("any"))) {
      errors << "Unsupported value '" << opts.FloatDenormalMode
          << "' for denorm option.";
      return 1;
    }
  }

  llvm::StringRef auto_binding_space = Args.getLastArgValue(OPT_auto_binding_space);
  if (!auto_binding_space.empty()) {
    if (auto_binding_space.getAsInteger(10, opts.AutoBindingSpace)) {
      errors << "Unsupported value '" << auto_binding_space << "' for auto binding space.";
      return 1;
    }
  }

  opts.Exports = Args.getAllArgValues(OPT_exports);

  opts.DefaultLinkage = Args.getLastArgValue(OPT_default_linkage);
  if (!opts.DefaultLinkage.empty()) {
    if (!(opts.DefaultLinkage.equals_lower("internal") ||
          opts.DefaultLinkage.equals_lower("external"))) {
      errors << "Unsupported value '" << opts.DefaultLinkage
             << "for -default-linkage option.";
      return 1;
    }
  }

  // Check options only allowed in shader model >= 6.2FPDenormalMode
  unsigned Major = 0;
  unsigned Minor = 0;
  if (!opts.TargetProfile.empty()) {
    if (!GetTargetVersionFromString(opts.TargetProfile, &Major, &Minor)) {
      errors << "unable to parse shader model.";
      return 1;
    }
  }

  if (opts.TargetProfile.empty() || Major < 6 || (Major == 6 && Minor < 2)) {
    if (!opts.FloatDenormalMode.empty()) {
      errors << "denorm option is only allowed for shader model 6.2 and above.";
      return 1;
    }
  }

  // /enable-16bit-types only allowed for HLSL 2018 and shader model 6.2
  if (opts.Enable16BitTypes) {
    if (opts.TargetProfile.empty() || opts.HLSLVersion < 2018
      || Major < 6 || (Major == 6 && Minor < 2)) {
      errors << "enable-16bit-types is only allowed for shader model >= 6.2 and HLSL Language >= 2018.";
      return 1;
    }
  }

  opts.DisableOptimizations = false;
  if (Arg *A = Args.getLastArg(OPT_O0, OPT_O1, OPT_O2, OPT_O3, OPT_Od)) {
    if (A->getOption().matches(OPT_O0))
      opts.OptLevel = 0;
    if (A->getOption().matches(OPT_O1))
      opts.OptLevel = 1;
    if (A->getOption().matches(OPT_O2))
      opts.OptLevel = 2;
    if (A->getOption().matches(OPT_O3))
      opts.OptLevel = 3;
    if (A->getOption().matches(OPT_Od)) {
      opts.DisableOptimizations = true;
      opts.OptLevel = 0;
    }
  }
  else
    opts.OptLevel = 3;
  opts.OptDump = Args.hasFlag(OPT_Odump, OPT_INVALID, false);

  opts.DisableValidation = Args.hasFlag(OPT_VD, OPT_INVALID, false);

  opts.AllResourcesBound = Args.hasFlag(OPT_all_resources_bound, OPT_INVALID, false);
  opts.ColorCodeAssembly = Args.hasFlag(OPT_Cc, OPT_INVALID, false);
  opts.DefaultRowMajor = Args.hasFlag(OPT_Zpr, OPT_INVALID, false);
  opts.DefaultColMajor = Args.hasFlag(OPT_Zpc, OPT_INVALID, false);
  opts.DumpBin = Args.hasFlag(OPT_dumpbin, OPT_INVALID, false);
  opts.NotUseLegacyCBufLoad = Args.hasFlag(OPT_not_use_legacy_cbuf_load, OPT_INVALID, false);
  opts.PackPrefixStable = Args.hasFlag(OPT_pack_prefix_stable, OPT_INVALID, false);
  opts.PackOptimized = Args.hasFlag(OPT_pack_optimized, OPT_INVALID, false);
  opts.DisplayIncludeProcess = Args.hasFlag(OPT_H, OPT_INVALID, false);
  opts.WarningAsError = Args.hasFlag(OPT__SLASH_WX, OPT_INVALID, false);
  opts.AvoidFlowControl = Args.hasFlag(OPT_Gfa, OPT_INVALID, false);
  opts.PreferFlowControl = Args.hasFlag(OPT_Gfp, OPT_INVALID, false);
  opts.RecompileFromBinary = Args.hasFlag(OPT_recompile, OPT_INVALID, false);
  opts.StripDebug = Args.hasFlag(OPT_Qstrip_debug, OPT_INVALID, false);
  opts.StripRootSignature = Args.hasFlag(OPT_Qstrip_rootsignature, OPT_INVALID, false);
  opts.StripPrivate = Args.hasFlag(OPT_Qstrip_priv, OPT_INVALID, false);
  opts.StripReflection = Args.hasFlag(OPT_Qstrip_reflect, OPT_INVALID, false);
  opts.ExtractRootSignature = Args.hasFlag(OPT_extractrootsignature, OPT_INVALID, false);
  opts.DisassembleColorCoded = Args.hasFlag(OPT_Cc, OPT_INVALID, false);
  opts.DisassembleInstNumbers = Args.hasFlag(OPT_Ni, OPT_INVALID, false);
  opts.DisassembleByteOffset = Args.hasFlag(OPT_No, OPT_INVALID, false);
  opts.DisaseembleHex = Args.hasFlag(OPT_Lx, OPT_INVALID, false);
  opts.LegacyMacroExpansion = Args.hasFlag(OPT_flegacy_macro_expansion, OPT_INVALID, false);
  opts.LegacyResourceReservation = Args.hasFlag(OPT_flegacy_resource_reservation, OPT_INVALID, false);
  opts.ExportShadersOnly = Args.hasFlag(OPT_export_shaders_only, OPT_INVALID, false);

  if (opts.DefaultColMajor && opts.DefaultRowMajor) {
    errors << "Cannot specify /Zpr and /Zpc together, use /? to get usage information";
    return 1;
  }
  if (opts.AvoidFlowControl && opts.PreferFlowControl) {
    errors << "Cannot specify /Gfa and /Gfp together, use /? to get usage information";
    return 1;
  }
  if (opts.PackPrefixStable && opts.PackOptimized) {
    errors << "Cannot specify /pack_prefix_stable and /pack_optimized together, use /? to get usage information";
    return 1;
  }
  // TODO: more fxc option check.
  // ERR_RES_MAY_ALIAS_ONLY_IN_CS_5
  // ERR_NOT_ABLE_TO_FLATTEN on if that contain side effects
  // TODO: other front-end error.
  // ERR_RESOURCE_NOT_IN_TEMPLATE
  // ERR_COMPLEX_TEMPLATE_RESOURCE
  // ERR_RESOURCE_BIND_CONFLICT
  // ERR_TEMPLATE_VAR_CONFLICT
  // ERR_ATTRIBUTE_PARAM_SIDE_EFFECT

  if ((flagsToInclude & hlsl::options::DriverOption) && opts.InputFile.empty()) {
    // Input file is required in arguments only for drivers; APIs take this through an argument.
    errors << "Required input file argument is missing. use -help to get more information.";
    return 1;
  }
  if (opts.OutputHeader.empty() && !opts.VariableName.empty()) {
    errors << "Cannot specify a header variable name when not writing a header.";
    return 1;
  }

  if (!opts.Preprocess.empty() &&
      (!opts.OutputHeader.empty() || !opts.OutputObject.empty() ||
       !opts.OutputWarnings || !opts.OutputWarningsFile.empty())) {
    errors << "Preprocess cannot be specified with other options.";
    return 1;
  }

  if (opts.DumpBin) {
    if (opts.DisplayIncludeProcess || opts.AstDump) {
      errors << "Cannot perform actions related to sources from a binary file.";
      return 1;
    }
    if (opts.AllResourcesBound || opts.AvoidFlowControl ||
        opts.CodeGenHighLevel || opts.DebugInfo || opts.DefaultColMajor ||
        opts.DefaultRowMajor || opts.Defines.size() != 0 ||
        opts.DisableOptimizations ||
        !opts.EntryPoint.empty() || !opts.ForceRootSigVer.empty() ||
        opts.PreferFlowControl || !opts.TargetProfile.empty()) {
      errors << "Cannot specify compilation options when reading a binary file.";
      return 1;
    }
  }

  if ((flagsToInclude & hlsl::options::DriverOption) &&
      opts.TargetProfile.empty() && !opts.DumpBin && opts.Preprocess.empty() && !opts.RecompileFromBinary) {
    // Target profile is required in arguments only for drivers when compiling;
    // APIs take this through an argument.
    errors << "Target profile argument is missing";
    return 1;
  }

  if (!opts.DebugNameForBinary && !opts.DebugNameForSource) {
    opts.DebugNameForSource = true;
  }
  else if (opts.DebugNameForBinary && opts.DebugNameForSource) {
    errors << "Cannot specify both /Zss and /Zsb";
    return 1;
  }

  if (opts.IsLibraryProfile() && Minor == 0xF) {
    // Disable validation for offline link only target
    opts.DisableValidation = true;
  }

  // Disable lib_6_1 and lib_6_2 if /Vd is not present
  if (opts.IsLibraryProfile() && (Major < 6 || (Major == 6 && Minor < 3))) {
    if (!opts.DisableValidation) {
      errors << "Must disable validation for unsupported lib_6_1 or lib_6_2 "
                "targets.";
      return 1;
    }
  }

    // SPIRV Change Starts
#ifdef ENABLE_SPIRV_CODEGEN
  opts.GenSPIRV = Args.hasFlag(OPT_spirv, OPT_INVALID, false);
  opts.SpirvOptions.invertY = Args.hasFlag(OPT_fvk_invert_y, OPT_INVALID, false);
  opts.SpirvOptions.invertW = Args.hasFlag(OPT_fvk_use_dx_position_w, OPT_INVALID, false);
  opts.SpirvOptions.useGlLayout = Args.hasFlag(OPT_fvk_use_gl_layout, OPT_INVALID, false);
  opts.SpirvOptions.useDxLayout = Args.hasFlag(OPT_fvk_use_dx_layout, OPT_INVALID, false);
  opts.SpirvOptions.useScalarLayout = Args.hasFlag(OPT_fvk_use_scalar_layout, OPT_INVALID, false);
  opts.SpirvOptions.enableReflect = Args.hasFlag(OPT_fspv_reflect, OPT_INVALID, false);
  opts.SpirvOptions.noWarnIgnoredFeatures = Args.hasFlag(OPT_Wno_vk_ignored_features, OPT_INVALID, false);
  opts.SpirvOptions.noWarnEmulatedFeatures = Args.hasFlag(OPT_Wno_vk_emulated_features, OPT_INVALID, false);

  if (!handleVkShiftArgs(Args, OPT_fvk_b_shift, "b", &opts.SpirvOptions.bShift, errors) ||
      !handleVkShiftArgs(Args, OPT_fvk_t_shift, "t", &opts.SpirvOptions.tShift, errors) ||
      !handleVkShiftArgs(Args, OPT_fvk_s_shift, "s", &opts.SpirvOptions.sShift, errors) ||
      !handleVkShiftArgs(Args, OPT_fvk_u_shift, "u", &opts.SpirvOptions.uShift, errors))
    return 1;

  opts.SpirvOptions.bindRegister = Args.getAllArgValues(OPT_fvk_bind_register);
  opts.SpirvOptions.stageIoOrder = Args.getLastArgValue(OPT_fvk_stage_io_order_EQ, "decl");
  if (opts.SpirvOptions.stageIoOrder != "alpha" && opts.SpirvOptions.stageIoOrder != "decl") {
    errors << "unknown Vulkan stage I/O location assignment order: "
           << opts.SpirvOptions.stageIoOrder;
    return 1;
  }

  for (const Arg *A : Args.filtered(OPT_fspv_extension_EQ)) {
    opts.SpirvOptions.allowedExtensions.push_back(A->getValue());
  }

  opts.SpirvOptions.debugInfoFile = opts.SpirvOptions.debugInfoSource = false;
  opts.SpirvOptions.debugInfoLine = opts.SpirvOptions.debugInfoTool = false;
  if (Args.hasArg(OPT_fspv_debug_EQ)) {
    opts.DebugInfo = true;
    for (const Arg *A : Args.filtered(OPT_fspv_debug_EQ)) {
      const llvm::StringRef v = A->getValue();
      if (v == "file") {
        opts.SpirvOptions.debugInfoFile = true;
      } else if (v == "source") {
        opts.SpirvOptions.debugInfoFile = true;
        opts.SpirvOptions.debugInfoSource = true;
      } else if (v == "line") {
        opts.SpirvOptions.debugInfoFile = true;
        opts.SpirvOptions.debugInfoSource = true;
        opts.SpirvOptions.debugInfoLine = true;
      } else if (v == "tool") {
        opts.SpirvOptions.debugInfoTool = true;
      } else {
        errors << "unknown SPIR-V debug info control parameter: " << v;
        return 1;
      }
    }
  } else if (opts.DebugInfo) {
    // By default turn on all categories
    opts.SpirvOptions.debugInfoFile = opts.SpirvOptions.debugInfoSource = true;
    opts.SpirvOptions.debugInfoLine = opts.SpirvOptions.debugInfoTool = true;
  }

  opts.SpirvOptions.targetEnv = Args.getLastArgValue(OPT_fspv_target_env_EQ, "vulkan1.0");

  // Handle -Oconfig=<comma-separated-list> option.
  uint32_t numOconfigs = 0;
  for (const Arg *A : Args.filtered(OPT_Oconfig)) {
    ++numOconfigs;
    if (numOconfigs > 1) {
      errors << "-Oconfig should not be specified more than once";
      return 1;
    }
    if (Args.getLastArg(OPT_O0, OPT_O1, OPT_O2, OPT_O3)) {
      errors << "-Oconfig should not be used together with -O";
      return 1;
    }
    for (const auto v : A->getValues()) {
      opts.SpirvOptions.optConfig.push_back(v);
    }
  }

#else
  if (Args.hasFlag(OPT_spirv, OPT_INVALID, false) ||
      Args.hasFlag(OPT_fvk_invert_y, OPT_INVALID, false) ||
      Args.hasFlag(OPT_fvk_use_dx_position_w, OPT_INVALID, false) ||
      Args.hasFlag(OPT_fvk_use_gl_layout, OPT_INVALID, false) ||
      Args.hasFlag(OPT_fvk_use_dx_layout, OPT_INVALID, false) ||
      Args.hasFlag(OPT_fvk_use_scalar_layout, OPT_INVALID, false) ||
      Args.hasFlag(OPT_fspv_reflect, OPT_INVALID, false) ||
      Args.hasFlag(OPT_Wno_vk_ignored_features, OPT_INVALID, false) ||
      Args.hasFlag(OPT_Wno_vk_emulated_features, OPT_INVALID, false) ||
      !Args.getLastArgValue(OPT_fvk_stage_io_order_EQ).empty() ||
      !Args.getLastArgValue(OPT_fspv_debug_EQ).empty() ||
      !Args.getLastArgValue(OPT_fspv_extension_EQ).empty() ||
      !Args.getLastArgValue(OPT_fspv_target_env_EQ).empty() ||
      !Args.getLastArgValue(OPT_Oconfig).empty() ||
      !Args.getLastArgValue(OPT_fvk_bind_register).empty() ||
      !Args.getLastArgValue(OPT_fvk_b_shift).empty() ||
      !Args.getLastArgValue(OPT_fvk_t_shift).empty() ||
      !Args.getLastArgValue(OPT_fvk_s_shift).empty() ||
      !Args.getLastArgValue(OPT_fvk_u_shift).empty()) {
    errors << "SPIR-V CodeGen not available. "
              "Please recompile with -DENABLE_SPIRV_CODEGEN=ON.";
    return 1;
  }
#endif // ENABLE_SPIRV_CODEGEN
  // SPIRV Change Ends

  opts.Args = std::move(Args);
  return 0;
}

/// Sets up the specified DxcDllSupport instance as per the given options.
int SetupDxcDllSupport(const DxcOpts &opts, dxc::DxcDllSupport &dxcSupport,
                       llvm::raw_ostream &errors) {
  if (!opts.ExternalLib.empty()) {
    DXASSERT(!opts.ExternalFn.empty(), "else ReadDxcOpts should have failed");
    StringRefUtf16 externalLib(opts.ExternalLib);
    HRESULT hrLoad =
        dxcSupport.InitializeForDll(externalLib, opts.ExternalFn.data());
    if (DXC_FAILED(hrLoad)) {
      errors << "Unable to load support for external DLL " << opts.ExternalLib
             << " with function " << opts.ExternalFn << " - error 0x";
      errors.write_hex(hrLoad);
      return 1;
    }
  }
  return 0;
}

void CopyArgsToWStrings(const InputArgList &inArgs, unsigned flagsToInclude,
                        std::vector<std::wstring> &outArgs) {
  ArgStringList stringList;
  for (const Arg *A : inArgs) {
    if (A->getOption().hasFlag(flagsToInclude)) {
      A->renderAsInput(inArgs, stringList);
    }
  }
  for (const char *argText : stringList) {
    outArgs.emplace_back(Unicode::UTF8ToUTF16StringOrThrow(argText));
  }
}

} } // hlsl::options

//===--- HLSLOptions.td - Options for HLSL --------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLSLOptions.td                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file defines the options accepted by HLSL.                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
// Include the common option parsing interfaces.                             //

include "llvm/Option/OptParser.td"

//////////////////////////////////////////////////////////////////////////////
// Flags

// DriverOption - The option is a "driver" option, and should not be forwarded
// to other tools.
def DriverOption : OptionFlag;

// CoreOption - This is considered a "core" HLSL option.
def CoreOption : OptionFlag;

// ISenseOption - This option is only supported for IntelliSense.
def ISenseOption : OptionFlag;

//////////////////////////////////////////////////////////////////////////////
// Groups

// Meta-group for options which are only used for compilation,
// and not linking etc.
def CompileOnly_Group     : OptionGroup<"<CompileOnly group>">;

def Action_Group          : OptionGroup<"<action group>">;

def I_Group               : OptionGroup<"<I group>">, Group<CompileOnly_Group>;
def M_Group               : OptionGroup<"<M group>">, Group<CompileOnly_Group>;
def T_Group               : OptionGroup<"<T group>">;
def O_Group               : OptionGroup<"<O group>">, Group<CompileOnly_Group>;
def R_Group               : OptionGroup<"<R group>">, Group<CompileOnly_Group>;
def R_value_Group         : OptionGroup<"<R (with value) group>">, Group<R_Group>;
def W_Group               : OptionGroup<"<W group>">, Group<CompileOnly_Group>;
def W_value_Group         : OptionGroup<"<W (with value) group>">, Group<W_Group>;
def d_Group               : OptionGroup<"<d group>">;
def f_Group               : OptionGroup<"<f group>">, Group<CompileOnly_Group>;
def f_clang_Group         : OptionGroup<"<f (clang-only) group>">, Group<CompileOnly_Group>;
def g_Group               : OptionGroup<"<g group>">;
def g_flags_Group         : OptionGroup<"<g flags group>">;
def i_Group               : OptionGroup<"<i group>">, Group<CompileOnly_Group>;
def clang_i_Group         : OptionGroup<"<clang i group>">, Group<i_Group>;
def m_Group               : OptionGroup<"<m group>">, Group<CompileOnly_Group>;
def u_Group               : OptionGroup<"<u group>">;

// Developer Driver Options

def internal_Group : OptionGroup<"<clang internal options>">;
def internal_driver_Group : OptionGroup<"<clang driver internal options>">,
  Group<internal_Group>, HelpText<"DRIVER OPTIONS">;
def internal_debug_Group :
  OptionGroup<"<clang debug/development internal options>">,
  Group<internal_Group>, HelpText<"DEBUG/DEVELOPMENT OPTIONS">;

def hlslcomp_Group : OptionGroup<"HLSL Compilation">, HelpText<"Compilation Options">;
def hlsloptz_Group : OptionGroup<"HLSL Optimization">, HelpText<"Optimization Options">;
def hlslutil_Group : OptionGroup<"HLSL Utility">, HelpText<"Utility Options">;
def hlslcore_Group : OptionGroup<"HLSL Core">, HelpText<"Common Options">;

//////////////////////////////////////////////////////////////////////////////
// Options

// The internal option ID must be a valid C++ identifier and results in a
// hlsl::options::OPT_XX enum constant for XX.
//
// See tools/clang/include/clang/Driver/Options.td for more notes.
// These options are based on those used by clang.
// fxc-based options kept for compatiblity are below.
//
// The general approach is to include only things that are in use, in the
// same order as in Options.td.

def D : JoinedOrSeparate<["-", "/"], "D">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
    HelpText<"Define macro">;
def H : Flag<["-"], "H">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
    HelpText<"Show header includes and nesting depth">;
def I : JoinedOrSeparate<["-", "/"], "I">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
    HelpText<"Add directory to include search path">;
def O0 : Flag<["-", "/"], "O0">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
    HelpText<"Optimization Level 0">;
def O1 : Flag<["-", "/"], "O1">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
    HelpText<"Optimization Level 1">;
def O2 : Flag<["-", "/"], "O2">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
    HelpText<"Optimization Level 2">;
def O3 : Flag<["-", "/"], "O3">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
    HelpText<"Optimization Level 3 (Default)">;
def O4 : Flag<["-", "/"], "O4">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
    HelpText<"Optimization Level 4">;
def Odump : Flag<["-", "/"], "Odump">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
    HelpText<"Print the optimizer commands.">;
def Qunused_arguments : Flag<["-"], "Qunused-arguments">, Group<hlslcore_Group>, Flags<[CoreOption]>,
  HelpText<"Don't emit warning for unused driver arguments">;
def Wall : Flag<["-"], "Wall">, Group<hlslcomp_Group>, Flags<[CoreOption]>;
def Wdeprecated : Flag<["-"], "Wdeprecated">, Group<hlslcomp_Group>, Flags<[CoreOption]>;
//def W_Joined : Joined<["-"], "W">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
//  MetaVarName<"<warning>">, HelpText<"Enable the specified warning">;
def d_Flag : Flag<["-"], "d">, Group<d_Group>;
def d_Joined : Joined<["-"], "d">, Group<d_Group>;
//def fcolor_diagnostics : Flag<["-"], "fcolor-diagnostics">, Group<hlslcomp_Group>,
//  Flags<[CoreOption, DriverOption]>, HelpText<"Use colors in diagnostics">;
def fdiagnostics_color : Flag<["-"], "fdiagnostics-color">, Group<hlslcomp_Group>,
  Flags<[CoreOption, DriverOption]>;
def fdiagnostics_color_EQ : Joined<["-"], "fdiagnostics-color=">, Group<hlslcomp_Group>;
//def fansi_escape_codes : Flag<["-"], "fansi-escape-codes">, Group<f_Group>, // HLSL Change: disable ANSI escape codes
//  Flags<[CoreOption, CC1Option]>, HelpText<"Use ANSI escape codes for diagnostics">;
def fconstexpr_depth_EQ : Joined<["-"], "fconstexpr-depth=">, Group<f_Group>;
def fconstexpr_steps_EQ : Joined<["-"], "fconstexpr-steps=">, Group<f_Group>;
def fconstexpr_backtrace_limit_EQ : Joined<["-"], "fconstexpr-backtrace-limit=">,
                                    Group<f_Group>;
//def fdiagnostics_show_option : Flag<["-"], "fdiagnostics-show-option">, Group<hlslcomp_Group>,
//    Flags<[CoreOption]>, HelpText<"Print option name with mappable diagnostics">;
def fdiagnostics_show_category_EQ : Joined<["-"], "fdiagnostics-show-category=">, Group<hlslcomp_Group>;
def ferror_limit_EQ : Joined<["-"], "ferror-limit=">, Group<hlslcomp_Group>, Flags<[CoreOption]>;

def funsafe_math_optimizations : Flag<["-"], "funsafe-math-optimizations">,
  Group<hlsloptz_Group>;
def fno_unsafe_math_optimizations : Flag<["-"], "fno-unsafe-math-optimizations">,
  Group<hlsloptz_Group>;
def fassociative_math : Flag<["-"], "fassociative-math">, Group<hlsloptz_Group>;
def fno_associative_math : Flag<["-"], "fno-associative-math">, Group<hlsloptz_Group>;
//def freciprocal_math :
//  Flag<["-"], "freciprocal-math">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
//  HelpText<"Allow division operations to be reassociated">;
def fno_reciprocal_math : Flag<["-"], "fno-reciprocal-math">, Group<hlsloptz_Group>;
def ffinite_math_only : Flag<["-"], "ffinite-math-only">, Group<hlsloptz_Group>, Flags<[CoreOption]>;
def fno_finite_math_only : Flag<["-"], "fno-finite-math-only">, Group<hlsloptz_Group>;
def fsigned_zeros : Flag<["-"], "fsigned-zeros">, Group<hlsloptz_Group>;
//def fno_signed_zeros :
//  Flag<["-"], "fno-signed-zeros">, Group<hlsloptz_Group>, Flags<[CoreOption]>,
//  HelpText<"Allow optimizations that ignore the sign of floating point zeros">;
def fhonor_nans : Flag<["-"], "fhonor-nans">, Group<hlsloptz_Group>;
def fno_honor_nans : Flag<["-"], "fno-honor-nans">, Group<hlsloptz_Group>;
def fhonor_infinities : Flag<["-"], "fhonor-infinities">, Group<hlsloptz_Group>;
def fno_honor_infinities : Flag<["-"], "fno-honor-infinities">, Group<hlsloptz_Group>;
//def ftrapping_math : Flag<["-"], "ftrapping-math">, Group<f_Group>;
//def fno_trapping_math : Flag<["-"], "fno-trapping-math">, Group<f_Group>;
//def ffp_contract : Joined<["-"], "ffp-contract=">, Group<hlsloptz_Group>,
//  Flags<[CoreOption]>, HelpText<"Form fused FP ops (e.g. FMAs): fast (everywhere)"
//  " | on (according to FP_CONTRACT pragma, default) | off (never fuse)">;
def flimited_precision_EQ : Joined<["-"], "flimited-precision=">, Group<hlsloptz_Group>;


/*
def fno_caret_diagnostics : Flag<["-"], "fno-caret-diagnostics">, Group<hlslcomp_Group>,
 Flags<[CoreOption]>;
def fno_color_diagnostics : Flag<["-"], "fno-color-diagnostics">, Group<hlslcomp_Group>,
  Flags<[CoreOption]>;
def fno_diagnostics_color : Flag<["-"], "fno-diagnostics-color">, Group<hlslcomp_Group>,
  Flags<[CoreOption, DriverOption]>;
def fno_show_column : Flag<["-"], "fno-show-column">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"Do not include column number on diagnostics">;
def fno_spell_checking : Flag<["-"], "fno-spell-checking">, Group<hlslcomp_Group>,
  Flags<[CoreOption]>, HelpText<"Disable spell-checking">;
def fshow_column : Flag<["-"], "fshow-column">, Group<hlslcomp_Group>, Flags<[CoreOption]>;
def fshow_source_location : Flag<["-"], "fshow-source-location">, Group<hlslcomp_Group>;
def fspell_checking : Flag<["-"], "fspell-checking">, Group<hlslcomp_Group>;
def fspell_checking_limit_EQ : Joined<["-"], "fspell-checking-limit=">, Group<hlslcomp_Group>;
def funroll_loops : Flag<["-"], "funroll-loops">, Group<hlsloptz_Group>,
  HelpText<"Turn on loop unroller">, Flags<[CoreOption]>;
def fno_unroll_loops : Flag<["-"], "fno-unroll-loops">, Group<hlsloptz_Group>,
  HelpText<"Turn off loop unroller">, Flags<[CoreOption]>;
def freroll_loops : Flag<["-"], "freroll-loops">, Group<hlsloptz_Group>,
  HelpText<"Turn on loop reroller">, Flags<[CoreOption]>;
def fno_reroll_loops : Flag<["-"], "fno-reroll-loops">, Group<hlsloptz_Group>,
  HelpText<"Turn off loop reroller">;
*/
def help : Flag<["-", "--", "/"], "help">, Flags<[DriverOption]>, Group<hlslcore_Group>,
  HelpText<"Display available options">;
/*
def imacros : JoinedOrSeparate<["-", "--"], "imacros">, Group<clang_i_Group>, Flags<[CoreOption]>,
  HelpText<"Include macros from file before parsing">, MetaVarName<"<file>">;
def include_ : JoinedOrSeparate<["-", "--"], "include">, Group<clang_i_Group>, EnumName<"include">,
    MetaVarName<"<file>">, HelpText<"Include file before parsing">, Flags<[CoreOption]>;
def mllvm : Separate<["-"], "mllvm">, Flags<[CoreOption]>,
  HelpText<"Additional arguments to forward to LLVM's option processing">;
def mms_bitfields : Flag<["-"], "mms-bitfields">, Group<m_Group>, Flags<[CoreOption]>,
  HelpText<"Set the default structure layout to be compatible with the Microsoft compiler standard">;
def o : JoinedOrSeparate<["-"], "o">, Flags<[DriverOption, RenderAsInput, CoreOption]>,
  HelpText<"Write output to <file>">, MetaVarName<"<file>">;
def time : Flag<["-"], "time">,
  HelpText<"Time individual commands">;
// def U : JoinedOrSeparate<["-"], "U">, Group<CompileOnly_Group>, Flags<[CoreOption]>;
def w : Flag<["-"], "w">, HelpText<"Suppress all warnings">, Flags<[CoreOption]>;
def working_directory : JoinedOrSeparate<["-"], "working-directory">, Flags<[CoreOption]>,
  MetaVarName<"<dir>">, HelpText<"Resolve file paths relative to the specified directory">;
def working_directory_EQ : Joined<["-"], "working-directory=">, Flags<[CoreOption]>,
  MetaVarName<"<dir>">, Alias<working_directory>;
*/

// Double dash options, which are usually an alias for one of the previous
// options.

def _all_warnings : Flag<["--"], "all-warnings">, Flags<[CoreOption]>, Alias<Wall>;
def _help_hidden : Flag<["--"], "help-hidden">, Flags<[DriverOption]>;
def _help_question : Flag<["-", "/"], "?">, Flags<[DriverOption]>, Alias<help>;

//////////////////////////////////////////////////////////////////////////////
// New HLSL-specific flags.

def ast_dump : Flag<["-", "/"], "ast-dump">, Flags<[CoreOption, DriverOption, HelpHidden]>,
  HelpText<"Dumps the parsed Abstract Syntax Tree.">; // should not be core, but handy workaround until explicit API written
def spirv : Flag<["-"], "spirv">, Flags<[CoreOption, DriverOption, HelpHidden]>, // SPIRV change: temporary solution to support
  HelpText<"Generates SPIR-V binary code">;                                      // SPIRV change: SPIR-V gen in a command line tool
def external_lib : Separate<["-", "/"], "external">, Group<hlslcore_Group>, Flags<[DriverOption, HelpHidden]>,
  HelpText<"External DLL name to load for compiler support">;
def external_fn : Separate<["-", "/"], "external-fn">, Group<hlslcore_Group>, Flags<[DriverOption, HelpHidden]>,
  HelpText<"External function name to load for compiler support">;
def fcgl : Flag<["-", "/"], "fcgl">, Group<hlslcore_Group>, Flags<[CoreOption, HelpHidden]>,
  HelpText<"Generate high-level code only">;
def not_use_legacy_cbuf_load : Flag<["-", "/"], "not_use_legacy_cbuf_load">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"Do not use legacy cbuffer load">;
def pack_prefix_stable : Flag<["-", "/"], "pack_prefix_stable">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"(default) Pack signatures preserving prefix-stable property - appended elements will not disturb placement of prior elements">;
def pack_optimized : Flag<["-", "/"], "pack_optimized">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"Optimize signature packing assuming identical signature provided for each connecting stage">;
def hlsl_version : Separate<["-", "/"], "HV">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"HLSL version (Only supports 2016 for now)">;
def no_warnings : Flag<["-", "/"], "no-warnings">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"Suppress warnings">;
def rootsig_define : Separate<["-", "/"], "rootsig-define">, Group<hlslcomp_Group>, Flags<[CoreOption]>,
  HelpText<"Read root signature from a #define">;

//////////////////////////////////////////////////////////////////////////////
// fxc-based flags that don't match those previously defined.

def target_profile : JoinedOrSeparate<["-", "/"], "T">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Target profile">;
def entrypoint :  JoinedOrSeparate<["-", "/"], "E">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Entry point name">;
// /I <include> - already defined above
def _vi : Flag<["-", "/"], "Vi">, Alias<H>, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Display details about the include process.">;
def Od : Flag<["-", "/"], "Od">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Disable optimizations">;
def _SLASH_WX : Flag<["-", "/"], "WX">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Treat warnings as errors">;
def VD : Flag<["-", "/"], "Vd">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Disable validation">;
def _SLASH_Zi : Flag<["-", "/"], "Zi">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Enable debug information">;
def recompile : Flag<["-", "/"], "recompile">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"recompile from DXIL container with Debug Info or Debug Info bitcode file">;
def Zpr : Flag<["-", "/"], "Zpr">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Pack matrices in row-major order">;
def Zpc : Flag<["-", "/"], "Zpc">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Pack matrices in column-major order">;

// deprecated /Gpp def Gpp : Flag<["-", "/"], "Gpp">, HelpText<"Force partial precision">;
def Gfa : Flag<["-", "/"], "Gfa">, HelpText<"Avoid flow control constructs">, Flags<[CoreOption]>, Group<hlslcomp_Group>;
def Gfp : Flag<["-", "/"], "Gfp">, HelpText<"Prefer flow control constructs">, Flags<[CoreOption]>, Group<hlslcomp_Group>;
// /Gdp - disable effect performance mode - unsupported
def Ges : Flag<["-", "/"], "Ges">, HelpText<"Enable strict mode">, Flags<[CoreOption]>, Group<hlslcomp_Group>;
def Gis : Flag<["-", "/"], "Gis">, HelpText<"Force IEEE strictness">, Flags<[CoreOption]>, Group<hlslcomp_Group>;

def Fo : JoinedOrSeparate<["-", "/"], "Fo">, MetaVarName<"<file>">, HelpText<"Output object file">, Flags<[DriverOption]>, Group<hlslcomp_Group>;
// def Fl : JoinedOrSeparate<["-", "/"], "Fl">, MetaVarName<"<file>">, HelpText<"Output a library">;
def Fc : JoinedOrSeparate<["-", "/"], "Fc">, MetaVarName<"<file>">, HelpText<"Output assembly code listing file">, Flags<[DriverOption]>, Group<hlslcomp_Group>;
//def Fx : JoinedOrSeparate<["-", "/"], "Fx">, MetaVarName<"<file>">, HelpText<"Output assembly code and hex listing file">;
def Fh : JoinedOrSeparate<["-", "/"], "Fh">, MetaVarName<"<file>">, HelpText<"Output header file containing object code">, Flags<[DriverOption]>, Group<hlslcomp_Group>;
def Fe : JoinedOrSeparate<["-", "/"], "Fe">, MetaVarName<"<file>">, HelpText<"Output warnings and errors to a specific file">, Flags<[DriverOption]>, Group<hlslcomp_Group>;
def Fd : JoinedOrSeparate<["-", "/"], "Fd">, MetaVarName<"<file>">, HelpText<"Extract LLVM Debug IR and write to given file">, Flags<[DriverOption]>, Group<hlslcomp_Group>;
def Vn : JoinedOrSeparate<["-", "/"], "Vn">, MetaVarName<"<name>">, HelpText<"Use <name> as variable name in header file">, Flags<[DriverOption]>, Group<hlslcomp_Group>;
def Cc : Flag<["-", "/"], "Cc">, HelpText<"Output color coded assembly listings">, Group<hlslcomp_Group>, Flags<[DriverOption]>;
def Ni : Flag<["-", "/"], "Ni">, HelpText<"Output instruction numbers in assembly listings">, Group<hlslcomp_Group>, Flags<[DriverOption]>;
def No : Flag<["-", "/"], "No">, HelpText<"Output instruction byte offsets in assembly listings">, Group<hlslcomp_Group>, Flags<[DriverOption]>;
def Lx : Flag<["-", "/"], "Lx">, HelpText<"Output hexadecimal literals">, Group<hlslcomp_Group>, Flags<[DriverOption]>;

// In place of 'E' for clang; fxc uses 'E' for entry point.
def P : Separate<["-", "/"], "P">, Flags<[DriverOption]>, Group<hlslutil_Group>,
  HelpText<"Preprocess to file (must be used alone)">;

// @<file> - options response file

def dumpbin : Flag<["-", "/"], "dumpbin">, Flags<[DriverOption]>, Group<hlslutil_Group>,
  HelpText<"Load a binary file rather than compiling">;
def Qstrip_reflect : Flag<["-", "/"], "Qstrip_reflect">, Flags<[DriverOption]>, Group<hlslutil_Group>,
  HelpText<"Strip reflection data from shader bytecode  (must be used with /Fo <file>)">;
def Qstrip_debug : Flag<["-", "/"], "Qstrip_debug">, Flags<[DriverOption]>, Group<hlslutil_Group>,
  HelpText<"Strip debug information from 4_0+ shader bytecode  (must be used with /Fo <file>)">;
def Qstrip_priv : Flag<["-", "/"], "Qstrip_priv">, Flags<[DriverOption]>, Group<hlslutil_Group>,
  HelpText<"Strip private data from shader bytecode  (must be used with /Fo <file>)">;

def Qstrip_rootsignature : Flag<["-", "/"], "Qstrip_rootsignature">, Flags<[DriverOption]>, Group<hlslutil_Group>, HelpText<"Strip root signature data from shader bytecode  (must be used with /Fo <file>)">;
def setrootsignature     : JoinedOrSeparate<["-", "/"], "setrootsignature">,     MetaVarName<"<file>">, Flags<[DriverOption]>, Group<hlslutil_Group>, HelpText<"Attach root signature to shader bytecode">;
def extractrootsignature : Flag<["-", "/"], "extractrootsignature">, Flags<[DriverOption]>, Group<hlslutil_Group>, HelpText<"Extract root signature from shader bytecode (must be used with /Fo <file>)">;
def verifyrootsignature  : JoinedOrSeparate<["-", "/"], "verifyrootsignature">,  MetaVarName<"<file>">, Flags<[DriverOption]>, Group<hlslutil_Group>, HelpText<"Verify shader bytecode with root signature">;
def force_rootsig_ver    : JoinedOrSeparate<["-", "/"], "force_rootsig_ver">,    Flags<[CoreOption]>, MetaVarName<"<profile>">, Group<hlslcomp_Group>, HelpText<"force root signature version (rootsig_1_1 if omitted)">;

/*
def shtemplate : JoinedOrSeparate<["-", "/"], "shtemplate">, MetaVarName<"<file>">, Group<hlslcomp_Group>,
  HelpText<"Template shader file for merging/matching resources">;
def mergeUAVs : JoinedOrSeparate<["-", "/"], "mergeUAVs">, MetaVarName<"<file>">, Group<hlslcomp_Group>,
  HelpText<"Merge UAV slots of template shader and current shader">;
def matchUAVs : JoinedOrSeparate<["-", "/"], "matchUAVs">, MetaVarName<"<file>">, Group<hlslcomp_Group>,
  HelpText<"Match template shader UAV slots in current shader">;
def res_may_alias : Flag<["-", "/"], "res_may_alias">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Assume that UAVs/SRVs may alias">;
def enable_unbounded_descriptor_tables : Flag<["-", "/"], "enable_unbounded_descriptor_tables">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Enables unbounded descriptor tables">;
*/
def all_resources_bound : Flag<["-", "/"], "all_resources_bound">, Flags<[CoreOption]>, Group<hlslcomp_Group>,
  HelpText<"Enables agressive flattening">;

def setprivate : JoinedOrSeparate<["-", "/"], "setprivate">, Flags<[DriverOption]>, MetaVarName<"<file>">, Group<hlslutil_Group>,
  HelpText<"Private data to add to compiled shader blob">;
def getprivate : JoinedOrSeparate<["-", "/"], "getprivate">, Flags<[DriverOption]>, MetaVarName<"<file>">, Group<hlslutil_Group>,
  HelpText<"Save private data from shader blob">;

def nologo : Flag<["-", "/"], "nologo">, Group<hlslcore_Group>, Flags<[DriverOption]>,
  HelpText<"Suppress copyright message">;

// Also removed: compress, decompress, /Gch (child effect), /Gec (back compat), /Gpp (partial precision)
// /Op - no support for preshaders.

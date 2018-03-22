//===------ FeatureManager.h - SPIR-V Version/Extension Manager -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file defines a SPIR-V version and extension manager.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_FEATUREMANAGER_H
#define LLVM_CLANG_LIB_SPIRV_FEATUREMANAGER_H

#include <string>

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

/// A list of SPIR-V extensions known to our CodeGen.
enum class Extension {
  KHR_multiview,
  KHR_shader_draw_parameters,
  EXT_fragment_fully_covered,
  EXT_shader_stencil_export,
  AMD_gpu_shader_half_float,
  AMD_shader_explicit_vertex_parameter,
  GOOGLE_decorate_string,
  GOOGLE_hlsl_functionality1,
  Unknown,
};

/// The class for handling SPIR-V version and extension requests.
class FeatureManager {
public:
  explicit FeatureManager(DiagnosticsEngine &de);

  /// Allows the given extension to be used in CodeGen.
  bool allowExtension(llvm::StringRef);
  /// Allows all extensions to be used in CodeGen.
  void allowAllKnownExtensions();
  /// Rqeusts the given extension for translating the given target feature at
  /// the given source location. Emits an error if the given extension is not
  /// permitted to use.
  bool requestExtension(Extension, llvm::StringRef target, SourceLocation);

  /// Translates extension name to symbol.
  static Extension getExtensionSymbol(llvm::StringRef name);
  /// Translates extension symbol to name.
  static const char *getExtensionName(Extension symbol);

  /// Returns the names of all known extensions as a string.
  std::string getKnownExtensions(const char *delimiter, const char *prefix = "",
                                 const char *postfix = "");

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this object.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create an note message and report it
  /// in the diagnostic engine associated with this object.
  template <unsigned N>
  DiagnosticBuilder emitNote(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Note, message);
    return diags.Report(loc, diagId);
  }

  DiagnosticsEngine &diags;

  llvm::SmallBitVector allowedExtensions;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_FEATUREMANAGER_H

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

#include "spirv-tools/libspirv.h"

#include "dxc/Support/SPIRVOptions.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

/// A list of SPIR-V extensions known to our CodeGen.
enum class Extension {
  KHR = 0,
  KHR_16bit_storage,
  KHR_device_group,
  KHR_non_semantic_info,
  KHR_multiview,
  KHR_shader_draw_parameters,
  KHR_post_depth_coverage,
  KHR_ray_tracing,
  EXT_demote_to_helper_invocation,
  EXT_descriptor_indexing,
  EXT_fragment_fully_covered,
  EXT_fragment_invocation_density,
  EXT_shader_stencil_export,
  EXT_shader_viewport_index_layer,
  AMD_gpu_shader_half_float,
  AMD_shader_explicit_vertex_parameter,
  GOOGLE_hlsl_functionality1,
  GOOGLE_user_type,
  NV_ray_tracing,
  NV_mesh_shader,
  KHR_ray_query,
  Unknown,
};

/// The class for handling SPIR-V version and extension requests.
class FeatureManager {
public:
  FeatureManager(DiagnosticsEngine &de, const SpirvCodeGenOptions &);

  /// Allows the given extension to be used in CodeGen.
  bool allowExtension(llvm::StringRef);

  /// Allows all extensions to be used in CodeGen.
  void allowAllKnownExtensions();

  /// Rqeusts the given extension for translating the given target feature at
  /// the given source location. Emits an error if the given extension is not
  /// permitted to use.
  bool requestExtension(Extension, llvm::StringRef target, SourceLocation);

  /// Translates extension name to symbol.
  Extension getExtensionSymbol(llvm::StringRef name);

  /// Translates extension symbol to name.
  const char *getExtensionName(Extension symbol);

  /// Returns true if the given extension is a KHR extension.
  bool isKHRExtension(llvm::StringRef name);

  /// Returns the names of all known extensions as a string.
  std::string getKnownExtensions(const char *delimiter, const char *prefix = "",
                                 const char *postfix = "");

  /// Rqeusts the given target environment for translating the given feature at
  /// the given source location. Emits an error if the requested target
  /// environment does not match user's target environemnt.
  bool requestTargetEnv(spv_target_env, llvm::StringRef target, SourceLocation);

  /// Returns the target environment corresponding to the target environment
  /// that was specified as command line option. If no option is specified, the
  /// default (Vulkan 1.0) is returned.
  spv_target_env getTargetEnv() const { return targetEnv; }

  /// Returns true if the given extension is not part of the core of the target
  /// environment.
  bool isExtensionRequiredForTargetEnv(Extension);

  /// Returns true if the given extension is set in allowedExtensions
  bool isExtensionEnabled(Extension);

private:
  /// Returns whether codegen should allow usage of this extension by default.
  bool enabledByDefault(Extension);

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
  spv_target_env targetEnv;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_FEATUREMANAGER_H

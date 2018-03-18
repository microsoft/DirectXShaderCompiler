//===---- FeatureManager.cpp - SPIR-V Version/Extension Manager -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/FeatureManager.h"

#include "llvm/ADT/StringSwitch.h"

namespace clang {
namespace spirv {

FeatureManager::FeatureManager(DiagnosticsEngine &de) : diags(de) {
  allowedExtensions.resize(static_cast<unsigned>(Extension::Unknown) + 1);
}

bool FeatureManager::allowExtension(llvm::StringRef name) {
  const auto symbol = getExtensionSymbol(name);
  if (symbol == Extension::Unknown) {
    emitError("unknown SPIR-V extension '%0'", {}) << name;
    return false;
  }

  allowedExtensions.set(static_cast<unsigned>(symbol));
  if (symbol == Extension::GOOGLE_hlsl_functionality1)
    allowedExtensions.set(
        static_cast<unsigned>(Extension::GOOGLE_decorate_string));

  return true;
}

void FeatureManager::allowAllKnownExtensions() { allowedExtensions.set(); }

bool FeatureManager::requestExtension(Extension ext, llvm::StringRef target,
                                      SourceLocation srcLoc) {
  if (allowedExtensions.test(static_cast<unsigned>(ext)))
    return true;

  emitError("SPIR-V extension '%0' required for %1 but not permitted to use",
            srcLoc)
      << getExtensionName(ext) << target;
  return false;
}

Extension FeatureManager::getExtensionSymbol(llvm::StringRef name) {
  return llvm::StringSwitch<Extension>(name)
      .Case("SPV_KHR_multiview", Extension::KHR_multiview)
      .Case("SPV_KHR_shader_draw_parameters",
            Extension::KHR_shader_draw_parameters)
      .Case("SPV_EXT_shader_stencil_export",
            Extension::EXT_shader_stencil_export)
      .Case("SPV_AMD_gpu_shader_half_float",
            Extension::AMD_gpu_shader_half_float)
      .Case("SPV_AMD_shader_explicit_vertex_parameter",
            Extension::AMD_shader_explicit_vertex_parameter)
      .Case("SPV_GOOGLE_decorate_string", Extension::GOOGLE_decorate_string)
      .Case("SPV_GOOGLE_hlsl_functionality1",
            Extension::GOOGLE_hlsl_functionality1)
      .Default(Extension::Unknown);
}

const char *FeatureManager::getExtensionName(Extension symbol) {
  switch (symbol) {
  case Extension::KHR_multiview:
    return "SPV_KHR_multiview";
  case Extension::KHR_shader_draw_parameters:
    return "SPV_KHR_shader_draw_parameters";
  case Extension::EXT_shader_stencil_export:
    return "SPV_EXT_shader_stencil_export";
  case Extension::AMD_gpu_shader_half_float:
    return "SPV_AMD_gpu_shader_half_float";
  case Extension::AMD_shader_explicit_vertex_parameter:
    return "SPV_AMD_shader_explicit_vertex_parameter";
  case Extension::GOOGLE_decorate_string:
    return "SPV_GOOGLE_decorate_string";
  case Extension::GOOGLE_hlsl_functionality1:
    return "SPV_GOOGLE_hlsl_functionality1";
  default:
    break;
  }
  return "<unknown extension>";
}

} // end namespace spirv
} // end namespace clang

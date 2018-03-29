//===---- FeatureManager.cpp - SPIR-V Version/Extension Manager -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/FeatureManager.h"

#include <sstream>

#include "llvm/ADT/StringSwitch.h"

namespace clang {
namespace spirv {

FeatureManager::FeatureManager(DiagnosticsEngine &de,
                               const EmitSPIRVOptions &opts)
    : diags(de) {
  allowedExtensions.resize(static_cast<unsigned>(Extension::Unknown) + 1);

  if (opts.allowedExtensions.empty()) {
    // If no explicit extension control from command line, use the default mode:
    // allowing all extensions.
    allowAllKnownExtensions();
  } else {
    for (auto ext : opts.allowedExtensions)
      allowExtension(ext);
  }

  if (opts.targetEnv == "vulkan1.0")
    targetEnv = SPV_ENV_VULKAN_1_0;
  else if (opts.targetEnv == "vulkan1.1")
    targetEnv = SPV_ENV_VULKAN_1_1;
  else {
    emitError("unknown SPIR-V target environment '%0'", {}) << opts.targetEnv;
    emitNote("allowed options are:\n vulkan1.0\n vulkan1.1", {});
  }
}

bool FeatureManager::allowExtension(llvm::StringRef name) {
  const auto symbol = getExtensionSymbol(name);
  if (symbol == Extension::Unknown) {
    emitError("unknown SPIR-V extension '%0'", {}) << name;
    emitNote("known extensions are\n%0", {})
        << getKnownExtensions("\n* ", "* ");
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

bool FeatureManager::requestTargetEnv(spv_target_env requestedEnv,
                                      llvm::StringRef target,
                                      SourceLocation srcLoc) {
  if (targetEnv == SPV_ENV_VULKAN_1_0 && requestedEnv == SPV_ENV_VULKAN_1_1) {
    emitError("Vulkan 1.1 is required for %0 but not permitted to use", srcLoc)
        << target;
    emitNote("please specify your target environment via command line option -fspv-target-env=",
             {});
    return false;
  }
  return true;
}

Extension FeatureManager::getExtensionSymbol(llvm::StringRef name) {
  return llvm::StringSwitch<Extension>(name)
      .Case("SPV_KHR_device_group", Extension::KHR_device_group)
      .Case("SPV_KHR_multiview", Extension::KHR_multiview)
      .Case("SPV_KHR_shader_draw_parameters",
            Extension::KHR_shader_draw_parameters)
      .Case("SPV_EXT_fragment_fully_covered",
            Extension::EXT_fragment_fully_covered)
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
  case Extension::KHR_device_group:
    return "SPV_KHR_device_group";
  case Extension::KHR_multiview:
    return "SPV_KHR_multiview";
  case Extension::KHR_shader_draw_parameters:
    return "SPV_KHR_shader_draw_parameters";
  case Extension::EXT_fragment_fully_covered:
    return "SPV_EXT_fragment_fully_covered";
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

std::string FeatureManager::getKnownExtensions(const char *delimiter,
                                               const char *prefix,
                                               const char *postfix) {
  std::ostringstream oss;

  oss << prefix;

  const auto numExtensions = static_cast<uint32_t>(Extension::Unknown);
  for (uint32_t i = 0; i < numExtensions; ++i) {
    oss << getExtensionName(static_cast<Extension>(i));
    if (i + 1 < numExtensions)
      oss << delimiter;
  }

  oss << postfix;

  return oss.str();
}

bool FeatureManager::isExtensionRequiredForTargetEnv(Extension ext) {
  bool required = true;
  if (targetEnv == SPV_ENV_VULKAN_1_1) {
    // The following extensions are incorporated into Vulkan 1.1, and are
    // therefore not required to be emitted for that target environment. The
    // last 3 are currently not supported by the FeatureManager.
    // TODO: Add the last 3 extensions to the list if we start to support them.
    // SPV_KHR_shader_draw_parameters
    // SPV_KHR_device_group
    // SPV_KHR_multiview
    // SPV_KHR_16bit_storage
    // SPV_KHR_storage_buffer_storage_class
    // SPV_KHR_variable_pointers
    switch (ext) {
    case Extension::KHR_shader_draw_parameters:
    case Extension::KHR_device_group:
    case Extension::KHR_multiview:
      required = false;
    }
  }

  return required;
}

} // end namespace spirv
} // end namespace clang

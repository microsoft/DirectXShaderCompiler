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
                               const SpirvCodeGenOptions &opts)
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
  else if (opts.targetEnv == "vulkan1.2")
    targetEnv = SPV_ENV_VULKAN_1_2;
  else {
    emitError("unknown SPIR-V target environment '%0'", {}) << opts.targetEnv;
    emitNote("allowed options are:\n vulkan1.0\n vulkan1.1\n vulkan1.2", {});
  }
}

bool FeatureManager::allowExtension(llvm::StringRef name) {
  // Special case: If we are asked to allow "SPV_KHR" extension, it indicates
  // that we should allow using *all* KHR extensions.
  if (getExtensionSymbol(name) == Extension::KHR) {
    bool result = true;
    for (uint32_t i = 0; i < static_cast<uint32_t>(Extension::Unknown); ++i) {
      llvm::StringRef extName(getExtensionName(static_cast<Extension>(i)));
      if (isKHRExtension(extName))
        result = result && allowExtension(extName);
    }
    return result;
  }

  const auto symbol = getExtensionSymbol(name);
  if (symbol == Extension::Unknown) {
    emitError("unknown SPIR-V extension '%0'", {}) << name;
    emitNote("known extensions are\n%0", {})
        << getKnownExtensions("\n* ", "* ");
    return false;
  }

  allowedExtensions.set(static_cast<unsigned>(symbol));

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
  if (targetEnv < requestedEnv) {
    emitError("%0 is required for %1 but not permitted to use", srcLoc)
        << (requestedEnv == SPV_ENV_VULKAN_1_2 ? "Vulkan 1.2" : "Vulkan 1.1")
        << target;
    emitNote("please specify your target environment via command line option "
             "-fspv-target-env=",
             {});
    return false;
  }
  return true;
}

Extension FeatureManager::getExtensionSymbol(llvm::StringRef name) {
  return llvm::StringSwitch<Extension>(name)
      .Case("KHR", Extension::KHR)
      .Case("SPV_KHR_16bit_storage", Extension::KHR_16bit_storage)
      .Case("SPV_KHR_device_group", Extension::KHR_device_group)
      .Case("SPV_KHR_multiview", Extension::KHR_multiview)
      .Case("SPV_KHR_shader_draw_parameters",
            Extension::KHR_shader_draw_parameters)
      .Case("SPV_EXT_descriptor_indexing", Extension::EXT_descriptor_indexing)
      .Case("SPV_EXT_fragment_fully_covered",
            Extension::EXT_fragment_fully_covered)
      .Case("SPV_EXT_fragment_invocation_density",
            Extension::EXT_fragment_invocation_density)
      .Case("SPV_EXT_shader_stencil_export",
            Extension::EXT_shader_stencil_export)
      .Case("SPV_EXT_shader_viewport_index_layer",
            Extension::EXT_shader_viewport_index_layer)
      .Case("SPV_AMD_gpu_shader_half_float",
            Extension::AMD_gpu_shader_half_float)
      .Case("SPV_AMD_shader_explicit_vertex_parameter",
            Extension::AMD_shader_explicit_vertex_parameter)
      .Case("SPV_GOOGLE_hlsl_functionality1",
            Extension::GOOGLE_hlsl_functionality1)
      .Case("SPV_GOOGLE_user_type",
            Extension::GOOGLE_user_type)
      .Case("SPV_KHR_post_depth_coverage", Extension::KHR_post_depth_coverage)
      .Case("SPV_NV_ray_tracing", Extension::NV_ray_tracing)
      .Case("SPV_NV_mesh_shader", Extension::NV_mesh_shader)
      .Default(Extension::Unknown);
}

const char *FeatureManager::getExtensionName(Extension symbol) {
  switch (symbol) {
  case Extension::KHR:
    return "KHR";
  case Extension::KHR_16bit_storage:
    return "SPV_KHR_16bit_storage";
  case Extension::KHR_device_group:
    return "SPV_KHR_device_group";
  case Extension::KHR_multiview:
    return "SPV_KHR_multiview";
  case Extension::KHR_shader_draw_parameters:
    return "SPV_KHR_shader_draw_parameters";
  case Extension::KHR_post_depth_coverage:
    return "SPV_KHR_post_depth_coverage";
  case Extension::EXT_descriptor_indexing:
    return "SPV_EXT_descriptor_indexing";
  case Extension::EXT_fragment_fully_covered:
    return "SPV_EXT_fragment_fully_covered";
  case Extension::EXT_fragment_invocation_density:
    return "SPV_EXT_fragment_invocation_density";
  case Extension::EXT_shader_stencil_export:
    return "SPV_EXT_shader_stencil_export";
  case Extension::EXT_shader_viewport_index_layer:
    return "SPV_EXT_shader_viewport_index_layer";
  case Extension::AMD_gpu_shader_half_float:
    return "SPV_AMD_gpu_shader_half_float";
  case Extension::AMD_shader_explicit_vertex_parameter:
    return "SPV_AMD_shader_explicit_vertex_parameter";
  case Extension::GOOGLE_hlsl_functionality1:
    return "SPV_GOOGLE_hlsl_functionality1";
  case Extension::GOOGLE_user_type:
    return "SPV_GOOGLE_user_type";
  case Extension::NV_ray_tracing:
    return "SPV_NV_ray_tracing";
  case Extension::NV_mesh_shader:
    return "SPV_NV_mesh_shader";
  default:
    break;
  }
  return "<unknown extension>";
}

bool FeatureManager::isKHRExtension(llvm::StringRef name) {
  return name.startswith_lower("spv_khr_");
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
  if (targetEnv >= SPV_ENV_VULKAN_1_1) {
    // The following extensions are incorporated into Vulkan 1.1 or above, and
    // are therefore not required to be emitted for that target environment.
    // TODO: Also add the following extensions  if we start to support them.
    // * SPV_KHR_storage_buffer_storage_class
    // * SPV_KHR_variable_pointers
    switch (ext) {
    case Extension::KHR_16bit_storage:
    case Extension::KHR_device_group:
    case Extension::KHR_multiview:
    case Extension::KHR_shader_draw_parameters:
      required = false;
      break;
    default:
      // Only 1.1 or above extensions can be suppressed.
      required = true;
    }
  }

  return required;
}

} // end namespace spirv
} // end namespace clang

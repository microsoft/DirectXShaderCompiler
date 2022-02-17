///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxil2spv.h                                                                //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides wrappers to dxil2spv main function.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef __DXIL2SPV_DXIL2SPV__
#define __DXIL2SPV_DXIL2SPV__

#include "dxc/DXIL/DxilSignature.h"
#include "dxc/Support/SPIRVOptions.h"
#include "dxc/dxcapi.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvContext.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace dxil2spv {

class Translator {
public:
  Translator(CompilerInstance &instance);
  int Run(CComPtr<IDxcBlobEncoding> blob);

private:
  CompilerInstance &ci;
  DiagnosticsEngine &diagnosticsEngine;
  spirv::SpirvCodeGenOptions &spirvOptions;
  spirv::SpirvContext spvContext;
  spirv::FeatureManager featureManager;
  spirv::SpirvBuilder spvBuilder;

  const spirv::SpirvType *toSpirvType(hlsl::CompType compType);
  const spirv::SpirvType *toSpirvType(hlsl::DxilSignatureElement *elem);

  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]);
};

} // namespace dxil2spv
} // namespace clang

#endif // __DXIL2SPV_DXIL2SPV__

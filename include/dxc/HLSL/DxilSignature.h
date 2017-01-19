///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignature.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Representation of HLSL shader signature.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilSignatureElement.h"
#include <memory>
#include <string>
#include <vector>


namespace hlsl {

/// Use this class to represent HLSL signature.
class DxilSignature {
public:
  using Kind = DXIL::SignatureKind;

  DxilSignature(DXIL::ShaderKind shaderKind, DXIL::SignatureKind sigKind);
  DxilSignature(DXIL::SigPointKind sigPointKind);
  virtual ~DxilSignature();

  bool IsInput() const;
  bool IsOutput() const;

  virtual std::unique_ptr<DxilSignatureElement> CreateElement();

  unsigned AppendElement(std::unique_ptr<DxilSignatureElement> pSE, bool bSetID = true);

  DxilSignatureElement &GetElement(unsigned idx);
  const DxilSignatureElement &GetElement(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilSignatureElement> > &GetElements() const;

  // Packs the signature elements per DXIL constraints and returns the number of rows used for the signature
  unsigned PackElements();

  // Returns true if all signature elements that should be allocated are allocated
  bool IsFullyAllocated();

private:
  DXIL::SigPointKind m_sigPointKind;
  std::vector<std::unique_ptr<DxilSignatureElement> > m_Elements;
};

} // namespace hlsl

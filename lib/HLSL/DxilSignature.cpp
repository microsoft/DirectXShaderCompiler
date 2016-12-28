///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignature.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilSignature.h"
#include "dxc/HLSL/DxilSigPoint.h"

using std::vector;
using std::unique_ptr;


namespace hlsl {

//------------------------------------------------------------------------------
//
// Singnature methods.
//
DxilSignature::DxilSignature(DXIL::ShaderKind shaderKind, DXIL::SignatureKind sigKind)
: m_sigPointKind(SigPoint::GetKind(shaderKind, sigKind, /*isPatchConstantFunction*/false, /*isSpecialInput*/false)) {}

DxilSignature::DxilSignature(DXIL::SigPointKind sigPointKind)
: m_sigPointKind(sigPointKind) {}

DxilSignature::~DxilSignature() {
}

bool DxilSignature::IsInput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsInput();
}

bool DxilSignature::IsOutput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsOutput();
}

unique_ptr<DxilSignatureElement> DxilSignature::CreateElement() {
  return unique_ptr<DxilSignatureElement>(new DxilSignatureElement(m_sigPointKind));
}

unsigned DxilSignature::AppendElement(std::unique_ptr<DxilSignatureElement> pSE, bool bSetID) {
  DXASSERT_NOMSG((unsigned)m_Elements.size() < UINT_MAX);
  unsigned Id = (unsigned)m_Elements.size();
  if (bSetID) {
    pSE->SetID(Id);
  }
  m_Elements.emplace_back(std::move(pSE));
  return Id;
}

DxilSignatureElement &DxilSignature::GetElement(unsigned idx) {
  return *m_Elements[idx].get();
}

const DxilSignatureElement &DxilSignature::GetElement(unsigned idx) const {
  return *m_Elements[idx].get();
}

const std::vector<std::unique_ptr<DxilSignatureElement> > &DxilSignature::GetElements() const {
  return m_Elements;
}

} // namespace hlsl

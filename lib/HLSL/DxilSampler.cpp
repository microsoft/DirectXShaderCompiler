///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSampler.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilSampler.h"
#include "dxc/Support/Global.h"


namespace hlsl {

//------------------------------------------------------------------------------
//
// Sampler class methods.
//
DxilSampler::DxilSampler()
    : DxilResourceBase(DxilResourceBase::Class::Sampler),
      m_SamplerKind(DXIL::SamplerKind::Invalid) {
  DxilResourceBase::SetKind(DxilResourceBase::Kind::Sampler);
}

DxilSampler::SamplerKind DxilSampler::GetSamplerKind() const  { return m_SamplerKind; }
bool DxilSampler::IsCompSampler() const         { return m_SamplerKind == SamplerKind::Comparison; }

void DxilSampler::SetSamplerKind(SamplerKind K)  { m_SamplerKind = K; }

} // namespace hlsl

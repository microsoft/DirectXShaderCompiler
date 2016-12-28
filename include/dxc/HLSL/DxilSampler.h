///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSampler.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Representation of HLSL sampler state.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilResourceBase.h"

namespace hlsl {

/// Use this class to represent HLSL sampler state.
class DxilSampler : public DxilResourceBase {
public:
  using SamplerKind = DXIL::SamplerKind;

  DxilSampler();

  SamplerKind GetSamplerKind() const;
  bool IsCompSampler() const;

  void SetSamplerKind(SamplerKind K);

private:
  SamplerKind m_SamplerKind;      // Sampler mode.
};

} // namespace hlsl

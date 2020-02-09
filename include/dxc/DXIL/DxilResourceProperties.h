///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilHandleAnnotation.h                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Representation properties for DXIL handle.                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "DxilConstants.h"

namespace llvm {
class Constant;
class Type;
}

namespace hlsl {

struct DxilResourceProperties {
  DXIL::ResourceClass Class;
  DXIL::ResourceKind  Kind;
  static constexpr unsigned kSampleCountUndefined = 0x7;

  union {
    struct {
      DXIL::ComponentType CompType : 5; // TypedBuffer/Image.
      // 2^SampleCountPow2 for Sample count of Texture2DMS.
      unsigned SampleCountPow2 : 3;
      unsigned Reserved : 24;
    } Typed;
    unsigned ElementStride; // in bytes for StructurizedBuffer.
    DXIL::SamplerFeedbackType SamplerFeedbackType; // FeedbackTexture2D.
    unsigned SizeInBytes; // Cbuffer instance size in bytes.
    uint32_t RawDword0;
  };

  union {
    struct {
      unsigned bROV : 1;              // UAV
      unsigned bGloballyCoherent : 1; // UAV
      unsigned Reserved : 30;
    } UAV;
    uint32_t RawDword1;
  };

};

class ShaderModel;

namespace resource_helper {
llvm::Constant *getAsConstant(const DxilResourceProperties &,
                              llvm::Type *Ty, const ShaderModel &);
DxilResourceProperties loadFromConstant(const llvm::Constant &,
                                        DXIL::ResourceClass RC,
                                        DXIL::ResourceKind RK,
                                        llvm::Type *Ty,
                                        const ShaderModel &);
}; // namespace resource_helper

} // namespace hlsl

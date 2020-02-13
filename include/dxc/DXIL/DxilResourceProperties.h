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
      uint32_t SingleComponent : 1;     // Return type is single component.
      // 2^SampleCountPow2 for Sample count of Texture2DMS.
      uint32_t SampleCountPow2 : 3;
      uint32_t Reserved : 23;
    } Typed;
    uint32_t ElementStride; // in bytes for StructurizedBuffer.
    DXIL::SamplerFeedbackType SamplerFeedbackType; // FeedbackTexture2D.
    uint32_t SizeInBytes; // Cbuffer instance size in bytes.
    uint32_t RawDword0;
  };

  union {
    struct {
      uint32_t bROV : 1;              // UAV
      uint32_t bGloballyCoherent : 1; // UAV
      uint32_t Reserved : 30;
    } UAV;
    uint32_t RawDword1;
  };

  bool operator==(const DxilResourceProperties &);
  bool operator!=(const DxilResourceProperties &);
};

static_assert(sizeof(DxilResourceProperties) == 4 * sizeof(uint32_t),
              "update shader model and functions read/write "
              "DxilResourceProperties when size is changed");

class ShaderModel;
class DxilResourceBase;
struct DxilInst_AnnotateHandle;

namespace resource_helper {
llvm::Constant *getAsConstant(const DxilResourceProperties &, llvm::Type *Ty,
                              const ShaderModel &);
DxilResourceProperties
loadFromAnnotateHandle(DxilInst_AnnotateHandle &annotateHandle, llvm::Type *Ty,
                       const ShaderModel &);
DxilResourceProperties loadFromResourceBase(DxilResourceBase *);

}; // namespace resource_helper

} // namespace hlsl

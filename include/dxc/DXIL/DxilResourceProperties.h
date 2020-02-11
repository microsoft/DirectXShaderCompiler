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
      unsigned SingleComponent : 1; // Return type is single component.
      // 2^SampleCountPow2 for Sample count of Texture2DMS.
      unsigned SampleCountPow2 : 3;
      unsigned Reserved : 23;
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

  bool operator==(const DxilResourceProperties &);
  bool operator!=(const DxilResourceProperties &);
};

class ShaderModel;
class DxilResourceBase;
struct DxilInst_AnnotateHandle;

namespace resource_helper {
llvm::Constant *getAsConstant(const DxilResourceProperties &, llvm::Type *Ty,
                              const ShaderModel &);
DxilResourceProperties loadFromConstant(const llvm::Constant &,
                                        DXIL::ResourceClass RC,
                                        DXIL::ResourceKind RK, llvm::Type *Ty,
                                        const ShaderModel &);
DxilResourceProperties
loadFromAnnotateHandle(DxilInst_AnnotateHandle &annotateHandle, llvm::Type *Ty,
                       const ShaderModel &);
DxilResourceProperties loadFromResourceBase(DxilResourceBase *);
bool IsResourceSingleComponent(llvm::Type *Ty);

bool IsAnyTexture(DXIL::ResourceKind ResourceKind);
bool IsStructuredBuffer(DXIL::ResourceKind ResourceKind);
bool IsTypedBuffer(DXIL::ResourceKind ResourceKind);
bool IsTyped(DXIL::ResourceKind ResourceKind);
bool IsRawBuffer(DXIL::ResourceKind ResourceKind);
bool IsTBuffer(DXIL::ResourceKind ResourceKind);
bool IsFeedbackTexture(DXIL::ResourceKind ResourceKind);

}; // namespace resource_helper

} // namespace hlsl

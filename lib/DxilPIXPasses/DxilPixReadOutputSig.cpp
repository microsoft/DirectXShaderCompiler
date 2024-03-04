///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPixEmitMetadata.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Emit Dxil resource meta data                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilFunctionProps.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/DxilPIXPasses/DxilPIXVirtualRegisters.h"
#include "dxc/HLSL/DxilGenerationPass.h"

#include "llvm/IR/Module.h"

using namespace llvm;
using namespace hlsl;


const char* SemanticKindName(DXIL::SemanticKind Kind) {
    switch (Kind) {
    case DXIL::SemanticKind::Arbitrary:
        return "Arbitrary";
    case DXIL::SemanticKind::VertexID:
        return "VertexID";
    case DXIL::SemanticKind::InstanceID:
        return "InstanceID";
    case DXIL::SemanticKind::Position:
        return "Position";
    case DXIL::SemanticKind::RenderTargetArrayIndex:
        return "RenderTargetArrayIndex";
    case DXIL::SemanticKind::ViewPortArrayIndex:
        return "ViewPortArrayIndex";
    case DXIL::SemanticKind::ClipDistance:
        return "ClipDistance";
    case DXIL::SemanticKind::CullDistance:
        return "CullDistance";
    case DXIL::SemanticKind::OutputControlPointID:
        return "OutputControlPointID";
    case DXIL::SemanticKind::DomainLocation:
        return "DomainLocation";
    case DXIL::SemanticKind::PrimitiveID:
        return "PrimitiveID";
    case DXIL::SemanticKind::GSInstanceID:
        return "GSInstanceID";
    case DXIL::SemanticKind::SampleIndex:
        return "SampleIndex";
    case DXIL::SemanticKind::IsFrontFace:
        return "IsFrontFace";
    case DXIL::SemanticKind::Coverage:
        return "Coverage";
    case DXIL::SemanticKind::InnerCoverage:
        return "InnerCoverage";
    case DXIL::SemanticKind::Target:
        return "Target";
    case DXIL::SemanticKind::Depth:
        return "Depth";
    case DXIL::SemanticKind::DepthLessEqual:
        return "DepthLessEqual";
    case DXIL::SemanticKind::DepthGreaterEqual:
        return "DepthGreaterEqual";
    case DXIL::SemanticKind::StencilRef:
        return "StencilRef";
    case DXIL::SemanticKind::DispatchThreadID:
        return "DispatchThreadID";
    case DXIL::SemanticKind::GroupID:
        return "GroupID";
    case DXIL::SemanticKind::GroupIndex:
        return "GroupIndex";
    case DXIL::SemanticKind::GroupThreadID:
        return "GroupThreadID";
    case DXIL::SemanticKind::TessFactor:
        return "TessFactor";
    case DXIL::SemanticKind::InsideTessFactor:
        return "InsideTessFactor";
    case DXIL::SemanticKind::ViewID:
        return "ViewID";
    case DXIL::SemanticKind::Barycentrics:
        return "Barycentrics";
    case DXIL::SemanticKind::ShadingRate:
        return "ShadingRate";
    case DXIL::SemanticKind::CullPrimitive:
        return "CullPrimitive";
    case DXIL::SemanticKind::StartVertexLocation:
        return "StartVertexLocation";
    case DXIL::SemanticKind::StartInstanceLocation:
        return "StartInstanceLocation";
    case DXIL::SemanticKind::Invalid:
        return "Invalid";
    }
    return "Unknown";
}
class DxilPixReadOutputSig : public ModulePass {
public:
  static char ID;
  explicit DxilPixReadOutputSig() : ModulePass(ID) {}
  bool runOnModule(Module &M) override {
      if (OSOverride != nullptr) {
          DxilModule& DM = M.GetOrCreateDxilModule();
          auto const& Meta = DM.GetOutputSignature();
          auto const& Elements = Meta.GetElements();
          for (auto const& Element : Elements) {
              *OSOverride << "OutputSigElement:" << Element->GetName() << ":"
                  << SemanticKindName(Element->GetKind()) << "="
                  << std::to_string(Element->GetStartRow()) << "-"
                  << std::to_string(Element->GetStartCol()) << "-"
                  << std::to_string(Element->GetRows()) << "-"
                  << std::to_string(Element->GetCols()) << "\n";
          }
      }
    return false;
  }
};

char DxilPixReadOutputSig::ID = 0;

ModulePass *createDxilPixReadOutputSigPass() {
  return new DxilPixReadOutputSig();
}

INITIALIZE_PASS(DxilPixReadOutputSig, "dxil-read-output-sig",
                "Read and print module output sig", false, false)

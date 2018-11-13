///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerReader.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for manipulating DXIL container structures.              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/DXIL/DxilSubobject.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/DxilContainer/DxilRuntimeReflection.h"

namespace hlsl {

bool LoadSubobjectsFromRDAT(DxilSubobjects &subobjects, RDAT::SubobjectTableReader *pSubobjectTableReader) {
  if (!pSubobjectTableReader)
    return false;
  bool result = true;
  for (unsigned i = 0; i < pSubobjectTableReader->GetCount(); ++i) {
    try {
      auto reader = pSubobjectTableReader->GetItem(i);
      DXIL::SubobjectKind kind = reader.GetKind();
      bool bLocalRS = false;
      switch (kind) {
      case DXIL::SubobjectKind::StateObjectConfig:
        subobjects.CreateStateObjectConfig(reader.GetName(),
          reader.GetStateObjectConfig_Flags());
        break;
      case DXIL::SubobjectKind::LocalRootSignature:
        bLocalRS = true;
      case DXIL::SubobjectKind::GlobalRootSignature: {
        const void *pOutBytes;
        uint32_t OutSizeInBytes;
        if (!reader.GetRootSignature(&pOutBytes, &OutSizeInBytes)) {
          result = false;
          continue;
        }
        subobjects.CreateRootSignature(reader.GetName(), bLocalRS, pOutBytes, OutSizeInBytes);
        break;
      }
      case DXIL::SubobjectKind::SubobjectToExportsAssociation: {
        uint32_t NumExports = reader.GetSubobjectToExportsAssociation_NumExports();
        std::vector<llvm::StringRef> Exports;
        Exports.resize(NumExports);
        for (unsigned i = 0; i < NumExports; ++i) {
          Exports[i] = reader.GetSubobjectToExportsAssociation_Export(i);
        }
        subobjects.CreateSubobjectToExportsAssociation(reader.GetName(),
          reader.GetSubobjectToExportsAssociation_Subobject(),
          Exports.data(), NumExports);
        break;
      }
      case DXIL::SubobjectKind::RaytracingShaderConfig:
        subobjects.CreateRaytracingShaderConfig(reader.GetName(),
          reader.GetRaytracingShaderConfig_MaxPayloadSizeInBytes(),
          reader.GetRaytracingShaderConfig_MaxAttributeSizeInBytes());
        break;
      case DXIL::SubobjectKind::RaytracingPipelineConfig:
        subobjects.CreateRaytracingPipelineConfig(reader.GetName(),
          reader.GetRaytracingPipelineConfig_MaxTraceRecursionDepth());
        break;
      case DXIL::SubobjectKind::HitGroup:
        subobjects.CreateHitGroup(reader.GetName(),
          reader.GetHitGroup_Type(),
          reader.GetHitGroup_AnyHit(),
          reader.GetHitGroup_ClosestHit(),
          reader.GetHitGroup_Intersection());
          break;
      }
    } catch (hlsl::Exception &) {
      result = false;
    }
  }
  return result;
}


} // namespace hlsl

// DxilRuntimeReflection implementation
#include "dxc/DxilContainer/DxilRuntimeReflection.inl"

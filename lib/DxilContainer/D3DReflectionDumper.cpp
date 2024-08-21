///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// D3DReflectionDumper.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Use this to dump D3D Reflection data for testing.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Test/D3DReflectionDumper.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/Test/D3DReflectionStrings.h"
#include "dxc/dxcapi.h"
#include <sstream>

namespace hlsl {
namespace dump {

void D3DReflectionDumper::DumpDefaultValue(LPCVOID pDefaultValue, UINT Size) {
  WriteLn("DefaultValue: ",
          pDefaultValue ? "<present>" : "<nullptr>"); // TODO: Dump DefaultValue
}
void D3DReflectionDumper::DumpShaderVersion(UINT Version) {
  const char *szType = "<unknown>";
  UINT Type = D3D12_SHVER_GET_TYPE(Version);
  switch (Type) {
  case (UINT)hlsl::DXIL::ShaderKind::Pixel:
    szType = "Pixel";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Vertex:
    szType = "Vertex";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Geometry:
    szType = "Geometry";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Hull:
    szType = "Hull";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Domain:
    szType = "Domain";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Compute:
    szType = "Compute";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Library:
    szType = "Library";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::RayGeneration:
    szType = "RayGeneration";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Intersection:
    szType = "Intersection";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::AnyHit:
    szType = "AnyHit";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::ClosestHit:
    szType = "ClosestHit";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Miss:
    szType = "Miss";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Callable:
    szType = "Callable";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Mesh:
    szType = "Mesh";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Amplification:
    szType = "Amplification";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Node:
    szType = "Node";
    break;
  case (UINT)hlsl::DXIL::ShaderKind::Invalid:
    szType = "Invalid";
    break;
  }
  UINT Major = D3D12_SHVER_GET_MAJOR(Version);
  UINT Minor = D3D12_SHVER_GET_MINOR(Version);
  WriteLn("Shader Version: ", szType, " ", Major, ".", Minor);
}

void D3DReflectionDumper::Dump(D3D12_SHADER_TYPE_DESC &tyDesc) {
  SetLastName(tyDesc.Name);
  WriteLn("D3D12_SHADER_TYPE_DESC: Name: ", m_LastName);
  Indent();
  DumpEnum("Class", tyDesc.Class);
  DumpEnum("Type", tyDesc.Type);
  WriteLn("Elements: ", tyDesc.Elements);
  WriteLn("Rows: ", tyDesc.Rows);
  WriteLn("Columns: ", tyDesc.Columns);
  WriteLn("Members: ", tyDesc.Members);
  WriteLn("Offset: ", tyDesc.Offset);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_VARIABLE_DESC &varDesc) {
  SetLastName(varDesc.Name);
  WriteLn("D3D12_SHADER_VARIABLE_DESC: Name: ", m_LastName);
  Indent();
  WriteLn("Size: ", varDesc.Size);
  WriteLn("StartOffset: ", varDesc.StartOffset);
  WriteLn("uFlags: ", FlagsValue<D3D_SHADER_VARIABLE_FLAGS>(varDesc.uFlags));
  DumpDefaultValue(varDesc.DefaultValue, varDesc.Size);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_BUFFER_DESC &Desc) {
  SetLastName(Desc.Name);
  WriteLn("D3D12_SHADER_BUFFER_DESC: Name: ", m_LastName);
  Indent();
  DumpEnum("Type", Desc.Type);
  WriteLn("Size: ", Desc.Size);
  WriteLn("uFlags: ", FlagsValue<D3D_SHADER_CBUFFER_FLAGS>(Desc.uFlags));
  WriteLn("Num Variables: ", Desc.Variables);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  SetLastName(resDesc.Name);
  WriteLn("D3D12_SHADER_INPUT_BIND_DESC: Name: ", m_LastName);
  Indent();
  DumpEnum("Type", resDesc.Type);
  WriteLn("uID: ", resDesc.uID);
  WriteLn("BindCount: ", resDesc.BindCount);
  WriteLn("BindPoint: ", resDesc.BindPoint);
  WriteLn("Space: ", resDesc.Space);
  DumpEnum("ReturnType", resDesc.ReturnType);
  DumpEnum("Dimension", resDesc.Dimension);
  WriteLn("NumSamples (or stride): ", resDesc.NumSamples);
  WriteLn("uFlags: ", FlagsValue<D3D_SHADER_INPUT_FLAGS>(resDesc.uFlags));
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SIGNATURE_PARAMETER_DESC &elDesc) {
  WriteLn("D3D12_SIGNATURE_PARAMETER_DESC: SemanticName: ", elDesc.SemanticName,
          " SemanticIndex: ", elDesc.SemanticIndex);
  Indent();
  WriteLn("Register: ", elDesc.Register);
  DumpEnum("SystemValueType", elDesc.SystemValueType);
  DumpEnum("ComponentType", elDesc.ComponentType);
  WriteLn("Mask: ", CompMaskToString(elDesc.Mask), " (", (UINT)elDesc.Mask,
          ")");
  WriteLn("ReadWriteMask: ", CompMaskToString(elDesc.ReadWriteMask), " (",
          (UINT)elDesc.ReadWriteMask, ") (AlwaysReads/NeverWrites)");
  WriteLn("Stream: ", elDesc.Stream);
  DumpEnum("MinPrecision", elDesc.MinPrecision);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_NODE_SHADER_DESC &Desc) {
  WriteLn("D3D12_NODE_SHADER_DESC:");
  Indent();
  Dump(Desc.ComputeDesc);
  DumpEnum("LaunchType", Desc.LaunchType);
  WriteLn("IsProgramEntry: ", Desc.IsProgramEntry ? "TRUE" : "FALSE");
  WriteLn("LocalRootArgumentsTableIndex: ", std::dec,
          Desc.LocalRootArgumentsTableIndex);
  WriteLn("DispatchGrid[0]: ", std::dec, Desc.DispatchGrid[0]);
  WriteLn("DispatchGrid[1]: ", std::dec, Desc.DispatchGrid[1]);
  WriteLn("DispatchGrid[2]: ", std::dec, Desc.DispatchGrid[2]);
  WriteLn("MaxDispatchGrid[0]: ", std::dec, Desc.MaxDispatchGrid[0]);
  WriteLn("MaxDispatchGrid[1]: ", std::dec, Desc.MaxDispatchGrid[1]);
  WriteLn("MaxDispatchGrid[2]: ", std::dec, Desc.MaxDispatchGrid[2]);
  WriteLn("MaxRecursionDepth: ", std::dec, Desc.MaxRecursionDepth);
  Dump(Desc.ShaderId, "ShaderId");
  Dump(Desc.ShaderId, "ShaderSharedInput");
  WriteLn("InputNodes: ", std::dec, Desc.InputNodes);
  WriteLn("OutputNodes: ", std::dec, Desc.OutputNodes);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_HULL_SHADER_DESC &Desc) {
  WriteLn("D3D12_HULL_SHADER_DESC:");
  Indent();
  DumpEnum("Domain", Desc.Domain);
  DumpEnum("Partition", Desc.Partition);
  DumpEnum("OutputPrimitive", Desc.OutputPrimitive);
  WriteLn("InputControlPoints: ", std::dec, Desc.InputControlPoints);
  WriteLn("OutputControlPoints: ", std::dec, Desc.OutputControlPoints);
  WriteLn("MaxTessFactor: ", Desc.MaxTessFactor);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_COMPUTE_SHADER_DESC &Desc) {
  WriteLn("D3D12_COMPUTE_SHADER_DESC:");
  Indent();
  if (Desc.WaveSizeMin | Desc.WaveSizeMax | Desc.WaveSizePreferred)
    WriteLn("WaveSize: min: ", std::dec, Desc.WaveSizeMin,
            ", max: ", Desc.WaveSizeMax,
            ", preferred: ", Desc.WaveSizePreferred);
  WriteLn("NumThreads: ", std::dec, Desc.NumThreads[0], ", ",
          Desc.NumThreads[1], ", ", Desc.NumThreads[2]);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_MESH_SHADER_DESC &Desc) {
  WriteLn("D3D12_MESH_SHADER_DESC:");
  Indent();
  WriteLn("PayloadSize: ", std::dec, Desc.PayloadSize);
  WriteLn("MaxVertexCount: ", std::dec, Desc.MaxVertexCount);
  WriteLn("MaxPrimitiveCount: ", std::dec, Desc.MaxPrimitiveCount);
  WriteLn("OutputTopology: ",
          Desc.OutputTopology == D3D12_MESH_OUTPUT_TOPOLOGY_LINE ? "Line"
                                                                 : "Triangle");
  WriteLn("NumThreads: ", std::dec, Desc.NumThreads[0], ", ",
          Desc.NumThreads[1], ", ", Desc.NumThreads[2]);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_GEOMETRY_SHADER_DESC &Desc) {
  WriteLn("D3D12_GEOMETRY_SHADER_DESC:");
  Indent();
  DumpEnum("InputPrimitive", Desc.InputPrimitive);
  WriteLn("MaxVertexCount: ", std::dec, Desc.MaxVertexCount);
  WriteLn("InstanceCount: ", std::dec, Desc.InstanceCount);
  DumpEnum("StreamPrimitiveTopologies[0]", Desc.StreamPrimitiveTopologies[0]);
  DumpEnum("StreamPrimitiveTopologies[1]", Desc.StreamPrimitiveTopologies[1]);
  DumpEnum("StreamPrimitiveTopologies[2]", Desc.StreamPrimitiveTopologies[2]);
  DumpEnum("StreamPrimitiveTopologies[3]", Desc.StreamPrimitiveTopologies[3]);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_DOMAIN_SHADER_DESC &Desc) {
  WriteLn("D3D12_DOMAIN_SHADER_DESC:");
  Indent();
  DumpEnum("Domain", Desc.Domain);
  WriteLn("InputControlPoints: ", std::dec, Desc.InputControlPoints);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_NODE_ID_DESC &Desc, const char *name) {
  WriteLn("D3D12_NODE_ID_DESC: (", name, ")");
  Indent();
  WriteLn("Name: ", Desc.Name);
  WriteLn("ID: ", std::dec, Desc.ID);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_NODE_DESC &Desc) {
  WriteLn("D3D12_NODE_DESC:");
  Indent();
  WriteLn("Flags: ", std::hex, std::showbase, Desc.Flags);

  WriteLn("Type:");
  Indent();
  WriteLn("Size: ", std::dec, Desc.Type.Size);
  WriteLn("Alignment: ", std::dec, Desc.Type.Alignment);

  WriteLn("DispatchGrid:");
  Indent();
  WriteLn("ByteOffset: ", std::dec, Desc.Type.DispatchGrid.ByteOffset);
  DumpEnum("ComponentType", Desc.Type.DispatchGrid.ComponentType);
  WriteLn("NumComponents: ", std::dec, Desc.Type.DispatchGrid.NumComponents);
  Dedent();

  Dedent();

  if (Desc.Flags & D3D12_NODE_IO_FLAGS_OUTPUT)
    Dump(Desc.OutputID, "OutputID");

  WriteLn("MaxRecords: ", std::dec, Desc.MaxRecords);
  WriteLn("MaxRecordsSharedWith: ", std::dec, Desc.MaxRecordsSharedWith);
  WriteLn("OutputArraySize: ", std::dec, Desc.OutputArraySize);
  WriteLn("AllowSparseNodes: ", Desc.AllowSparseNodes ? "TRUE" : "FALSE");
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_DESC &Desc) {
  WriteLn("D3D12_SHADER_DESC:");
  Indent();
  DumpShaderVersion(Desc.Version);
  WriteLn("Creator: ", Desc.Creator ? Desc.Creator : "<nullptr>");
  WriteLn("Flags: ", std::hex, std::showbase,
          Desc.Flags); // TODO: fxc compiler flags
  WriteLn("ConstantBuffers: ", Desc.ConstantBuffers);
  WriteLn("BoundResources: ", Desc.BoundResources);
  WriteLn("InputParameters: ", Desc.InputParameters);
  WriteLn("OutputParameters: ", Desc.OutputParameters);
  hlsl::DXIL::ShaderKind ShaderKind =
      (hlsl::DXIL::ShaderKind)D3D12_SHVER_GET_TYPE(Desc.Version);
  if (ShaderKind == hlsl::DXIL::ShaderKind::Geometry) {
    WriteLn("cGSInstanceCount: ", Desc.cGSInstanceCount);
    WriteLn("GSMaxOutputVertexCount: ", Desc.GSMaxOutputVertexCount);
    DumpEnum("GSOutputTopology", Desc.GSOutputTopology);
    DumpEnum("InputPrimitive", Desc.InputPrimitive);
  }
  if (ShaderKind == hlsl::DXIL::ShaderKind::Hull) {
    WriteLn("PatchConstantParameters: ", Desc.PatchConstantParameters);
    WriteLn("cControlPoints: ", Desc.cControlPoints);
    DumpEnum("InputPrimitive", Desc.InputPrimitive);
    DumpEnum("HSOutputPrimitive", Desc.HSOutputPrimitive);
    DumpEnum("HSPartitioning", Desc.HSPartitioning);
    DumpEnum("TessellatorDomain", Desc.TessellatorDomain);
  }
  if (ShaderKind == hlsl::DXIL::ShaderKind::Domain) {
    WriteLn("PatchConstantParameters: ", Desc.PatchConstantParameters);
    WriteLn("cControlPoints: ", Desc.cControlPoints);
    DumpEnum("TessellatorDomain", Desc.TessellatorDomain);
  }
  if (ShaderKind == hlsl::DXIL::ShaderKind::Mesh) {
    WriteLn("PatchConstantParameters: ", Desc.PatchConstantParameters,
            " (output primitive parameters for mesh shader)");
  }
  // Instruction Counts
  WriteLn("InstructionCount: ", Desc.InstructionCount);
  WriteLn("TempArrayCount: ", Desc.TempArrayCount);
  WriteLn("DynamicFlowControlCount: ", Desc.DynamicFlowControlCount);
  WriteLn("ArrayInstructionCount: ", Desc.ArrayInstructionCount);
  WriteLn("TextureNormalInstructions: ", Desc.TextureNormalInstructions);
  WriteLn("TextureLoadInstructions: ", Desc.TextureLoadInstructions);
  WriteLn("TextureCompInstructions: ", Desc.TextureCompInstructions);
  WriteLn("TextureBiasInstructions: ", Desc.TextureBiasInstructions);
  WriteLn("TextureGradientInstructions: ", Desc.TextureGradientInstructions);
  WriteLn("FloatInstructionCount: ", Desc.FloatInstructionCount);
  WriteLn("IntInstructionCount: ", Desc.IntInstructionCount);
  WriteLn("UintInstructionCount: ", Desc.UintInstructionCount);
  WriteLn("CutInstructionCount: ", Desc.CutInstructionCount);
  WriteLn("EmitInstructionCount: ", Desc.EmitInstructionCount);
  WriteLn("cBarrierInstructions: ", Desc.cBarrierInstructions);
  WriteLn("cInterlockedInstructions: ", Desc.cInterlockedInstructions);
  WriteLn("cTextureStoreInstructions: ", Desc.cTextureStoreInstructions);

  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_FUNCTION_DESC &Desc) {
  SetLastName(Desc.Name);
  WriteLn("D3D12_FUNCTION_DESC: Name: ", EscapedString(m_LastName));
  Indent();
  DumpShaderVersion(Desc.Version);
  WriteLn("Creator: ", Desc.Creator ? Desc.Creator : "<nullptr>");
  WriteLn("Flags: ", std::hex, std::showbase, Desc.Flags);
  WriteLn("RequiredFeatureFlags: ", std::hex, std::showbase,
          Desc.RequiredFeatureFlags);
  WriteLn("ConstantBuffers: ", Desc.ConstantBuffers);
  WriteLn("BoundResources: ", Desc.BoundResources);
  WriteLn("FunctionParameterCount: ", Desc.FunctionParameterCount);
  WriteLn("HasReturn: ", Desc.HasReturn ? "TRUE" : "FALSE");
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_FUNCTION_DESC1 &Desc) {
  WriteLn("D3D12_FUNCTION_DESC1: ");
  Indent();
  WriteLn("RootSignatureSize: ", std::dec, Desc.RootSignatureSize);
  switch (Desc.ShaderType) {
  case D3D12_SHVER_NODE_SHADER:
    Dump(Desc.NodeShader);
    break;
  case D3D12_SHVER_HULL_SHADER:
    Dump(Desc.HullShader);
    break;
  case D3D12_SHVER_COMPUTE_SHADER:
    Dump(Desc.ComputeShader);
    break;
  case D3D12_SHVER_MESH_SHADER:
    Dump(Desc.MeshShader);
    break;
  case D3D12_SHVER_GEOMETRY_SHADER:
    Dump(Desc.GeometryShader);
    break;
  case D3D12_SHVER_DOMAIN_SHADER:
    Dump(Desc.DomainShader);
    break;
  case D3D12_SHVER_AMPLIFICATION_SHADER:
    WriteLn("PayloadSize: ", std::dec, Desc.AmplificationShader.PayloadSize);
    WriteLn("NumThreads: ", std::dec, Desc.AmplificationShader.NumThreads[0],
            ", ", Desc.AmplificationShader.NumThreads[1], ", ",
            Desc.AmplificationShader.NumThreads[2]);
    break;
  case D3D12_SHVER_PIXEL_SHADER:
    WriteLn("EarlyDepthStencil: ",
            Desc.PixelShader.EarlyDepthStencil ? "TRUE" : "FALSE");
    break;
  case D3D12_SHVER_CALLABLE_SHADER:
  case D3D12_SHVER_INTERSECTION_SHADER:
  case D3D12_SHVER_ANY_HIT_SHADER:
  case D3D12_SHVER_CLOSEST_HIT_SHADER:
    WriteLn("AttributeSize: ", std::dec, Desc.RaytracingShader.AttributeSize);
    [[fallthrough]];
  case D3D12_SHVER_MISS_SHADER:
    WriteLn("ParamPayloadSize: ", std::dec,
            Desc.RaytracingShader.ParamPayloadSize);
    break;
  }
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_LIBRARY_DESC &Desc) {
  WriteLn("D3D12_LIBRARY_DESC:");
  Indent();
  WriteLn("Creator: ", Desc.Creator ? Desc.Creator : "<nullptr>");
  WriteLn("Flags: ", std::hex, std::showbase, Desc.Flags);
  WriteLn("FunctionCount: ", Desc.FunctionCount);
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12ShaderReflectionType *pType) {
  WriteLn("ID3D12ShaderReflectionType:");
  Indent();
  D3D12_SHADER_TYPE_DESC tyDesc;
  if (!pType || FAILED(pType->GetDesc(&tyDesc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(tyDesc);
  if (tyDesc.Members) {
    WriteLn("{");
    Indent();
    for (UINT uMember = 0; uMember < tyDesc.Members; uMember++) {
      Dump(pType->GetMemberTypeByIndex(uMember));
    }
    Dedent();
    WriteLn("}");
  }
  Dedent();
}
void D3DReflectionDumper::Dump(ID3D12ShaderReflectionVariable *pVar) {
  WriteLn("ID3D12ShaderReflectionVariable:");
  Indent();
  D3D12_SHADER_VARIABLE_DESC varDesc;
  if (!pVar || FAILED(pVar->GetDesc(&varDesc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(varDesc);
  Dump(pVar->GetType());
  ID3D12ShaderReflectionConstantBuffer *pCB = pVar->GetBuffer();
  D3D12_SHADER_BUFFER_DESC CBDesc;
  if (pCB && SUCCEEDED(pCB->GetDesc(&CBDesc))) {
    WriteLn("CBuffer: ", CBDesc.Name);
  }
  Dedent();
}

void D3DReflectionDumper::Dump(
    ID3D12ShaderReflectionConstantBuffer *pCBReflection) {
  WriteLn("ID3D12ShaderReflectionConstantBuffer:");
  Indent();
  D3D12_SHADER_BUFFER_DESC Desc;
  if (!pCBReflection || FAILED(pCBReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.Variables) {
    WriteLn("{");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uVar = 0; uVar < Desc.Variables; uVar++) {
      if (m_bCheckByName)
        SetLastName();
      ID3D12ShaderReflectionVariable *pVar =
          pCBReflection->GetVariableByIndex(uVar);
      Dump(pVar);
      if (m_bCheckByName) {
        if (pCBReflection->GetVariableByName(m_LastName) != pVar) {
          bCheckByNameFailed = true;
          Failure("GetVariableByName ", m_LastName);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetVariableByName checks succeeded.");
    }
    Dedent();
    WriteLn("}");
  }
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12ShaderReflection *pShaderReflection) {
  WriteLn("ID3D12ShaderReflection:");
  Indent();
  D3D12_SHADER_DESC Desc;
  if (!pShaderReflection || FAILED(pShaderReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);

  if (Desc.InputParameters) {
    WriteLn("InputParameter Elements: ", Desc.InputParameters);
    Indent();
    for (UINT i = 0; i < Desc.InputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC elDesc;
      if (FAILED(pShaderReflection->GetInputParameterDesc(i, &elDesc))) {
        Failure("GetInputParameterDesc ", i);
        continue;
      }
      Dump(elDesc);
    }
    Dedent();
  }
  if (Desc.OutputParameters) {
    WriteLn("OutputParameter Elements: ", Desc.OutputParameters);
    Indent();
    for (UINT i = 0; i < Desc.OutputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC elDesc;
      if (FAILED(pShaderReflection->GetOutputParameterDesc(i, &elDesc))) {
        Failure("GetOutputParameterDesc ", i);
        continue;
      }
      Dump(elDesc);
    }
    Dedent();
  }
  if (Desc.PatchConstantParameters) {
    WriteLn("PatchConstantParameter Elements: ", Desc.PatchConstantParameters);
    Indent();
    for (UINT i = 0; i < Desc.PatchConstantParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC elDesc;
      if (FAILED(
              pShaderReflection->GetPatchConstantParameterDesc(i, &elDesc))) {
        Failure("GetPatchConstantParameterDesc ", i);
        continue;
      }
      Dump(elDesc);
    }
    Dedent();
  }

  if (Desc.ConstantBuffers) {
    WriteLn("Constant Buffers:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uCB = 0; uCB < Desc.ConstantBuffers; uCB++) {
      ID3D12ShaderReflectionConstantBuffer *pCB =
          pShaderReflection->GetConstantBufferByIndex(uCB);
      if (!pCB) {
        Failure("GetConstantBufferByIndex ", uCB);
        continue;
      }
      Dump(pCB);
      if (m_bCheckByName && m_LastName) {
        if (pShaderReflection->GetConstantBufferByName(m_LastName) != pCB) {
          bCheckByNameFailed = true;
          Failure("GetConstantBufferByName ", m_LastName);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetConstantBufferByName checks succeeded.");
    }
    Dedent();
  }
  if (Desc.BoundResources) {
    WriteLn("Bound Resources:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uRes = 0; uRes < Desc.BoundResources; uRes++) {
      D3D12_SHADER_INPUT_BIND_DESC bindDesc;
      if (FAILED(pShaderReflection->GetResourceBindingDesc(uRes, &bindDesc))) {
        Failure("GetResourceBindingDesc ", uRes);
        continue;
      }
      Dump(bindDesc);
      if (m_bCheckByName && bindDesc.Name) {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc2;
        if (FAILED(pShaderReflection->GetResourceBindingDescByName(
                bindDesc.Name, &bindDesc2)) ||
            bindDesc2.Name != bindDesc.Name) {
          bCheckByNameFailed = true;
          Failure("GetResourceBindingDescByName ", bindDesc.Name);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetResourceBindingDescByName checks succeeded.");
    }
    Dedent();
  }
  // TODO
  Dedent();
}
void D3DReflectionDumper::Dump(ID3D12ShaderReflection1 *pShaderReflection) {

  ID3D12ShaderReflection *shaderRefl0 = nullptr;
  if (FAILED(pShaderReflection->QueryInterface(IID_PPV_ARGS(&shaderRefl0)))) {
    Failure("QueryInterface ID3D12ShaderReflection");
    return;
  }

  Dump(shaderRefl0);
  shaderRefl0->Release();

  UINT waveSizePreferred = 0, waveSizeMin = 0, waveSizeMax = 0;

  if (!pShaderReflection->GetWaveSize(&waveSizePreferred, &waveSizeMin,
                                      &waveSizeMax))
    return;

  WriteLn("ID3D12ShaderReflection1:");
  Indent();
  WriteLn("WaveSizePreferred: ", waveSizePreferred);
  WriteLn("WaveSizeMin: ", waveSizeMin);
  WriteLn("WaveSizeMax: ", waveSizeMax);
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12FunctionReflection *pFunctionReflection) {
  WriteLn("ID3D12FunctionReflection:");
  Indent();
  D3D12_FUNCTION_DESC Desc;
  if (!pFunctionReflection || FAILED(pFunctionReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.ConstantBuffers) {
    WriteLn("Constant Buffers:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uCB = 0; uCB < Desc.ConstantBuffers; uCB++) {
      ID3D12ShaderReflectionConstantBuffer *pCB =
          pFunctionReflection->GetConstantBufferByIndex(uCB);
      Dump(pCB);
      if (m_bCheckByName && m_LastName) {
        if (pFunctionReflection->GetConstantBufferByName(m_LastName) != pCB) {
          bCheckByNameFailed = true;
          Failure("GetConstantBufferByName ", m_LastName);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetConstantBufferByName checks succeeded.");
    }
    Dedent();
  }
  if (Desc.BoundResources) {
    WriteLn("Bound Resources:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uRes = 0; uRes < Desc.BoundResources; uRes++) {
      D3D12_SHADER_INPUT_BIND_DESC bindDesc;
      if (FAILED(
              pFunctionReflection->GetResourceBindingDesc(uRes, &bindDesc))) {
      }
      Dump(bindDesc);
      if (m_bCheckByName && bindDesc.Name) {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc2;
        if (FAILED(pFunctionReflection->GetResourceBindingDescByName(
                bindDesc.Name, &bindDesc2)) ||
            bindDesc2.Name != bindDesc.Name) {
          bCheckByNameFailed = true;
          Failure("GetResourceBindingDescByName ", bindDesc.Name);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetResourceBindingDescByName checks succeeded.");
    }
    Dedent();
  }
  // TODO
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12FunctionReflection1 *pFunctionReflection) {
  WriteLn("ID3D12FunctionReflection1:");
  Indent();
  D3D12_FUNCTION_DESC1 Desc;
  if (!pFunctionReflection || FAILED(pFunctionReflection->GetDesc1(&Desc))) {
    Failure("GetDesc1");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.ShaderType == D3D12_SHVER_NODE_SHADER &&
      Desc.NodeShader.InputNodes) {
    WriteLn("Input Nodes:");
    Indent();
    for (UINT i = 0; i < Desc.NodeShader.InputNodes; i++) {
      D3D12_NODE_DESC pND{};
      if (FAILED(pFunctionReflection->GetInputNode(i, &pND)))
        Failure("GetInputNode", m_LastName);
      else
        Dump(pND);
    }
    Dedent();
  }
  if (Desc.ShaderType == D3D12_SHVER_NODE_SHADER &&
      Desc.NodeShader.OutputNodes) {
    WriteLn("Output Nodes:");
    Indent();
    for (UINT i = 0; i < Desc.NodeShader.OutputNodes; i++) {
      D3D12_NODE_DESC pND{};
      if (FAILED(pFunctionReflection->GetOutputNode(i, &pND)))
        Failure("GetOutputNode", m_LastName);
      else
        Dump(pND);
    }
    Dedent();
  }
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12LibraryReflection *pLibraryReflection) {
  WriteLn("ID3D12LibraryReflection:");
  Indent();
  D3D12_LIBRARY_DESC Desc;
  if (!pLibraryReflection || FAILED(pLibraryReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.FunctionCount) {
    for (UINT uFunc = 0; uFunc < Desc.FunctionCount; uFunc++)
      Dump(pLibraryReflection->GetFunctionByIndex((INT)uFunc));
  }
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12LibraryReflection1 *pLibraryReflection) {
  WriteLn("ID3D12LibraryReflection1:");
  Indent();
  D3D12_LIBRARY_DESC Desc;
  if (!pLibraryReflection || FAILED(pLibraryReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.FunctionCount) {
    for (UINT uFunc = 0; uFunc < Desc.FunctionCount; uFunc++) {
      Dump(pLibraryReflection->GetFunctionByIndex((INT)uFunc));
      Dump(pLibraryReflection->GetFunctionByIndex1((INT)uFunc));
    }
  }
  Dedent();
}

} // namespace dump
} // namespace hlsl

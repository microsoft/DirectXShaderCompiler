// RUN: %dxc -auto-binding-space 13 -T lib_6_3 -exports HSMain1;HSMain2;HSMain3 %s | %D3DReflect %s | FileCheck %s

// This version of HSPerPatchFunc1 should not be exported
// CHECK:  ID3D12LibraryReflection1:
// CHECK:    D3D12_LIBRARY_DESC:
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      FunctionCount: 8
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: \01?HSMain1@@YAXIV?$InputPatch@UPSSceneIn@@$02@@@Z
// CHECK:        Shader Version: Library 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        EarlyDepthStencil: FALSE
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: \01?HSMain2@@YAXIV?$InputPatch@UPSSceneIn@@$03@@@Z
// CHECK:        Shader Version: Library 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        EarlyDepthStencil: FALSE
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: \01?HSMain3@@YAXIV?$InputPatch@UPSSceneIn@@$02@@@Z
// CHECK:        Shader Version: Library 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        EarlyDepthStencil: FALSE
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: \01?HSPerPatchFunc1@@YA?AUHSPerPatchData@@XZ
// CHECK:        Shader Version: Library 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        EarlyDepthStencil: FALSE
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: \01?HSPerPatchFunc2@@YA?AUHSPerPatchDataQuad@@V?$InputPatch@UPSSceneIn@@$03@@@Z
// CHECK:        Shader Version: Library 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        EarlyDepthStencil: FALSE
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: HSMain1
// CHECK:        Shader Version: Hull 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        D3D12_HULL_SHADER_DESC:
// CHECK:          Domain: D3D_TESSELLATOR_DOMAIN_TRI
// CHECK:          Partition: D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD
// CHECK:          OutputPrimitive: D3D_TESSELLATOR_OUTPUT_TRIANGLE_CW
// CHECK:          InputControlPoints: 3
// CHECK:          OutputControlPoints: 3
// CHECK:          MaxTessFactor: 64
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: HSMain2
// CHECK:        Shader Version: Hull 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        D3D12_HULL_SHADER_DESC:
// CHECK:          Domain: D3D_TESSELLATOR_DOMAIN_QUAD
// CHECK:          Partition: D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD
// CHECK:          OutputPrimitive: D3D_TESSELLATOR_OUTPUT_TRIANGLE_CW
// CHECK:          InputControlPoints: 4
// CHECK:          OutputControlPoints: 4
// CHECK:          MaxTessFactor: 64
// CHECK:    ID3D12FunctionReflection:
// CHECK:      D3D12_FUNCTION_DESC: Name: HSMain3
// CHECK:        Shader Version: Hull 6.3
// CHECK:        Creator: <nullptr>
// CHECK:        Flags: 0
// CHECK:        RequiredFeatureFlags: 0
// CHECK:        ConstantBuffers: 0
// CHECK:        BoundResources: 0
// CHECK:        FunctionParameterCount: 0
// CHECK:        HasReturn: FALSE
// CHECK:    ID3D12FunctionReflection1:
// CHECK:      D3D12_FUNCTION_DESC1:
// CHECK:        RootSignatureSize: 0
// CHECK:        D3D12_HULL_SHADER_DESC:
// CHECK:          Domain: D3D_TESSELLATOR_DOMAIN_TRI
// CHECK:          Partition: D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD
// CHECK:          OutputPrimitive: D3D_TESSELLATOR_OUTPUT_TRIANGLE_CCW
// CHECK:          InputControlPoints: 3
// CHECK:          OutputControlPoints: 3
// CHECK:          MaxTessFactor: 64
// CHECK-NOT:ID3D12FunctionReflection:

Buffer<float> T_unused;

struct PSSceneIn
{
  float4 pos  : SV_Position;
  float2 tex  : TEXCOORD0;
  float3 norm : NORMAL;
};

struct HSPerPatchData
{
  float edges[3] : SV_TessFactor;
  float inside   : SV_InsideTessFactor;
};

struct HSPerPatchDataQuad
{
  float edges[4] : SV_TessFactor;
  float inside[2]   : SV_InsideTessFactor;
};

// Should not be selected, since later candidate function with same name exists.
// If selected, it should fail, since patch size mismatches HS function.
HSPerPatchData HSPerPatchFunc1(
  const InputPatch< PSSceneIn, 16 > points)
{
  HSPerPatchData d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.inside = T_unused.Load(1).x;

  return d;
}

HSPerPatchDataQuad HSPerPatchFunc2(
  const InputPatch< PSSceneIn, 4 > points)
{
  HSPerPatchDataQuad d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.edges[3] = -7;
  d.inside[0] = -8;
  d.inside[1] = -8;

  return d;
}


[shader("hull")]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HSPerPatchFunc1")]
[outputcontrolpoints(3)]
void HSMain1( const uint id : SV_OutputControlPointID,
              const InputPatch< PSSceneIn, 3 > points )
{
}

[shader("hull")]
[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HSPerPatchFunc2")]
[outputcontrolpoints(4)]
void HSMain2( const uint id : SV_OutputControlPointID,
              const InputPatch< PSSceneIn, 4 > points )
{
}

[shader("hull")]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[patchconstantfunc("HSPerPatchFunc1")]
[outputcontrolpoints(3)]
void HSMain3( const uint id : SV_OutputControlPointID,
              const InputPatch< PSSceneIn, 3 > points )
{
}

// actual selected HSPerPatchFunc1 for HSMain1 and HSMain3
HSPerPatchData HSPerPatchFunc1()
{
  HSPerPatchData d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.inside = -8;

  return d;
}

// RUN: %dxc -auto-binding-space 13 -T lib_6_3 -exports HSMain1;HSMain3 %s | %D3DReflect %s | FileCheck %s

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

HSPerPatchData HSPerPatchFunc2(
  const InputPatch< PSSceneIn, 4 > points)
{
  HSPerPatchData d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.inside = -8;

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

// CHECK: DxilRuntimeData (size = 436 bytes):
// CHECK:   StringBuffer (size = 176 bytes)
// CHECK:   IndexTable (size = 0 bytes)
// CHECK:   RawBytes (size = 0 bytes)
// CHECK:   RecordTable (stride = 44 bytes) FunctionTable[5] = {
// CHECK:     <0:RuntimeDataFunctionInfo> = {
// CHECK:       Name: "\01?HSMain1{{[@$?.A-Za-z0-9_]+}}"
// CHECK:       UnmangledName: "HSMain1"
// CHECK:       Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:       FunctionDependencies: <string[0]> = {}
// CHECK:       ShaderKind: Library
// CHECK:       PayloadSizeInBytes: 0
// CHECK:       AttributeSizeInBytes: 0
// CHECK:       FeatureInfo1: 0
// CHECK:       FeatureInfo2: 0
// CHECK:       ShaderStageFlag: 32767
// CHECK:       MinShaderTarget: 393312
// CHECK:     }
// CHECK:     <1:RuntimeDataFunctionInfo> = {
// CHECK:       Name: "\01?HSMain3{{[@$?.A-Za-z0-9_]+}}"
// CHECK:       UnmangledName: "HSMain3"
// CHECK:       Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:       FunctionDependencies: <string[0]> = {}
// CHECK:       ShaderKind: Library
// CHECK:       PayloadSizeInBytes: 0
// CHECK:       AttributeSizeInBytes: 0
// CHECK:       FeatureInfo1: 0
// CHECK:       FeatureInfo2: 0
// CHECK:       ShaderStageFlag: 32767
// CHECK:       MinShaderTarget: 393312
// CHECK:     }
// CHECK:     <2:RuntimeDataFunctionInfo> = {
// CHECK:       Name: "HSMain1"
// CHECK:       UnmangledName: "HSMain1"
// CHECK:       Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:       FunctionDependencies: <string[0]> = {}
// CHECK:       ShaderKind: Hull
// CHECK:       PayloadSizeInBytes: 0
// CHECK:       AttributeSizeInBytes: 0
// CHECK:       FeatureInfo1: 0
// CHECK:       FeatureInfo2: 0
// CHECK:       ShaderStageFlag: 8
// CHECK:       MinShaderTarget: 196704
// CHECK:     }
// CHECK:     <3:RuntimeDataFunctionInfo> = {
// CHECK:       Name: "HSMain3"
// CHECK:       UnmangledName: "HSMain3"
// CHECK:       Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:       FunctionDependencies: <string[0]> = {}
// CHECK:       ShaderKind: Hull
// CHECK:       PayloadSizeInBytes: 0
// CHECK:       AttributeSizeInBytes: 0
// CHECK:       FeatureInfo1: 0
// CHECK:       FeatureInfo2: 0
// CHECK:       ShaderStageFlag: 8
// CHECK:       MinShaderTarget: 196704
// CHECK:     }
// CHECK:     <4:RuntimeDataFunctionInfo> = {
// CHECK:       Name: "\01?HSPerPatchFunc1{{[@$?.A-Za-z0-9_]+}}"
// CHECK:       UnmangledName: "HSPerPatchFunc1"
// CHECK:       Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:       FunctionDependencies: <string[0]> = {}
// CHECK:       ShaderKind: Library
// CHECK:       PayloadSizeInBytes: 0
// CHECK:       AttributeSizeInBytes: 0
// CHECK:       FeatureInfo1: 0
// CHECK:       FeatureInfo2: 0
// CHECK:       ShaderStageFlag: 8
// CHECK:       MinShaderTarget: 393312
// CHECK:     }
// CHECK:   }

// CHECK: ID3D12LibraryReflection:
// CHECK:   D3D12_LIBRARY_DESC:
// CHECK:     FunctionCount: 5
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?HSMain1{{[@$?.A-Za-z0-9_]+}}
// CHECK:       Shader Version: Library 6.3
// CHECK:       BoundResources: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK-NOT:     D3D12_FUNCTION_DESC: Name: \01?HSMain2{{[@$?.A-Za-z0-9_]+}}
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?HSMain3{{[@$?.A-Za-z0-9_]+}}
// CHECK:       Shader Version: Library 6.3
// CHECK:       BoundResources: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?HSPerPatchFunc1{{[@$?.A-Za-z0-9_]+}}
// CHECK:       Shader Version: Library 6.3
// CHECK:       BoundResources: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK-NOT:     D3D12_FUNCTION_DESC: Name: \01?HSPerPatchFunc2{{[@$?.A-Za-z0-9_]+}}
// CHECK:     D3D12_FUNCTION_DESC: Name: HSMain1
// CHECK:       Shader Version: Hull 6.3
// CHECK:       BoundResources: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK-NOT:     D3D12_FUNCTION_DESC: Name: HSMain2
// CHECK:     D3D12_FUNCTION_DESC: Name: HSMain3
// CHECK:       Shader Version: Hull 6.3
// CHECK:       BoundResources: 0

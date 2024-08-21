// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates shader stage of entry function
// These must be minimal shaders since intrinsic usage associated with the
// shader stage will cause the min target to be set that way.

// This covers raytracing entry points, which should always be SM 6.3+

// RDAT: FunctionTable[{{.*}}] = {

RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "raygen"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (RayGeneration)
// RDAT18: MinShaderTarget: 0x70063
// Old: 6.0
// RDAT17: MinShaderTarget: 0x70060

[shader("raygeneration")]
void raygen() {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "intersection"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Intersection)
// RDAT18: MinShaderTarget: 0x80063
// Old: 6.0
// RDAT17: MinShaderTarget: 0x80060

[shader("intersection")]
void intersection() {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "anyhit"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (AnyHit)
// RDAT18: MinShaderTarget: 0x90063
// Old: 6.0
// RDAT17: MinShaderTarget: 0x90060

struct [raypayload] MyPayload {
  float2 loc : write(caller) : read(caller);
};

[shader("anyhit")]
void anyhit(inout MyPayload payload : SV_RayPayload,
    in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes ) {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "closesthit"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (ClosestHit)
// RDAT18: MinShaderTarget: 0xa0063
// Old: 6.0
// RDAT17: MinShaderTarget: 0xa0060

[shader("closesthit")]
void closesthit(inout MyPayload payload : SV_RayPayload,
    in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes ) {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "miss"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Miss)
// RDAT18: MinShaderTarget: 0xb0063
// Old: 6.0
// RDAT17: MinShaderTarget: 0xb0060

[shader("miss")]
void miss(inout MyPayload payload : SV_RayPayload) {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "callable"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Callable)
// RDAT18: MinShaderTarget: 0xc0063
// Old: 6.0
// RDAT17: MinShaderTarget: 0xc0060

[shader("callable")]
void callable(inout MyPayload param) {
  BAB.Store(0, 0);
}

// RDAT: ID3D12LibraryReflection1:
// RDAT:   D3D12_LIBRARY_DESC:
// RDAT:     Creator: <nullptr>
// RDAT:     Flags: 0
// RDAT:     FunctionCount: 6
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: \01?anyhit@@YAXUMyPayload@@UBuiltInTriangleIntersectionAttributes@@@Z
// RDAT:       Shader Version: AnyHit 6.8
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       AttributeSize: 8
// RDAT:       ParamPayloadSize: 8
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: \01?callable@@YAXUMyPayload@@@Z
// RDAT:       Shader Version: Callable 6.8
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       AttributeSize: 0
// RDAT:       ParamPayloadSize: 8
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: \01?closesthit@@YAXUMyPayload@@UBuiltInTriangleIntersectionAttributes@@@Z
// RDAT:       Shader Version: ClosestHit 6.8
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       AttributeSize: 8
// RDAT:       ParamPayloadSize: 8
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: \01?intersection@@YAXXZ
// RDAT:       Shader Version: Intersection 6.8
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       AttributeSize: 0
// RDAT:       ParamPayloadSize: 0
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: \01?miss@@YAXUMyPayload@@@Z
// RDAT:       Shader Version: Miss 6.8
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       ParamPayloadSize: 8
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: \01?raygen@@YAXXZ
// RDAT:       Shader Version: RayGeneration 6.8
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
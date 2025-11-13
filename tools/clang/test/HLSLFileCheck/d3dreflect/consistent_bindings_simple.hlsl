// RUN: %dxc -T lib_6_3 -auto-binding-space 0 -fhlsl-unused-resource-bindings=reserve-all %s | %D3DReflect %s | FileCheck %s -check-prefix=CHECK
// RUN: %dxc -T cs_6_3 -E mainA -auto-binding-space 0 -fhlsl-unused-resource-bindings=reserve-all %s | %D3DReflect %s | FileCheck %s -check-prefix=CHECK2
// RUN: %dxc -T cs_6_3 -E mainB -auto-binding-space 0 -fhlsl-unused-resource-bindings=reserve-all %s | %D3DReflect %s | FileCheck %s -check-prefix=CHECK3

RWByteAddressBuffer a;
RWByteAddressBuffer b;

[shader("compute")]
[numthreads(16, 16, 1)]
void mainA(uint id : SV_DispatchThreadID) {
  a.Store(0, float4(id, 0.xxx));
}

[shader("compute")]
[numthreads(16, 16, 1)]
void mainB(uint id : SV_DispatchThreadID) {
  b.Store(0, float4(id, 0.xxx));
}

// CHECK: ID3D12LibraryReflection:
// CHECK:   D3D12_LIBRARY_DESC:
// CHECK:     FunctionCount: 2
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: mainA
// CHECK:       Shader Version: Compute 6.3
// CHECK:       BoundResources: 1
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: a
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: mainB
// CHECK:       Shader Version: Compute 6.3
// CHECK:       BoundResources: 1
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: b
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 1
// CHECK:         Space: 0

// CHECK2: ID3D12ShaderReflection:
// CHECK2:   D3D12_SHADER_DESC:
// CHECK2:     Shader Version: Compute
// CHECK2:   Bound Resources:
// CHECK2:     D3D12_SHADER_INPUT_BIND_DESC: Name: a
// CHECK2:       Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK2:       BindPoint: 0
// CHECK2:       Space: 0

// CHECK3: ID3D12ShaderReflection:
// CHECK3:   D3D12_SHADER_DESC:
// CHECK3:     Shader Version: Compute
// CHECK3:   Bound Resources:
// CHECK3:     D3D12_SHADER_INPUT_BIND_DESC: Name: b
// CHECK3:       Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK3:       BindPoint: 1
// CHECK3:       Space: 0
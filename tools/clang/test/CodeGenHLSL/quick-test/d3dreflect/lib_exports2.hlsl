// RUN: %dxc -auto-binding-space 13 -exports VSMain;VS_RENAMED=\01?VSMain@@YA?AV?$vector@M$03@@V?$vector@H$02@@@Z;RayGen1,RayGen2=RayGen -T lib_6_3 %s | %D3DReflect %s | FileCheck %s

Buffer<int> T0;

Texture2D<float4> T1;

struct Foo { uint u; float f; };
StructuredBuffer<Foo> T2;

RWByteAddressBuffer U0;

[shader("vertex")]
float4 VSMain(int3 coord : COORD) : SV_Position {
  return T1.Load(coord);
}

[shader("pixel")]
float4 PSMain(int idx : INDEX) : SV_Target {
  return T2[T0.Load(idx)].f;
}

[shader("raygeneration")]
void RayGen() {
  uint2 dim = DispatchRaysDimensions();
  uint2 idx = DispatchRaysIndex();
  U0.Store(idx.y * dim.x * 4 + idx.x * 4, idx.x ^ idx.y);
}


// CHECK: ID3D12LibraryReflection:
// CHECK:   D3D12_LIBRARY_DESC:
// CHECK:     FunctionCount: 4
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?RayGen1@@YAXXZ
// CHECK:       Shader Version: RayGeneration 6.3
// CHECK:       BoundResources: 1
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_BUFFER_DESC: Name: U0
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         uID: 0
// CHECK:         BindCount: 1
// CHECK:         BindPoint: 0
// CHECK:         Space: 13
// CHECK:         ReturnType: D3D_RETURN_TYPE_MIXED
// CHECK:         Dimension: D3D_SRV_DIMENSION_BUFFER
// CHECK:         NumSamples (or stride): 0
// CHECK:         uFlags: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?RayGen2@@YAXXZ
// CHECK:       Shader Version: RayGeneration 6.3
// CHECK:       BoundResources: 1
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_BUFFER_DESC: Name: U0
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         uID: 0
// CHECK:         BindCount: 1
// CHECK:         BindPoint: 0
// CHECK:         Space: 13
// CHECK:         ReturnType: D3D_RETURN_TYPE_MIXED
// CHECK:         Dimension: D3D_SRV_DIMENSION_BUFFER
// CHECK:         NumSamples (or stride): 0
// CHECK:         uFlags: 0
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?VS_RENAMED@@YA?AV?$vector@M$03@@V?$vector@H$02@@@Z
// CHECK:       Shader Version: Library 6.3
// CHECK:       BoundResources: 1
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_BUFFER_DESC: Name: T1
// CHECK:         Type: D3D_SIT_TEXTURE
// CHECK:         uID: 0
// CHECK:         BindCount: 1
// CHECK:         BindPoint: 0
// CHECK:         Space: 13
// CHECK:         ReturnType: D3D_RETURN_TYPE_FLOAT
// CHECK:         Dimension: D3D_SRV_DIMENSION_TEXTURE2D
// CHECK:         NumSamples (or stride): 4294967295
// CHECK:         uFlags: 0xc
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: VSMain
// CHECK:       Shader Version: Vertex 6.3
// CHECK:       BoundResources: 1
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_BUFFER_DESC: Name: T1
// CHECK:         Type: D3D_SIT_TEXTURE
// CHECK:         uID: 0
// CHECK:         BindCount: 1
// CHECK:         BindPoint: 0
// CHECK:         Space: 13
// CHECK:         ReturnType: D3D_RETURN_TYPE_FLOAT
// CHECK:         Dimension: D3D_SRV_DIMENSION_TEXTURE2D
// CHECK:         NumSamples (or stride): 4294967295
// CHECK:         uFlags: 0xc

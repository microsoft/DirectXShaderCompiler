// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external -keep-all-resources %s | %D3DReflect %s | FileCheck %s

struct Foo
{
    float4 a;
    uint b;
};

RWStructuredBuffer<Foo> g_buffer[2] : register(u0);

uint Dummy() {
  return 0;
}

// Buffer is maintained only as a resource, but not per entrypoint

// CHECK:    <1:RuntimeDataResourceInfo> = {
// CHECK:      Class: UAV
// CHECK:      Kind: StructuredBuffer
// CHECK:      ID: 0
// CHECK:      Space: 0
// CHECK:      LowerBound: 0
// CHECK:      UpperBound: 1
// CHECK:      Name: "g_buffer"
// CHECK:      Flags: 0 (None)
// CHECK:    }
    
// CHECK: ID3D12LibraryReflection:
// CHECK:   D3D12_LIBRARY_DESC:
// CHECK:     FunctionCount: 1
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: \01?Dummy{{[@$?.A-Za-z0-9_]+}}
// CHECK:       Shader Version: Library 6.3
// CHECK:       BoundResources: 0

// RUN: %dxc -E main -T ps_6_0 -HV 2021 -Vd -validator-version 0.0 %s | %D3DReflect %s | FileCheck %s

// Make sure bitfiled info is saved.
// TODO - make better dump. Now, Rows = 0 means bitfield. Columns is bit width. Elements is bit offset.

// CHECK: ID3D12ShaderReflectionType:
// CHECK:   D3D12_SHADER_TYPE_DESC: Name: BF
// CHECK:     Class: D3D_SVC_STRUCT
// CHECK:     Type: D3D_SVT_VOID
// CHECK:     Elements: 0
// CHECK:     Rows: 1
// CHECK:     Columns: 1
// CHECK:     Members: 4
// CHECK:     Offset: 0
// CHECK:   {
// CHECK:     ID3D12ShaderReflectionType:
// CHECK:       D3D12_SHADER_TYPE_DESC: Name: int
// CHECK:         Class: D3D_SVC_SCALAR
// CHECK:         Type: D3D_SVT_INT
// CHECK:         Elements: 0
// CHECK:         Rows: 0
// CHECK:         Columns: 8
// CHECK:         Members: 0
// CHECK:         Offset: 0
// CHECK:     ID3D12ShaderReflectionType:
// CHECK:       D3D12_SHADER_TYPE_DESC: Name: int
// CHECK:         Class: D3D_SVC_SCALAR
// CHECK:         Type: D3D_SVT_INT
// CHECK:         Elements: 8
// CHECK:         Rows: 0
// CHECK:         Columns: 8
// CHECK:         Members: 0
// CHECK:         Offset: 0
// CHECK:     ID3D12ShaderReflectionType:
// CHECK:       D3D12_SHADER_TYPE_DESC: Name: int
// CHECK:         Class: D3D_SVC_SCALAR
// CHECK:         Type: D3D_SVT_INT
// CHECK:         Elements: 16
// CHECK:         Rows: 0
// CHECK:         Columns: 8
// CHECK:         Members: 0
// CHECK:         Offset: 0
// CHECK:     ID3D12ShaderReflectionType:
// CHECK:       D3D12_SHADER_TYPE_DESC: Name: int
// CHECK:         Class: D3D_SVC_SCALAR
// CHECK:         Type: D3D_SVT_INT
// CHECK:         Elements: 24
// CHECK:         Rows: 0
// CHECK:         Columns: 8
// CHECK:         Members: 0
// CHECK:         Offset: 0
// CHECK:   }
// CHECK: CBuffer: B

struct BF {
   int i0 : 8;
   int i1 : 8;
   int i2 : 8;
   int i3 : 8;
};

StructuredBuffer<BF> B;


float4 main() : SV_Target {
  return float4(B[0].i0, B[0].i1, B[0].i2, B[0].i3);
}

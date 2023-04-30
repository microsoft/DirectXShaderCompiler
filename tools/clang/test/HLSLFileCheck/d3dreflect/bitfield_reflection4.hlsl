// RUN: %dxc -E main -T ps_6_0 -HV 2021 -Vd -validator-version 0.0 %s | %D3DReflect %s | FileCheck %s

// Make sure bitfiled info is saved.
// FIXME: check half as 16bit when enable-16bit-types.

// CHECK: D3D12_SHADER_TYPE_DESC: Name: BF
// CHECK-NEXT:               Class: D3D_SVC_STRUCT
// CHECK-NEXT:               Type: D3D_SVT_VOID
// CHECK-NEXT:               Elements: 0
// CHECK-NEXT:               Rows: 1
// CHECK-NEXT:               Columns: 3
// CHECK-NEXT:               Members: 3
// CHECK-NEXT:               Offset: 0
// CHECK-NEXT:             {
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 0
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 1
// CHECK-NEXT:                  Offset: 4
// CHECK-NEXT:                {
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                      Class: D3D_SVC_BIT_FIELD
// CHECK-NEXT:                      Type: D3D_SVT_INT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 8
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 0
// CHECK-NEXT:                }
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 8
// CHECK-NEXT:            }
// CHECK-NEXT: CBuffer: B

struct BF {
   float f0;
   int   i0 : 8;
   half  h0;
};

StructuredBuffer<BF> B;


float main() : SV_Target {
  return B[0].i0;
}

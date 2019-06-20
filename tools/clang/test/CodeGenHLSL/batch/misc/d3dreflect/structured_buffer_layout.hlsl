// RUN: %dxc -E main -T vs_6_0 %s | %D3DReflect %s | FileCheck %s

// Verify SB type description does not follow the CB offseting alignment
// even when structure is shared with a ConstantBuffer.

#if 0
// CHECK:       Constant Buffers:
// CHECK-NEXT:    ID3D12ShaderReflectionConstantBuffer:
// CHECK-NEXT:      D3D12_SHADER_BUFFER_DESC: Name: CB
// CHECK-NEXT:        Type: D3D_CT_CBUFFER
// CHECK-NEXT:        Size: 32
// CHECK-NEXT:        uFlags: 0
// CHECK-NEXT:        Num Variables: 1
// CHECK-NEXT:      {
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: CB
// CHECK-NEXT:            Size: 28
// CHECK-NEXT:            StartOffset: 0
// CHECK-NEXT:            uFlags: 0x2
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S1
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 5
// CHECK-NEXT:              Members: 3
// CHECK-NEXT:              Offset: 0
// CHECK-NEXT:            {
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_INT
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
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 4
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float3
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 16
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: CB
// CHECK-NEXT:      }
// CHECK-NEXT:    ID3D12ShaderReflectionConstantBuffer:
// CHECK-NEXT:      D3D12_SHADER_BUFFER_DESC: Name: SB
// CHECK-NEXT:        Type: D3D_CT_RESOURCE_BIND_INFO
// CHECK-NEXT:        Size: 20
// CHECK-NEXT:        uFlags: 0
// CHECK-NEXT:        Num Variables: 1
// CHECK-NEXT:      {
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: $Element
// CHECK-NEXT:            Size: 20
// CHECK-NEXT:            StartOffset: 0
// CHECK-NEXT:            uFlags: 0x2
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S1
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 5
// CHECK-NEXT:              Members: 3
// CHECK-NEXT:              Offset: 0
// CHECK-NEXT:            {
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_INT
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
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 4
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float3
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 8
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: SB
// CHECK-NEXT:      }

#endif

struct S1 {
  int i;
  int j;
  float3 c;
};

StructuredBuffer<S1> SB;
ConstantBuffer<S1> CB;

float3 main() : OUT {
  return SB[CB.i].c;
}

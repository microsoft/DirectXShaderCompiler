// RUN: %dxc -E main -T vs_6_0 %s | %D3DReflect %s | FileCheck %s

// Verify SB type description does not follow the CB offseting alignment
// even when structure is shared with a ConstantBuffer.

#if 0
// CHECK:       Constant Buffers:
// CHECK-NEXT:    ID3D12ShaderReflectionConstantBuffer:
// CHECK-NEXT:      D3D12_SHADER_BUFFER_DESC: Name: CB
// CHECK-NEXT:        Type: D3D_CT_CBUFFER
// CHECK-NEXT:        Size: 128
// CHECK-NEXT:        uFlags: 0
// CHECK-NEXT:        Num Variables: 1
// CHECK-NEXT:      {
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: CB
// CHECK-NEXT:            Size: 116
// CHECK-NEXT:            StartOffset: 0
// CHECK-NEXT:            uFlags: 0x2
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S1
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 16
// CHECK-NEXT:              Members: 8
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
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: S0
// CHECK-NEXT:                  Class: D3D_SVC_STRUCT
// CHECK-NEXT:                  Type: D3D_SVT_VOID
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 2
// CHECK-NEXT:                  Offset: 16
// CHECK-NEXT:                {
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                      Class: D3D_SVC_VECTOR
// CHECK-NEXT:                      Type: D3D_SVT_INT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 2
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 0
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: float
// CHECK-NEXT:                      Class: D3D_SVC_SCALAR
// CHECK-NEXT:                      Type: D3D_SVT_FLOAT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 1
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 8
// CHECK-NEXT:                }
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 32
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float3
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 48
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 64
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float2x1
// CHECK-NEXT:                  Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 2
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 72
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 80
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float1x2
// CHECK-NEXT:                  Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 96
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: CB
// CHECK-NEXT:      }
// CHECK-NEXT:    ID3D12ShaderReflectionConstantBuffer:
// CHECK-NEXT:      D3D12_SHADER_BUFFER_DESC: Name: CB1
// CHECK-NEXT:        Type: D3D_CT_CBUFFER
// CHECK-NEXT:        Size: 208
// CHECK-NEXT:        uFlags: 0
// CHECK-NEXT:        Num Variables: 7
// CHECK-NEXT:      {
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: s1
// CHECK-NEXT:            Size: 116
// CHECK-NEXT:            StartOffset: 0
// CHECK-NEXT:            uFlags: 0
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S1
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 16
// CHECK-NEXT:              Members: 8
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
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: S0
// CHECK-NEXT:                  Class: D3D_SVC_STRUCT
// CHECK-NEXT:                  Type: D3D_SVT_VOID
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 2
// CHECK-NEXT:                  Offset: 16
// CHECK-NEXT:                {
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                      Class: D3D_SVC_VECTOR
// CHECK-NEXT:                      Type: D3D_SVT_INT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 2
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 0
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: float
// CHECK-NEXT:                      Class: D3D_SVC_SCALAR
// CHECK-NEXT:                      Type: D3D_SVT_FLOAT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 1
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 8
// CHECK-NEXT:                }
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 32
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float3
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 48
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 64
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float2x1
// CHECK-NEXT:                  Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 2
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 72
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 80
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float1x2
// CHECK-NEXT:                  Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 96
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: s2
// CHECK-NEXT:            Size: 4
// CHECK-NEXT:            StartOffset: 128
// CHECK-NEXT:            uFlags: 0
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S2
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 1
// CHECK-NEXT:              Members: 1
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
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: s3
// CHECK-NEXT:            Size: 4
// CHECK-NEXT:            StartOffset: 144
// CHECK-NEXT:            uFlags: 0x2
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S2
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 1
// CHECK-NEXT:              Members: 1
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
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: c
// CHECK-NEXT:            Size: 12
// CHECK-NEXT:            StartOffset: 148
// CHECK-NEXT:            uFlags: 0
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: float3
// CHECK-NEXT:              Class: D3D_SVC_VECTOR
// CHECK-NEXT:              Type: D3D_SVT_FLOAT
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 3
// CHECK-NEXT:              Members: 0
// CHECK-NEXT:              Offset: 0
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: k
// CHECK-NEXT:            Size: 4
// CHECK-NEXT:            StartOffset: 160
// CHECK-NEXT:            uFlags: 0
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:              Class: D3D_SVC_SCALAR
// CHECK-NEXT:              Type: D3D_SVT_INT
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 1
// CHECK-NEXT:              Members: 0
// CHECK-NEXT:              Offset: 0
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: f2x1
// CHECK-NEXT:            Size: 8
// CHECK-NEXT:            StartOffset: 164
// CHECK-NEXT:            uFlags: 0
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: float2x1
// CHECK-NEXT:              Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:              Type: D3D_SVT_FLOAT
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 2
// CHECK-NEXT:              Columns: 1
// CHECK-NEXT:              Members: 0
// CHECK-NEXT:              Offset: 0
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: f1x2
// CHECK-NEXT:            Size: 20
// CHECK-NEXT:            StartOffset: 176
// CHECK-NEXT:            uFlags: 0
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: float1x2
// CHECK-NEXT:              Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:              Type: D3D_SVT_FLOAT
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 2
// CHECK-NEXT:              Members: 0
// CHECK-NEXT:              Offset: 0
// CHECK-NEXT:          CBuffer: CB1
// CHECK-NEXT:      }
// CHECK-NEXT:    ID3D12ShaderReflectionConstantBuffer:
// CHECK-NEXT:      D3D12_SHADER_BUFFER_DESC: Name: SB
// CHECK-NEXT:        Type: D3D_CT_RESOURCE_BIND_INFO
// CHECK-NEXT:        Size: 64
// CHECK-NEXT:        uFlags: 0
// CHECK-NEXT:        Num Variables: 1
// CHECK-NEXT:      {
// CHECK-NEXT:        ID3D12ShaderReflectionVariable:
// CHECK-NEXT:          D3D12_SHADER_VARIABLE_DESC: Name: $Element
// CHECK-NEXT:            Size: 64
// CHECK-NEXT:            StartOffset: 0
// CHECK-NEXT:            uFlags: 0x2
// CHECK-NEXT:            DefaultValue: <nullptr>
// CHECK-NEXT:          ID3D12ShaderReflectionType:
// CHECK-NEXT:            D3D12_SHADER_TYPE_DESC: Name: S1
// CHECK-NEXT:              Class: D3D_SVC_STRUCT
// CHECK-NEXT:              Type: D3D_SVT_VOID
// CHECK-NEXT:              Elements: 0
// CHECK-NEXT:              Rows: 1
// CHECK-NEXT:              Columns: 16
// CHECK-NEXT:              Members: 8
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
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: S0
// CHECK-NEXT:                  Class: D3D_SVC_STRUCT
// CHECK-NEXT:                  Type: D3D_SVT_VOID
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 2
// CHECK-NEXT:                  Offset: 4
// CHECK-NEXT:                {
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                      Class: D3D_SVC_VECTOR
// CHECK-NEXT:                      Type: D3D_SVT_INT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 2
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 0
// CHECK-NEXT:                  ID3D12ShaderReflectionType:
// CHECK-NEXT:                    D3D12_SHADER_TYPE_DESC: Name: float
// CHECK-NEXT:                      Class: D3D_SVC_SCALAR
// CHECK-NEXT:                      Type: D3D_SVT_FLOAT
// CHECK-NEXT:                      Elements: 0
// CHECK-NEXT:                      Rows: 1
// CHECK-NEXT:                      Columns: 1
// CHECK-NEXT:                      Members: 0
// CHECK-NEXT:                      Offset: 8
// CHECK-NEXT:                }
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 16
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float3
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 3
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 24
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int2
// CHECK-NEXT:                  Class: D3D_SVC_VECTOR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 36
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float2x1
// CHECK-NEXT:                  Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 2
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 44
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: int
// CHECK-NEXT:                  Class: D3D_SVC_SCALAR
// CHECK-NEXT:                  Type: D3D_SVT_INT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 1
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 52
// CHECK-NEXT:              ID3D12ShaderReflectionType:
// CHECK-NEXT:                D3D12_SHADER_TYPE_DESC: Name: float1x2
// CHECK-NEXT:                  Class: D3D_SVC_MATRIX_COLUMNS
// CHECK-NEXT:                  Type: D3D_SVT_FLOAT
// CHECK-NEXT:                  Elements: 0
// CHECK-NEXT:                  Rows: 1
// CHECK-NEXT:                  Columns: 2
// CHECK-NEXT:                  Members: 0
// CHECK-NEXT:                  Offset: 56
// CHECK-NEXT:            }
// CHECK-NEXT:          CBuffer: SB
// CHECK-NEXT:      }

#endif

struct S0 {
  int2 i2;    // CB: 0, SB: 0
  float f;    // CB: 8, SB: 8
};

struct S1 {
  int i;          // CB: 0,  SB: 0
  S0 s0;          // CB: 16, SB: 4  (new row for CB)
  int2 j;         // CB: 32, SB: 16 (new row for CB)
  float3 c;       // CB: 48, SB: 24 (new row for CB)
  int2 k;         // CB: 64, SB: 36 (new row for CB)
  float2x1 f2x1;  // CB: 72, SB: 44 pack with previous
  int l;          // CB: 80, SB: 52
  float1x2 f1x2;  // CB: 96, SB: 56 (new rows for CB)
};

StructuredBuffer<S1> SB;
ConstantBuffer<S1> CB;

struct S2 {
  int i;
};

cbuffer CB1 {
  S1 s1;          // CB: 0
  S2 s2;          // CB: 128 (new row for struct)
  S2 s3;          // CB: 144 (new row for struct)
  float3 c;       // CB: 148 (fits into last row)
  int k;          // CB: 160 (new row for CB)
  float2x1 f2x1;  // CB: 164 pack with previous
  float1x2 f1x2;  // CB: 176 (new rows for multi-row matrix in col_major)
}

float3 main() : OUT {
  return SB[CB.s0.i2.x + s3.i].c;
}

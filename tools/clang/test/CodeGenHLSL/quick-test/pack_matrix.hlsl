// RUN: %dxc -E main -T ps_6_0 -ast-dump %s  | FileCheck %s

// CHECK:row_major
#pragma pack_matrix(row_major)

struct Foo
{
  float2x2 a;
};

// CHECK:column_major
#pragma pack_matrix(column_major)

struct Bar {
  float2x2 a;
};

Foo f;
Bar b;

// CHECK:row_major
#pragma pack_matrix(row_major)

float2x2 c;

// CHECK:column_major
#pragma pack_matrix(column_major)
float2x2 d;

// CHECK: main 'float4 ()'
float4 main() : SV_Target
{
  float2x2 e = f.a + b.a + c + d;
  return e;
}
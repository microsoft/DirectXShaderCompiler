// Run: %dxc -T vs_6_0 -E main /Zpr

struct S {
// CHECK: OpMemberDecorate %S 0 ColMajor
               float2x3 mat1[2];
// CHECK: OpMemberDecorate %S 1 ColMajor
  row_major    float2x3 mat2[2];
// CHECK: OpMemberDecorate %S 2 RowMajor
  column_major float2x3 mat3[2];
               float    f;
};

cbuffer MyCBuffer {
// CHECK: OpMemberDecorate %type_MyCBuffer 0 ColMajor
               float2x3 field1;
// CHECK: OpMemberDecorate %type_MyCBuffer 1 ColMajor
  row_major    float2x3 field2;
// CHECK: OpMemberDecorate %type_MyCBuffer 2 RowMajor
  column_major float2x3 field3;
               S        field4;
}

struct T {
               float    f[2]; // Make sure that arrays of non-matrices work
// CHECK: OpMemberDecorate %T 1 ColMajor
               float2x3 mat1;
// CHECK: OpMemberDecorate %T 2 ColMajor
  row_major    float2x3 mat2;
// CHECK: OpMemberDecorate %T 3 RowMajor
  column_major float2x3 mat3;
};

struct U {
               T        t;
// CHECK: OpMemberDecorate %U 1 ColMajor
               float2x3 mat1[2];
// CHECK: OpMemberDecorate %U 2 ColMajor
  row_major    float2x3 mat2[2];
// CHECK: OpMemberDecorate %U 3 RowMajor
  column_major float2x3 mat3[2];
               float    f;
};


RWStructuredBuffer<U> MySBuffer;

float3 main() : A {
  return MySBuffer[0].mat1[1][1];
}
// RUN: %dxc -E main -T cs_6_0 -fcgl %s  | FileCheck %s

// Check unlowered type
// CHECK: %"class.StructuredBuffer<matrix<float, 4, 4> >" = type { %class.matrix.float.4.4 }

StructuredBuffer<matrix> buf1;
// Should be equivalent to:
// StructuredBuffer<matrix<float, 4, 4> > buf1;

RWBuffer<float4> buf2;

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  buf2[tid.x] = buf1[tid.x][tid.y];
}
// RUN: %dxc -E main -T ps_6_1 %s | FileCheck %s

// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 0, i32 0, i8 0, i8 0)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 0, i32 0, i8 1, i8 0)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 0, i32 0, i8 2, i8 0)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 0, i32 0, i8 3, i8 0)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 1, i32 0, i8 0, i8 1)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 1, i32 0, i8 1, i8 1)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 1, i32 0, i8 2, i8 1)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 1, i32 0, i8 3, i8 1)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 2, i32 0, i8 0, i8 2)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 2, i32 0, i8 1, i8 2)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 2, i32 0, i8 2, i8 2)
// CHECK: call float @dx.op.attributeAtVertex.f32(i32 141, i32 2, i32 0, i8 3, i8 2)

float4 main(float4 a : A, float4 b : B, float4 c : C) : SV_Target
{
  float4 a0 = GetAttributeAtVertex(a, 0);
  float4 b1 = GetAttributeAtVertex(b, 1);
  float4 c2 = GetAttributeAtVertex(c, 2);

  return a0 + b1 + c2;
}
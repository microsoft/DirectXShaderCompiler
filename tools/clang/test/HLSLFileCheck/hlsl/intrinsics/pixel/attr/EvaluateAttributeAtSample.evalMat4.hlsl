// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: %[[m10:.+]] = call float @dx.op.evalCentroid.f32(i32 89, i32 0, i32 0, i8 1)
// CHECK: %[[m20:.+]] = call float @dx.op.evalCentroid.f32(i32 89, i32 0, i32 0, i8 2)
// CHECK: %[[m11:.+]] = call float @dx.op.evalCentroid.f32(i32 89, i32 0, i32 1, i8 1)
// CHECK: %[[m21:.+]] = call float @dx.op.evalCentroid.f32(i32 89, i32 0, i32 1, i8 2)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %[[m10]])
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %[[m11]])
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %[[m20]])
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %[[m21]])

float4 main(float4x4 a : A) : SV_Target
{
  float3x2 r = EvaluateAttributeCentroid((float3x2)a);

  return float4(r[1], r[2]);
}
